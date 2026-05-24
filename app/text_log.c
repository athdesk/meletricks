#include "text_log.h"

char text_buf[TEXT_BUF_SIZE];
static u32 s_pos;

static const char HID_TO_ASCII[256] = {
    ['\x04'] = 'a', ['\x05'] = 'b', ['\x06'] = 'c', ['\x07'] = 'd',
    ['\x08'] = 'e', ['\x09'] = 'f', ['\x0A'] = 'g', ['\x0B'] = 'h',
    ['\x0C'] = 'i', ['\x0D'] = 'j', ['\x0E'] = 'k', ['\x0F'] = 'l',
    ['\x10'] = 'm', ['\x11'] = 'n', ['\x12'] = 'o', ['\x13'] = 'p',
    ['\x14'] = 'q', ['\x15'] = 'r', ['\x16'] = 's', ['\x17'] = 't',
    ['\x18'] = 'u', ['\x19'] = 'v', ['\x1A'] = 'w', ['\x1B'] = 'x',
    ['\x1C'] = 'y', ['\x1D'] = 'z',
    ['\x1E'] = '1', ['\x1F'] = '2', ['\x20'] = '3', ['\x21'] = '4',
    ['\x22'] = '5', ['\x23'] = '6', ['\x24'] = '7', ['\x25'] = '8',
    ['\x26'] = '9', ['\x27'] = '0',
    ['\x2C'] = ' ',  ['\x2D'] = '-',  ['\x2E'] = '=', ['\x2F'] = '[',
    ['\x30'] = ']',  ['\x31'] = '\\', ['\x33'] = ';', ['\x34'] = '\'',
    ['\x35'] = '`',  ['\x36'] = ',',  ['\x37'] = '.', ['\x38'] = '/',
};

static void text_append(const char *s)
{
    while (*s) {
        if (s_pos >= TEXT_BUF_SIZE - 1) {
            /* Roll: keep the most recent half, discard the rest. */
            u32 keep = TEXT_BUF_SIZE / 2;
            for (u32 i = 0; i < keep; i++) text_buf[i] = text_buf[s_pos - keep + i];
            s_pos = keep;
        }
        text_buf[s_pos++] = *s++;
    }
    text_buf[s_pos] = 0;
}

void text_log_hid(u8 code)
{
    static const char hex[] = "0123456789ABCDEF";
    char line[16];
    char g = HID_TO_ASCII[code];
    int  n = 0;
    if (g) {
        line[n++] = '\'';
        line[n++] = (g == ' ') ? '_' : g;
        line[n++] = '\'';
        line[n++] = ' ';
    }
    line[n++] = '0'; line[n++] = 'x';
    line[n++] = hex[code >> 4];
    line[n++] = hex[code & 0xF];
    line[n++] = '\n';
    line[n]   = 0;
    text_append(line);
}
