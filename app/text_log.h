#ifndef TEXT_LOG_H
#define TEXT_LOG_H

#include "gfx.h"

#define TEXT_BUF_SIZE  2048u

extern char text_buf[TEXT_BUF_SIZE];

void text_log_hid(u8 hid_code);

#endif
