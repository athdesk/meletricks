#include "kbd_event.h"
#include "uart_irq.h"
#include "timer.h"

/* ---- Event queue ---------------------------------------------------------- */

static volatile kbd_event_t s_queue[KBD_EVENT_QUEUE_SIZE];
static volatile u32         s_widx;
static volatile u32         s_ridx;

void kbd_event_push(u8 type, u8 code)
{
    u32 i = s_widx;
    s_queue[i].type    = type;
    s_queue[i].code    = code;
    s_queue[i].time_ms = fr_millis();
    s_widx = (i + 1u) % KBD_EVENT_QUEUE_SIZE;
}

int kbd_event_pop(kbd_event_t *out)
{
    if (s_ridx == s_widx) return 0;
    out->type    = s_queue[s_ridx].type;
    out->code    = s_queue[s_ridx].code;
    out->time_ms = s_queue[s_ridx].time_ms;
    s_ridx = (s_ridx + 1u) % KBD_EVENT_QUEUE_SIZE;
    return 1;
}

/* ---- Raw packet callback -------------------------------------------------- */

static volatile kbd_raw_pkt_cb_t s_raw_cb;

void kbd_event_set_raw_callback(kbd_raw_pkt_cb_t cb)
{
    s_raw_cb = cb;
}

/* ---- UART-derived snapshots ----------------------------------------------- */

static volatile kbd_battery_t      s_battery = { 0xFFu, 0xFFu, 0u };
static volatile kbd_battery_cb_t   s_battery_cb;

static volatile kbd_status_t       s_status  =
    { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0u };
static volatile kbd_status_cb_t    s_status_cb;

static volatile kbd_sensors_t      s_sensors =
    { KBD_TEMP_INVALID, KBD_TEMP_INVALID, KBD_TEMP_INVALID, 0u, 0u, 0u };
static volatile kbd_sensors_cb_t   s_sensors_cb;

static volatile kbd_badge_temps_t  s_badge   = { 0xFFu, 255, 255, 255, 0u };
static volatile kbd_badge_temps_cb_t s_badge_cb;

