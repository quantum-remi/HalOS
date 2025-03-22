#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#include "console.h"
#include "string.h"
#include "kernel.h"
#include "serial.h"
#include "keyboard.h"
#include "liballoc.h"

// Console state
static struct
{
    uint32_t fg;
    uint32_t bg;
    uint32_t cols;
    uint32_t rows;
    int cursor_x;
    int cursor_y;
    bool cursor_visible;
    char *buffer;
    PSF2_Header *font;
    uint16_t *unicode;
} console;

// VESA variables
extern uint32_t g_width;
extern uint32_t g_height;
extern uint32_t g_pitch;
extern uint32_t *g_vbe_buffer;

// font data
extern char _binary___config_ter_powerline_v20b_psf_start;

int console_scrolling = 0;

static inline char *buffer_at(int x, int y)
{
    return &console.buffer[y * console.cols + x];
}
void console_init(uint32_t fg, uint32_t bg)
{
    console.font = (PSF2_Header *)&_binary___config_ter_powerline_v20b_psf_start;
    if (console.font->magic != PSF2_FONT_MAGIC)
    {
        panic("Invalid font magic number");
    }
    console.cols = g_width / console.font->width;
    console.rows = g_height / console.font->height;

    console.buffer = (char *)malloc(console.rows * console.cols);
    console.cursor_x = 0;
    console.cursor_y = 0;
    console.cursor_visible = true;

    console.fg = fg;
    console.bg = bg;
    console_clear();
}

void draw_char(int x, int y, char c)
{
    uint32_t screen_x = x * console.font->width;
    uint32_t screen_y = y * console.font->height;
    int bytes_per_row = (console.font->width + 7) / 8;
    const unsigned char *glyph = (const unsigned char *)console.font + console.font->headersize + (unsigned char)c * console.font->bytesperglyph;

    for (int font_y = 0; font_y < console.font->height; font_y++)
    {
        for (int byte = 0; byte < bytes_per_row; byte++)
        {
            unsigned char font_byte = glyph[font_y * bytes_per_row + byte];
            for (int bit = 0; bit < 8; bit++)
            {
                int font_x = byte * 8 + bit;
                if (font_x >= console.font->width)
                    break;

                uint32_t color = (font_byte & (0x80 >> bit)) ? console.fg : console.bg;
                vbe_putpixel(screen_x + font_x, screen_y + font_y, color);
            }
        }
    }
}

void console_clear(void)
{
    for (uint32_t y = 0; y < g_height; y++)
    {
        for (uint32_t x = 0; x < g_width; x++)
        {
            vbe_putpixel(x, y, console.bg);
        }
    }
    memset(console.buffer, 0, console.rows * console.cols);
    console.cursor_x = console.cursor_y = 0;
}

void console_scroll(int lines)
{
    // Scroll using memmove with pitch-aware access
    const uint32_t scroll_pixels = lines * console.font->height;
    const uint32_t pitch_pixels = g_pitch / sizeof(uint32_t);
    const size_t scroll_bytes = scroll_pixels * pitch_pixels * sizeof(uint32_t);

    // Scroll framebuffer
    memmove(g_vbe_buffer,
            g_vbe_buffer + scroll_pixels * pitch_pixels,
            (g_height - scroll_pixels) * pitch_pixels * sizeof(uint32_t));

    // Clear scrolled area using putpixel
    for (uint32_t y = g_height - scroll_pixels; y < g_height; y++)
    {
        for (uint32_t x = 0; x < g_width; x++)
        {
            vbe_putpixel(x, y, console.bg);
        }
    }
}

void console_putchar(char c)
{
    if (c == '\n')
    {
        console.cursor_x = 0;
        if (++console.cursor_y >= console.rows)
        {
            console_scroll(1);
            console.cursor_y = console.rows - 1;
        }
        return;
    }

    if (console.cursor_x >= console.cols)
    {
        console_putchar('\n');
    }

    // Store character in buffer
    console.buffer[console.cursor_y * console.cols + console.cursor_x] = c;
    draw_char(console.cursor_x, console.cursor_y, c);
    console.cursor_x++;
}

void console_ungetchar(void)
{
    if (console.cursor_x > 0)
    {
        console.cursor_x--;
        *buffer_at(console.cursor_x, console.cursor_y) = '\0';

        // Clear character from screen
        uint32_t screen_x = console.cursor_x * console.font->width;
        uint32_t screen_y = console.cursor_y * console.font->height;

        for (int y = 0; y < FONT_HEIGHT + 1; y++)
        {
            for (int x = 0; x < FONT_WIDTH + 1; x++)
            {
                vbe_putpixel(screen_x + x, screen_y + y, console.bg);
            }
        }
    }
}

void console_ungetchar_bound(uint8_t n)
{
    while (n-- > 0 && console.cursor_x > 0)
    {
        console_ungetchar();
    }
}

