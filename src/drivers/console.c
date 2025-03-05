#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#include "console.h"
#include "string.h"
#include "font.h"
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
} console;


extern uint32_t g_width;
extern uint32_t g_height;
extern uint32_t g_pitch;
extern uint32_t* g_vbe_buffer;

int console_scrolling = 0;

static inline char* buffer_at(int x, int y) {
    return &console.buffer[y * console.cols + x];
}

void console_init(uint32_t fg, uint32_t bg)
{
    console.cols = g_width / FONT_WIDTH;
    console.rows = g_height / FONT_HEIGHT;

    console.buffer = (char *)malloc(console.rows * console.cols);
    console.cursor_x = 0;
    console.cursor_y = 0;
    console.cursor_visible = true;

    console.fg = fg;
    console.bg = bg;
    console_clear();
}

void draw_char(int x, int y, char c) {
    uint32_t screen_x = x * FONT_WIDTH;
    uint32_t screen_y = y * FONT_HEIGHT;
    
    for(int font_y = 0; font_y < FONT_HEIGHT; font_y++) {
        unsigned char font_row = font8x16[(unsigned char)c][font_y];
        
        for(int font_x = 0; font_x < FONT_WIDTH; font_x++) {
            uint32_t color = (font_row & (0x80 >> font_x)) 
                            ? console.fg 
                            : console.bg;
            vbe_putpixel(screen_x + font_x, screen_y + font_y, color);
        }
    }
}

void console_clear(void)
{
    for(uint32_t y = 0; y < g_height; y++) {
        for(uint32_t x = 0; x < g_width; x++) {
            vbe_putpixel(x, y, console.bg);
        }
    }
    memset(console.buffer, 0, console.rows * console.cols);
    console.cursor_x = console.cursor_y = 0;
}

void console_scroll(int lines) {
    // Scroll using memmove with pitch-aware access
    const uint32_t scroll_pixels = lines * FONT_HEIGHT;
    const uint32_t pitch_pixels = g_pitch / sizeof(uint32_t);
    const size_t scroll_bytes = scroll_pixels * pitch_pixels * sizeof(uint32_t);
    
    // Scroll framebuffer
    memmove(g_vbe_buffer,
            g_vbe_buffer + scroll_pixels * pitch_pixels,
            (g_height - scroll_pixels) * pitch_pixels * sizeof(uint32_t));
    
    // Clear scrolled area using putpixel
    for(uint32_t y = g_height - scroll_pixels; y < g_height; y++) {
        for(uint32_t x = 0; x < g_width; x++) {
            vbe_putpixel(x, y, console.bg);
        }
    }
}

void console_putchar(char c) {
    if(c == '\n') {
        console.cursor_x = 0;
        if(++console.cursor_y >= console.rows) {
            console_scroll(1);
            console.cursor_y = console.rows - 1;
        }
        return;
    }

    if(console.cursor_x >= console.cols) {
        console_putchar('\n');
    }

    // Store character in buffer
    console.buffer[console.cursor_y * console.cols + console.cursor_x] = c;
    draw_char(console.cursor_x, console.cursor_y, c);
    console.cursor_x++;
}

void console_ungetchar(void) {
    if (console.cursor_x > 0) {
        console.cursor_x--;
        *buffer_at(console.cursor_x, console.cursor_y) = '\0';
        
        // Clear character from screen
        uint32_t screen_x = console.cursor_x * FONT_WIDTH;
        uint32_t screen_y = console.cursor_y * FONT_HEIGHT;
        
        for(int y = 0; y < FONT_HEIGHT; y++) {
            for(int x = 0; x < FONT_WIDTH; x++) {
                vbe_putpixel(screen_x + x, screen_y + y, console.bg);
            }
        }
    }
}

void console_ungetchar_bound(uint8_t n) {
    while (n-- > 0 && console.cursor_x > 0) {
        console_ungetchar();
    }
}

void console_gotoxy(uint32_t x, uint32_t y) {
    if (x < console.cols && y < console.rows) {
        console.cursor_x = x;
        console.cursor_y = y;
    }
}


void console_putstr(const char *str) {
    while (*str) {
        console_putchar(*str++);
    }
}

void console_refresh(void) {
    for (uint32_t y = 0; y < console.rows; y++) {
        for (uint32_t x = 0; x < console.cols; x++) {
            char c = *buffer_at(x, y);
            if(c == 0) c = ' ';  // Draw spaces for empty cells
            
            // Only redraw if different from current display
            uint32_t screen_x = x * FONT_WIDTH;
            uint32_t screen_y = y * FONT_HEIGHT;
            bool needs_redraw = false;
            
            // Check if current pixels match expected character
            for(int fy = 0; fy < FONT_HEIGHT; fy++) {
                unsigned char font_row = font8x16[(unsigned char)c][fy];
                for(int fx = 0; fx < FONT_WIDTH; fx++) {
                    uint32_t expected = (font_row & (0x80 >> fx)) 
                                      ? console.fg : console.bg;
                    uint32_t actual = vbe_getpixel(screen_x + fx, screen_y + fy);
                    if(actual != expected) {
                        needs_redraw = true;
                        break;
                    }
                }
                if(needs_redraw) break;
            }
            
            if(needs_redraw) {
                draw_char(x, y, c);
            }
        }
    }
}

void getstr(char *buffer, uint32_t max_size) {
    uint32_t i = 0;
    while (i < max_size - 1) {
        char c = kb_getchar();
        if (c == '\b') {
            if (i > 0) {
                i--;
                console_ungetchar();
            }
        }
        else if (c == '\n') {
            console_putchar('\n');
            buffer[i] = '\0';
            return;
        }
        else if (c >= 32 && c <= 126) {  // Printable ASCII only
            buffer[i++] = c;
            console_putchar(c);
        }
        
        // Prevent overflow
        if (console.cursor_x >= console.cols) {
            console_putchar('\n');
        }
    }
    buffer[i] = '\0';
}

void getstr_bound(char *buffer, uint8_t bound) {
    if (!buffer || bound == 0) return;
    uint8_t idx = 0;

    while (idx < bound - 1) {
        char c = kb_getchar();

        if (c == '\n') {
            console_putchar('\n');
            buffer[idx] = 0;
            return;
        }

        if (c == '\b' && idx > 0) {
            idx--;
            buffer[idx] = 0;
            console_ungetchar();
            continue;
        }

        if (c >= 32 && c <= 126) {  // Printable ASCII only
            buffer[idx++] = c;
            console_putchar(c);
        }

        if (console.cursor_x >= console.cols) {
            console_putchar('\n');
        }
    }
    buffer[idx] = 0;
}

void console_vprintf(const char *format, va_list args)
{
    char buf[32];
    int i, n;

    while ((n = *format++) != 0)
    {
        if (n != '%')
        {
            console_putchar(n);
            continue;
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
            }
            break;
        case 'd':
        case 'x':
            {
                char *p;
                int val = va_arg(args, int);
                itoa(buf, n == 'x' ? 16 : 10, val);
                for (p = buf; *p; p++)
                    console_putchar(*p);
            }
            break;
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