static volatile kbd_rtc_t          s_rtc     = { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
static volatile kbd_rtc_cb_t       s_rtc_cb;

void kbd_battery_get(kbd_battery_t *out)
{
    out->battery_pct    = s_battery.battery_pct;
    out->is_charging    = s_battery.is_charging;
    out->last_update_ms = s_battery.last_update_ms;
}

void kbd_battery_set_callback(kbd_battery_cb_t cb)
{
    s_battery_cb = cb;
}

void kbd_status_get(kbd_status_t *out)
{
    out->caps_lock_active = s_status.caps_lock_active;
    out->connection_type  = s_status.connection_type;
    out->kbd_layer        = s_status.kbd_layer;
    out->os_mode          = s_status.os_mode;
    out->win_lock         = s_status.win_lock;
    out->last_update_ms   = s_status.last_update_ms;
}

void kbd_status_set_callback(kbd_status_cb_t cb)
{
    s_status_cb = cb;
}

void kbd_sensors_get(kbd_sensors_t *out)
{
    out->cpu_temp_c     = s_sensors.cpu_temp_c;
    out->gpu_temp_c     = s_sensors.gpu_temp_c;
    out->unk_field      = s_sensors.unk_field;
    out->fan_rpm        = s_sensors.fan_rpm;
    out->net_speed      = s_sensors.net_speed;
    out->last_update_ms = s_sensors.last_update_ms;
}

void kbd_sensors_set_callback(kbd_sensors_cb_t cb)
{
    s_sensors_cb = cb;
}

void kbd_badge_temps_get(kbd_badge_temps_t *out)
{
    out->badge_id       = s_badge.badge_id;
    out->temp_a_c       = s_badge.temp_a_c;
    out->temp_b_c       = s_badge.temp_b_c;
    out->temp_c_c       = s_badge.temp_c_c;
    out->last_update_ms = s_badge.last_update_ms;
}

void kbd_badge_temps_set_callback(kbd_badge_temps_cb_t cb)
{
    s_badge_cb = cb;
}

void kbd_rtc_get(kbd_rtc_t *out)
{
    out->year         = s_rtc.year;
    out->month        = s_rtc.month;
    out->day          = s_rtc.day;
    out->hour         = s_rtc.hour;
    out->minute       = s_rtc.minute;
    out->second       = s_rtc.second;
    out->synced       = s_rtc.synced;
    out->last_sync_ms = s_rtc.last_sync_ms;
}

void kbd_rtc_set_callback(kbd_rtc_cb_t cb)
{
    s_rtc_cb = cb;
}

/* ---- Packet log ----------------------------------------------------------- */

volatile u32                  g_kbd_pkt_log_widx;
volatile u32                  g_kbd_pkt_log_seq;
volatile kbd_pkt_log_entry_t  g_kbd_pkt_log[KBD_PKT_LOG_ENTRIES];

static void log_packet(u8 opcode, u16 len, const u8 *payload, u8 csum_ok)
{
    u32 i = g_kbd_pkt_log_widx;
    volatile kbd_pkt_log_entry_t *e = &g_kbd_pkt_log[i];

    /* Bump seq first so a partial-write race is detectable by readers
     * comparing entry seq to the global counter. */
    u32 seq = ++g_kbd_pkt_log_seq;
    e->seq     = seq;
    e->time_ms = fr_millis();
    e->opcode  = opcode;
    e->flags   = csum_ok ? 1u : 0u;
    e->len     = len;

    u32 n = len;
    if (n > KBD_PKT_LOG_MAX_PAYLOAD) n = KBD_PKT_LOG_MAX_PAYLOAD;
    for (u32 j = 0; j < n; j++) e->payload[j] = payload[j];

    g_kbd_pkt_log_widx = (i + 1u) % KBD_PKT_LOG_ENTRIES;
}

/* ---- FSM ------------------------------------------------------------------ */

enum { FSM_SOF = 0, FSM_OP, FSM_LH, FSM_LL, FSM_PL, FSM_CS };

static u8  s_fsm_state;
static u8  s_fsm_opcode;
static u16 s_fsm_len;
static u16 s_fsm_byte_cnt;
static u8  s_fsm_payload[KBD_PKT_LOG_MAX_PAYLOAD];

static void dispatch_scancode_packet(void)
{
    if (s_fsm_len < 3) return;
    /* payload = [0x00, sc_hi, sc_lo]; high byte is zero in practice. */
    kbd_event_push(KBD_EV_KEY, s_fsm_payload[2]);
}

static void dispatch_lcd_packet(void)
{
    /* payload = [0x00, action]; firmware only honours action in 1..4. */
    if (s_fsm_len < 2 || s_fsm_payload[0] != 0) return;
    u8 action = s_fsm_payload[1];
    if (action < KBD_LCD_DOWN || action > KBD_LCD_BACK) return;
    kbd_event_push(KBD_EV_LCD, action);
}

static void dispatch_status_packet(void)
{
    /* Firmware only writes status fields when sub_cmd == 0. */
    if (s_fsm_payload[0] != 0u) return;

    u32 now_ms = fr_millis();

    /* Battery + charging: need payload[14]. */
    if (s_fsm_len >= 15u) {
        u8 pct      = s_fsm_payload[14];
        u8 charging = (s_fsm_payload[13] == 2u) ? 1u : 0u;
        if (pct > 100u) pct = 100u;         /* clamp; wire occasionally jitters */

        u8 changed = (s_battery.battery_pct != pct)
                   | (s_battery.is_charging != charging);
        s_battery.battery_pct    = pct;
        s_battery.is_charging    = charging;
        s_battery.last_update_ms = now_ms;

        if (changed && s_battery_cb)
            s_battery_cb((const kbd_battery_t *)&s_battery);
    }

    /* Caps/conn/layer/OS/win-lock: need payload[21]. */
    if (s_fsm_len >= 22u) {
        u8 caps      = (s_fsm_payload[15] == 2u) ? 1u : 0u;
        u8 conn      = s_fsm_payload[16];
        u8 layer     = s_fsm_payload[17];
        u8 os_mode   = s_fsm_payload[20];
        u8 win_lock  = (s_fsm_payload[21] == 1u) ? 1u : 0u;

        u8 changed = (s_status.caps_lock_active != caps)
                   | (s_status.connection_type  != conn)
                   | (s_status.kbd_layer        != layer)
                   | (s_status.os_mode          != os_mode)
                   | (s_status.win_lock         != win_lock);
        s_status.caps_lock_active = caps;
        s_status.connection_type  = conn;
        s_status.kbd_layer        = layer;
        s_status.os_mode          = os_mode;
        s_status.win_lock         = win_lock;
        s_status.last_update_ms   = now_ms;

        if (changed && s_status_cb)
            s_status_cb((const kbd_status_t *)&s_status);
    }
}

static s16 decode_temp_be(u8 hi, u8 lo)
{
    u16 raw = ((u16)hi << 8) | lo;
    if (raw == 0xFFFFu) return KBD_TEMP_INVALID;
    s16 mag = (s16)((raw & 0x7FFFu) / 10u);
    return (raw & 0x8000u) ? (s16)(-(int)mag) : mag;
}

static u16 decode_be16(u8 hi, u8 lo)
{
    return ((u16)hi << 8) | lo;
}

static void dispatch_pc_sensor_packet(void)
{
    if (s_fsm_len < 11u || s_fsm_payload[0] != 0u) return;

    s16 cpu  = decode_temp_be(s_fsm_payload[1], s_fsm_payload[2]);
    s16 gpu  = decode_temp_be(s_fsm_payload[3], s_fsm_payload[4]);
    s16 unk  = decode_temp_be(s_fsm_payload[5], s_fsm_payload[6]);
    u16 rpm  = decode_be16(s_fsm_payload[7], s_fsm_payload[8]);
    u16 net  = decode_be16(s_fsm_payload[9], s_fsm_payload[10]);

    s_sensors.cpu_temp_c     = cpu;
    s_sensors.gpu_temp_c     = gpu;
    s_sensors.unk_field      = unk;
    s_sensors.fan_rpm        = rpm;
    s_sensors.net_speed      = net;
    s_sensors.last_update_ms = fr_millis();

    if (s_sensors_cb)
        s_sensors_cb((const kbd_sensors_t *)&s_sensors);
}

static void dispatch_badge_temps_packet(u8 opcode)
{
    /* Both 0x37 and 0xFE write to the same firmware struct; payload byte
     * indices for the three temps differ between the two opcodes.
     *   0x37: cmd_byte must == 1.  badge_id @ p[6], a@p[2..3], c@p[9..10], b@p[11..12]
     *   0xFE: badge_id @ p[1] (cmd_byte), a@p[2..3], c@p[4..5], b@p[6..7] */
    if (s_fsm_payload[0] != 0u) return;

    u8  badge;
    s16 a, b, c;
    if (opcode == 0x37u) {
        if (s_fsm_len < 13u || s_fsm_payload[1] != 1u) return;
        badge = s_fsm_payload[6];
        a = decode_temp_be(s_fsm_payload[2],  s_fsm_payload[3]);
        c = decode_temp_be(s_fsm_payload[9],  s_fsm_payload[10]);
        b = decode_temp_be(s_fsm_payload[11], s_fsm_payload[12]);
    } else {  /* 0xFE */
        if (s_fsm_len < 8u) return;
        badge = s_fsm_payload[1];
        a = decode_temp_be(s_fsm_payload[2], s_fsm_payload[3]);
        c = decode_temp_be(s_fsm_payload[4], s_fsm_payload[5]);
        b = decode_temp_be(s_fsm_payload[6], s_fsm_payload[7]);
    }

    s_badge.badge_id       = badge;
    s_badge.temp_a_c       = a;
    s_badge.temp_b_c       = b;
    s_badge.temp_c_c       = c;
    s_badge.last_update_ms = fr_millis();

    if (s_badge_cb)
        s_badge_cb((const kbd_badge_temps_t *)&s_badge);
}

static void dispatch_rtc_packet(void)
{
    /* Opcodes 0x38 and 0xFB; sub_cmd = 0 in both.  Payload after sub_cmd
     * and cmd_byte (so starting at s_fsm_payload[2]) encodes:
     *   [2..3] BE u16 year_raw — firmware computes year = 2000 + ((raw+48)&0x3F)
     *   [4]    month  & 0x0F
     *   [5]    day    & 0x1F
     *   [6]    hour   & 0x1F
     *   [7]    minute & 0x3F
     *   [8]    second & 0x3F
     */
    if (s_fsm_len < 9u || s_fsm_payload[0] != 0u) return;

    u16 year_be  = ((u16)s_fsm_payload[2] << 8) | s_fsm_payload[3];
    u8  year_idx = (u8)((year_be + 48u) & 0x3Fu);

    s_rtc.year         = (u16)(2000u + year_idx);
    s_rtc.month        = s_fsm_payload[4] & 0x0Fu;
    s_rtc.day          = s_fsm_payload[5] & 0x1Fu;
    s_rtc.hour         = s_fsm_payload[6] & 0x1Fu;
    s_rtc.minute       = s_fsm_payload[7] & 0x3Fu;
    s_rtc.second       = s_fsm_payload[8] & 0x3Fu;
    s_rtc.synced       = 1u;
    s_rtc.last_sync_ms = fr_millis();

    if (s_rtc_cb)
        s_rtc_cb((const kbd_rtc_t *)&s_rtc);
}

static void parse_byte(u8 b)
{
    switch (s_fsm_state) {
    case FSM_SOF:
        if (b == 0xA5) s_fsm_state = FSM_OP;
        break;
    case FSM_OP:
        s_fsm_opcode = b;
        s_fsm_state  = FSM_LH;
        break;
    case FSM_LH:
        s_fsm_len   = (u16)b << 8;
        s_fsm_state = FSM_LL;
        break;
    case FSM_LL:
        s_fsm_len     |= b;
        s_fsm_byte_cnt = 0;
        if (s_fsm_len > 0x43)        s_fsm_state = FSM_SOF;    /* invalid, resync */
        else if (s_fsm_len == 0)     s_fsm_state = FSM_CS;
        else                         s_fsm_state = FSM_PL;
        break;
    case FSM_PL:
        if (s_fsm_byte_cnt < KBD_PKT_LOG_MAX_PAYLOAD)
            s_fsm_payload[s_fsm_byte_cnt] = b;
        if (++s_fsm_byte_cnt >= s_fsm_len)
            s_fsm_state = FSM_CS;
        break;
    case FSM_CS: {
        u8 sum = s_fsm_opcode + (u8)(s_fsm_len >> 8) + (u8)(s_fsm_len & 0xFF);
        u16 n = s_fsm_len;
        if (n > KBD_PKT_LOG_MAX_PAYLOAD) n = KBD_PKT_LOG_MAX_PAYLOAD;
        for (u16 j = 0; j < n; j++) sum += s_fsm_payload[j];
        u8 ok = ((u8)~sum == b) || (b == 0xFF);

        log_packet(s_fsm_opcode, s_fsm_len, s_fsm_payload, ok);

        if (ok) {
            switch (s_fsm_opcode) {
            case 0x3A: dispatch_scancode_packet();     break;
            case 0x39: dispatch_lcd_packet();          break;
            case 0x35: dispatch_status_packet();       break;
            case 0xFC: dispatch_pc_sensor_packet();    break;
            case 0x37: dispatch_badge_temps_packet(0x37); break;
            case 0xFE: dispatch_badge_temps_packet(0xFE); break;
            case 0x38:
            case 0xFB: dispatch_rtc_packet();          break;
            default:
                if (s_raw_cb)
                    s_raw_cb(s_fsm_opcode, s_fsm_len, s_fsm_payload, ok);
                break;
            }
        }
        s_fsm_state = FSM_SOF;
        break;
    }
    }
}

/* ---- UART0 ISR ------------------------------------------------------------ */

static void uart_isr(void)
{
    u32 iir = UART0_IIR;

    if ((iir & 4) || ((iir >> 2) & 3) == 3) {
        while (UART0_LSR & 1)
            parse_byte((u8)UART0_THR);
    } else if (((iir >> 1) & 3) == 3) {
        /* Line-status error: read LSR to clear, write IIR back, gate off the
         * RX-line-status enable so the IRQ stops firing. Matches firmware. */
        (void)UART0_LSR;
        UART0_IIR = iir;
        UART0_IER &= ~4u;
    }
}


static irq_handler_t saved_uart_isr;
void kbd_event_init(void)
{
    saved_uart_isr = install_uart_isr(uart_isr);
}

void kbd_event_deinit(void)
{
    install_uart_isr(saved_uart_isr);
}
