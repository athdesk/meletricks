/* Append-only log of typed keys for the Text screen: shows the
 * glyph (when printable) and the raw HID scancode. The buffer is
 * a half-roll FIFO so it never grows without bound. */
#ifndef HELLO_TEXT_LOG_H
#define HELLO_TEXT_LOG_H

#include "gfx.h"

#define TEXT_BUF_SIZE  2048u

extern char text_buf[TEXT_BUF_SIZE];

void text_log_hid(u8 hid_code);

#endif
