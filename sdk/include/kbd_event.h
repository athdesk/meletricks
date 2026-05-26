/*
 * Keyboard event listener.  Hijacks UART0 and parses the keyboard
 * MCU's messages; for now emits two event types on the queue:
 *
 *   - KBD_EV_KEY: opcode 0x3A — key scancode reports.
 *   - KBD_EV_LCD: opcode 0x39 — navigation events.
 *
 * We also handle status events (battery, caps, conn, layer, OS, win-lock)
 *   - kbd_battery_get() / kbd_battery_set_callback()  for power state
 *   - kbd_status_get()  / kbd_status_set_callback()   for everything else
 *
 * Other opcodes go to an optional raw-packet callback. IRQ hook is auto-installed
 * by the SDK on first tick.
 *
 * Event model
 * -----------
 * The wire has no press/release: held keys autorepeat.
 * Release shows up as silence (maybe a `code == 0` "no key" frame).
 * One 0x3A frame -> one KEY event with the scancode + time_ms.
 * One 0x39 frame -> one LCD event with the action code.
 * Consumers that need debounced one-shots diff (type, code) AND time_ms; ones
 * that want continuous-on-hold react to every event.
 *
 * Caveat: installing this module silences the firmware's UART parser,
 * so any side-channel it feeds (e.g. firmware-written globals for
 * LCD command state) stops updating.
 * I haven't found a use case for keeping the firmware path alive, but
 * if there is one, implementing a call to the stolen callback should be straightforward.
 */
#ifndef KBD_EVENT_H
#define KBD_EVENT_H

#ifndef SDK_DISABLE_KBD_EVENTS

#include "fr_types.h"

#ifndef KBD_EVENT_QUEUE_SIZE
#define KBD_EVENT_QUEUE_SIZE 32u
#endif

#ifndef KBD_PKT_LOG_ENTRIES
#define KBD_PKT_LOG_ENTRIES 16u
#endif

#ifndef KBD_PKT_LOG_MAX_PAYLOAD
#define KBD_PKT_LOG_MAX_PAYLOAD 64u
#endif

typedef enum {
    KBD_EV_KEY = 0,    /* code = HID scancode (0 = "no key" wire report) */
    KBD_EV_LCD = 1,    /* code = kbd_lcd_action_t */
} kbd_event_type_t;

typedef enum {
    KBD_LCD_DOWN  = 1,
    KBD_LCD_UP    = 2,
    KBD_LCD_ENTER = 3,
    KBD_LCD_BACK  = 4,
} kbd_lcd_action_t;

typedef struct {
    u8  type;       /* kbd_event_type_t */
    u8  code;       /* HID scancode or kbd_lcd_action_t */
    u32 time_ms;
} kbd_event_t;

typedef struct {
    u32 seq;                                /* monotonic, ++ per write */
    u32 time_ms;
    u8  opcode;
    u8  flags;                              /* bit0 = checksum_ok */
    u16 len;
    u8  payload[KBD_PKT_LOG_MAX_PAYLOAD];
} kbd_pkt_log_entry_t;

/* IRQ-context callback; sees every parsed frame regardless of opcode
 * or checksum.  Use to handle opcodes other than 0x39 / 0x3A. */
typedef void (*kbd_raw_pkt_cb_t)(u8 opcode, u16 len,
                                 const u8 *payload, u8 csum_ok);

void kbd_event_init(void);
void kbd_event_deinit(void);

void kbd_event_set_raw_callback(kbd_raw_pkt_cb_t cb);

/* 1 = event popped into *out, 0 = queue empty.  Main-loop context. */
int kbd_event_pop(kbd_event_t *out);

/* Inject a synthetic event. */
void kbd_event_push(u8 type, u8 code);

typedef enum {
    KBD_CONN_NONE     = 0,
    KBD_CONN_BT_1    = 1,
    KBD_CONN_BT_2    = 2,
    KBD_CONN_BT_3    = 3,
    KBD_CONN_2_4_GHZ  = 4,
    KBD_CONN_WIRED    = 5,
} kbd_conn_type_t;

typedef enum {
    KBD_OS_NONE = 0,
    KBD_OS_MAC  = 1,
    KBD_OS_WIN  = 2,         /* firmware HUD treats any value > 0 && != 1 as Win */
} kbd_os_mode_t;

