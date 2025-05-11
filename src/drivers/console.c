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
    uint32_t cursor_x;    // Changed from int to uint32_t
    uint32_t cursor_y;    // Changed from int to uint32_t
    bool cursor_visible;
    char *buffer;
    PSF2_Header *font;
    uint16_t *unicode;
} console;

// Dirty rectangle tracking
static DirtyRect dirty_rect = {0, 0, 0, 0, false};

// VESA variables
extern uint32_t g_width;
extern uint32_t g_height;
extern uint32_t g_pitch;
extern uint32_t *g_vbe_buffer;
extern uint32_t *g_back_buffer; // Added declaration for back buffer

// font data
extern char _binary___config_output_psf_start;

int console_scrolling = 0;

static inline char *buffer_at(uint32_t x, uint32_t y)  // Changed parameters to uint32_t
{
    return &console.buffer[y * console.cols + x];
}
void console_init(uint32_t fg, uint32_t bg)
{
    console.font = (PSF2_Header *)&_binary___config_output_psf_start;
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

void console_mark_dirty(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!dirty_rect.dirty) {
        dirty_rect.x1 = x;
        dirty_rect.y1 = y;
        dirty_rect.x2 = x + width;
        dirty_rect.y2 = y + height;
        dirty_rect.dirty = true;
    } else {
        if (x < dirty_rect.x1) dirty_rect.x1 = x;
        if (y < dirty_rect.y1) dirty_rect.y1 = y;
        if (x + width > dirty_rect.x2) dirty_rect.x2 = x + width;
        if (y + height > dirty_rect.y2) dirty_rect.y2 = y + height;
    }
}

void console_flush(void) {
    if (!dirty_rect.dirty) return;

    // Copy only the dirty rectangle
    uint32_t pitch_pixels = g_pitch / sizeof(uint32_t);
    for (uint32_t y = dirty_rect.y1; y < dirty_rect.y2; y++) {
        uint32_t *src = g_back_buffer + y * pitch_pixels + dirty_rect.x1;
        uint32_t *dst = g_vbe_buffer + y * pitch_pixels + dirty_rect.x1;
        uint32_t width = dirty_rect.x2 - dirty_rect.x1;
        memcpy(dst, src, width * sizeof(uint32_t));
    }

    dirty_rect.dirty = false;
}

void draw_char(int x, int y, char c)
{
    uint32_t screen_x = x * console.font->width;
    uint32_t screen_y = y * console.font->height;
    uint32_t bytes_per_row = (console.font->width + 7) / 8;
    const unsigned char *glyph = (const unsigned char *)console.font + console.font->headersize + (unsigned char)c * console.font->bytesperglyph;

    // Pre-compute row offsets for better cache usage
    uint32_t row_offsets[8];
    for (uint32_t byte = 0; byte < bytes_per_row; byte++) {
        row_offsets[byte] = byte * 8;
    }

    for (uint32_t font_y = 0; font_y < console.font->height; font_y++) {
        for (uint32_t byte = 0; byte < bytes_per_row; byte++) {
            unsigned char font_byte = glyph[font_y * bytes_per_row + byte];
            if (font_byte == 0) continue; // Skip empty bytes

            for (uint32_t bit = 0; bit < 8; bit++) {
                if (!(font_byte & (0x80 >> bit))) continue; // Skip empty pixels
                
                uint32_t font_x = row_offsets[byte] + bit;
                if (font_x >= console.font->width) break;
                
                vbe_putpixel(screen_x + font_x, screen_y + font_y, console.fg);
            }
        }
    }

    console_mark_dirty(screen_x, screen_y, console.font->width, console.font->height);
}

void console_clear(void)
{
    // Clear back buffer instead of front buffer
    for (uint32_t y = 0; y < g_height; y++)
    {
        for (uint32_t x = 0; x < g_width; x++)
        {
            vbe_putpixel(x, y, console.bg); // Uses back buffer now
        }
    }
    memset(console.buffer, 0, console.rows * console.cols);
    console.cursor_x = console.cursor_y = 0;
    console_mark_dirty(0, 0, g_width, g_height);
    console_flush();
}

void console_scroll(int lines)
{
    const uint32_t scroll_pixels = lines * console.font->height;
    const uint32_t pitch_pixels = g_pitch / sizeof(uint32_t);

    // Scroll the back buffer (instead of front buffer)
    memmove(g_back_buffer,
            g_back_buffer + scroll_pixels * pitch_pixels,
            (g_height - scroll_pixels) * pitch_pixels * sizeof(uint32_t));

    // Clear scrolled area in the back buffer
    for (uint32_t y = g_height - scroll_pixels; y < g_height; y++)
    {
        for (uint32_t x = 0; x < g_width; x++)
        {
            vbe_putpixel(x, y, console.bg);
        }
    }
    console_mark_dirty(0, g_height - scroll_pixels, g_width, scroll_pixels);
    vesa_swap_buffers();
}

void console_putchar(char c)
{

    if ((c < 0x20 || c > 0x7E) && c != '\n')
    return;
    
    if (c == '\n')
    {
        console.cursor_x = 0;
        if (++console.cursor_y >= console.rows)
        {
            console_scroll(1);
            console.cursor_y = console.rows - 1;
        }
        console_flush(); // Flush on newline
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

        for (uint32_t y = 0; y < FONT_HEIGHT + 1; y++)
        {
            for (uint32_t x = 0; x < FONT_WIDTH + 1; x++)
            {
                vbe_putpixel(screen_x + x, screen_y + y, console.bg);
            }
        }
        console_mark_dirty(screen_x, screen_y, console.font->width, console.font->height);
    }
    console_flush(); // Update the screen after ungetting a character
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
    console_flush();
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
            console_flush();
            buffer[i] = '\0';
            return;
        }
        else if (c >= 32 && c <= 126)
        { // Printable ASCII only
            buffer[i++] = c;
            console_putchar(c);
            console_flush();
        }

        // Prevent overflow
        if (console.cursor_x >= console.cols)
        {
            console_putchar('\n');
            console_flush();
        }
    }
    buffer[i] = '\0';
}

void getstr_bound(char *buffer, uint8_t bound)
{
    if (!buffer || bound == 0)
        return;
    uint8_t idx = 0;

    // Draw initial state
    console_flush();

    while (idx < bound - 1)
    {
        char c = kb_getchar();

        if (c == '\n')
        {
            console_putchar('\n');
            buffer[idx] = 0;
            console_flush(); // Update screen on enter
            return;
        }

        if (c == '\b' && idx > 0)
        {
            idx--;
            buffer[idx] = 0;
            console_ungetchar();
            console_flush(); // Update screen after backspace
            continue;
        }

        if (c >= 32 && c <= 126)
        { // Printable ASCII only
            buffer[idx++] = c;
            console_putchar(c);
            console_flush(); // Update screen after each character
        }

        if (console.cursor_x >= console.cols)
        {
            console_putchar('\n');
            console_flush(); // Update screen after newline
        }
    }
    buffer[idx] = 0;
    console_flush(); // Final screen update
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
    int n, width, pad_zero;  // Removed unused variable 'i'
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
    console_flush();
}