void console_gotoxy(uint32_t x, uint32_t y)
{
    if (x < console.cols && y < console.rows)
    {
        console.cursor_x = x;
        console.cursor_y = y;
    }
}

void console_putstr(const char *str)
{
    while (*str)
    {
        console_putchar(*str++);
    }
}

void console_refresh(void)
{
    uint32_t screen_x = console.cursor_x * console.font->width;
    uint32_t screen_y = console.cursor_y * console.font->height;
    char c = console.buffer[console.cursor_y * console.cols + console.cursor_x];
    bool needs_redraw = false;
    int bytes_per_row = (console.font->width + 7) / 8;
    const unsigned char *glyph = (const unsigned char *)console.font + console.font->headersize + (unsigned char)c * console.font->bytesperglyph;

    for (int fy = 0; fy < console.font->height; fy++)
    {
        for (int byte = 0; byte < bytes_per_row; byte++)
        {
            unsigned char font_byte = glyph[fy * bytes_per_row + byte];
            for (int bit = 0; bit < 8; bit++)
            {
                int fx_pixel = byte * 8 + bit;
                if (fx_pixel >= console.font->width)
                    break;

                uint32_t expected = (font_byte & (0x80 >> bit)) ? console.fg : console.bg;
                uint32_t actual = vbe_getpixel(screen_x + fx_pixel, screen_y + fy);
                if (actual != expected)
                {
                    needs_redraw = true;
                    break;
                }
            }
            if (needs_redraw)
                break;
        }
        if (needs_redraw)
            break;
    }
}

void getstr(char *buffer, uint32_t max_size)
{
    uint32_t i = 0;
    while (i < max_size - 1)
    {
        char c = kb_getchar();
        if (c == '\b')
        {
            if (i > 0)
            {
                i--;
                console_ungetchar();
            }
        }
        else if (c == '\n')
        {
            console_putchar('\n');
            buffer[i] = '\0';
            return;
        }
        else if (c >= 32 && c <= 126)
        { // Printable ASCII only
            buffer[i++] = c;
            console_putchar(c);
        }

        // Prevent overflow
        if (console.cursor_x >= console.cols)
        {
            console_putchar('\n');
        }
    }
    buffer[i] = '\0';
}

void getstr_bound(char *buffer, uint8_t bound)
{
    if (!buffer || bound == 0)
        return;
    uint8_t idx = 0;

    while (idx < bound - 1)
    {
        char c = kb_getchar();

        if (c == '\n')
        {
            console_putchar('\n');
            buffer[idx] = 0;
            return;
        }

        if (c == '\b' && idx > 0)
        {
            idx--;
            buffer[idx] = 0;
            console_ungetchar();
            continue;
        }

        if (c >= 32 && c <= 126)
        { // Printable ASCII only
            buffer[idx++] = c;
            console_putchar(c);
        }

        if (console.cursor_x >= console.cols)
        {
            console_putchar('\n');
        }
    }
    buffer[idx] = 0;
}

static void format_number(char *buf, unsigned int val, int base,
                          int uppercase, int width, int pad_zero)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    // Handle zero explicitly
    if (val == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (val > 0)
        {
            buf[i++] = digits[val % base];
            val /= base;
        }
    }

    // Apply padding
    while (i < width)
    {
        buf[i++] = pad_zero ? '0' : ' ';
    }

    // Reverse digits
    for (int j = 0; j < i / 2; j++)
    {
        char temp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = temp;
    }

    buf[i] = '\0';
}

void console_vprintf(const char *format, va_list args)
{
    char buf[32];
    int i, n, width, pad_zero;
    unsigned int uval;

    while ((n = *format++) != 0)
    {
        if (n != '%')
        {
            console_putchar(n);
            continue;
        }

        // Parse format specifier
        pad_zero = 0;
        width = 0;

        // Check for padding flag
        if (*format == '0')
        {
            pad_zero = 1;
            format++;
        }

        // Parse width
        while (*format >= '0' && *format <= '9')
        {
            width = width * 10 + (*format++ - '0');
        }

        n = *format++;
        switch (n)
        {
        case 'c':
            console_putchar(va_arg(args, int));
            break;
        case 's':
        {
            char *p = va_arg(args, char *);
            if (!p)
                p = "(null)";
            while (*p)
                console_putchar(*p++);
            break;
        }
        case 'd':
        {
            int val = va_arg(args, int);
            format_number(buf, val, 10, 0, width, pad_zero);
            for (char *p = buf; *p; p++)
                console_putchar(*p);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        {
            uval = va_arg(args, unsigned int);
            format_number(buf, uval, 16, (n == 'X'), width, pad_zero);
            for (char *p = buf; *p; p++)
                console_putchar(*p);
            break;
        }
        default:
            console_putchar('%');
            console_putchar(n);
            break;
        }
    }
}

void console_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    console_vprintf(format, args);
    va_end(args);
}