typedef struct {
    u8  battery_pct;         /* 0..100; 0xFF before first 0x35 packet seen */
    u8  is_charging;         /* 0 or 1; 0xFF before first 0x35 packet seen */
    u32 last_update_ms;      /* fr_millis at last update; 0 if never updated */
} kbd_battery_t;

typedef struct {
    u8  caps_lock_active;    /* 0 or 1; 0xFF before first packet seen */
    u8  connection_type;     /* kbd_conn_type_t */
    u8  kbd_layer;           /* 0 = base; 1..N = active layer (HUD renders N-1 subscript) */
    u8  os_mode;             /* kbd_os_mode_t */
    u8  win_lock;            /* 0 or 1; 0xFF before first packet seen */
    u32 last_update_ms;      /* fr_millis at last update; 0 if never updated */
} kbd_status_t;

/* IRQ-context callbacks; fire only when a tracked field changes. */
typedef void (*kbd_battery_cb_t)(const kbd_battery_t *state);
typedef void (*kbd_status_cb_t) (const kbd_status_t  *state);

void kbd_battery_get(kbd_battery_t *out);
void kbd_battery_set_callback(kbd_battery_cb_t cb);

void kbd_status_get(kbd_status_t *out);
void kbd_status_set_callback(kbd_status_cb_t cb);

#define KBD_TEMP_INVALID  ((s16)0x8000)

typedef struct {
    s16 cpu_temp_c;
    s16 gpu_temp_c;
    s16 unk_field;
    u16 fan_rpm;
    u16 net_speed;
    u32 last_update_ms;
} kbd_sensors_t;

typedef void (*kbd_sensors_cb_t)(const kbd_sensors_t *state);

void kbd_sensors_get(kbd_sensors_t *out);
void kbd_sensors_set_callback(kbd_sensors_cb_t cb);

/* ---- Device badge temp trio (opcodes 0x37 and 0xFE) ----------------------
 *
 * A separate 3-temperature panel rendered next to a host-chosen badge icon.
 * Unlike the PC sensor panel above, the three slots are generic — the host
 * picks the icon (badge_id) and what each slot represents.  Same wire
 * encoding as the PC sensor panel; sentinel 255 (not KBD_TEMP_INVALID) is
 * what the firmware uses internally.
 *
 *   Opcode 0x37: badge_id comes from payload[6]; cmd_byte must be 1.
 *   Opcode 0xFE: badge_id comes from payload[1] (cmd_byte).
 * In both cases temps a/b/c land in the same three SDK fields.
 */

typedef struct {
    u8  badge_id;            /* host-chosen icon id (0 or 0xFF = no badge) */
    s16 temp_a_c;            /* °C, 255 = N/A */
    s16 temp_b_c;
    s16 temp_c_c;
    u32 last_update_ms;
} kbd_badge_temps_t;

typedef void (*kbd_badge_temps_cb_t)(const kbd_badge_temps_t *state);

void kbd_badge_temps_get(kbd_badge_temps_t *out);
void kbd_badge_temps_set_callback(kbd_badge_temps_cb_t cb);

typedef struct {
    u16 year;                /* full year e.g. 2026; 0 if never synced */
    u8  month;               /* 1..12 */
    u8  day;                 /* 1..31 */
    u8  hour;                /* 0..23 */
    u8  minute;              /* 0..59 */
    u8  second;              /* 0..59 */
    u8  synced;              /* 1 once first 0x38 / 0xFB arrives, else 0 */
    u32 last_sync_ms;        /* fr_millis at last successful sync; 0 if never */
} kbd_rtc_t;

typedef void (*kbd_rtc_cb_t)(const kbd_rtc_t *state);

void kbd_rtc_get(kbd_rtc_t *out);
void kbd_rtc_set_callback(kbd_rtc_cb_t cb);

/* Debug ring buffer of every parsed packet. */
extern volatile u32                  g_kbd_pkt_log_widx;
extern volatile u32                  g_kbd_pkt_log_seq;
extern volatile kbd_pkt_log_entry_t  g_kbd_pkt_log[KBD_PKT_LOG_ENTRIES];

#endif /* SDK_DISABLE_KBD_EVENTS */

#endif /* KBD_EVENT_H */
