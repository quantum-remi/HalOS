#include "console.h"
#include "string.h"
#include "types.h"
#include "vesa.h"
#include "font.h"
#include "serial.h"
#include "keyboard.h"

// Console state
static struct
{
    uint32 fore_color;
    uint32 back_color;
    int cursor_x;
    int cursor_y;
    char text_buffer[CONSOLE_ROWS][CONSOLE_COLS];
} console;

extern VBE20_MODEINFOBLOCK g_vbe_modeinfoblock;

extern uint32 g_width;
extern uint32 g_height;

int console_scrolling = 0;

void init_resolution()
{
    g_width = g_vbe_modeinfoblock.XResolution;
    g_height = g_vbe_modeinfoblock.YResolution;
}

void console_init(VESA_COLOR_TYPE fore_color, VESA_COLOR_TYPE back_color)
{
    serial_printf("Console: Starting initialization...\n");

    // Clear console state
    memset(&console, 0, sizeof(console));

    // Set colors
    console.fore_color = VBE_RGB(255, 255, 255);
    console.back_color = VBE_RGB(0, 0, 0);

    // Clear screen
    serial_printf("Console: Clearing screen...\n");
    console_clear(fore_color, back_color);

    serial_printf("Console: Initialization complete\n");
}

void draw_char(int x, int y, char c, uint32 fg, uint32 bg)
{

    for (int font_y = 0; font_y < FONT_HEIGHT; font_y++)
    {
        unsigned char font_row = font8x16[(unsigned char)c][font_y];

        for (int font_x = 0; font_x < FONT_WIDTH; font_x++)
        {
            uint32 pixel_x = x * FONT_WIDTH + font_x;
            uint32 pixel_y = y * FONT_HEIGHT + font_y;

            if (pixel_x < g_width && pixel_y < g_height)
            {
                vbe_putpixel(pixel_x, pixel_y,
                             (font_row & (0x80 >> font_x)) ? fg : bg);
            }
        }
    }
}

void console_clear(VESA_COLOR_TYPE fore_color, VESA_COLOR_TYPE back_color)
{
    serial_printf("Console: Clearing screen with fore %u back %u\n", fore_color, back_color);
    for (uint32 y = 0; y < g_height; y++)
    {
        for (uint32 x = 0; x < g_width; x++)
        {
            vbe_putpixel(x, y, back_color);
        }
    }
    serial_printf("Console: Clearing buffer...\n");
    memset(console.text_buffer, 0, sizeof(console.text_buffer));
    console.cursor_x = console.cursor_y = 0;
    serial_printf("Console: Clearing complete\n");
}

void console_scroll(int direction) {
    serial_printf("Console: Scrolling %s...\n", direction == SCROLL_UP ? "up" : "down");

    if (console_scrolling) {
        return;
    }
    console_scrolling = 1;

    if (direction == SCROLL_UP) {
        // Move rows up
        memmove(console.text_buffer[0],
                console.text_buffer[1],
                (CONSOLE_ROWS - 1) * CONSOLE_COLS * sizeof(char));

        // Clear the last row
        memset(console.text_buffer[CONSOLE_ROWS - 1], 0, CONSOLE_COLS * sizeof(char));
    } else if (direction == SCROLL_DOWN) {
        // Move rows down
        memmove(console.text_buffer[1],
                console.text_buffer[0],
                (CONSOLE_ROWS - 1) * CONSOLE_COLS * sizeof(char));

        // Clear the first row
        memset(console.text_buffer[0], 0, CONSOLE_COLS * sizeof(char));
    }

    // Clamp cursor position (if needed)
    if (direction == SCROLL_UP && console.cursor_y > 0) {
        console.cursor_y--;
    } else if (direction == SCROLL_DOWN && console.cursor_y < CONSOLE_ROWS - 1) {
        console.cursor_y++;
    }

    serial_printf("Console: Scrolling complete\n");
    console_scrolling = 0;
    // Refresh the console display
    console_refresh();
}

void console_putchar(char c)
{

    if (console.cursor_x >= CONSOLE_COLS) {
        console.cursor_x = 0;
        console.cursor_y++;
        if (console.cursor_y >= CONSOLE_ROWS) {
            console.cursor_y = CONSOLE_ROWS - 1;
        }
    }

    if (c == '\n') // Handle newline
    {
        console.cursor_x = 0;
        console.cursor_y++;
    }
    else if (c == '\r') // Handle carriage return
    {
        console.cursor_x = 0;
    }
    else
    {
        // Write character to buffer and render it
        if (console.cursor_x < CONSOLE_COLS && console.cursor_y < CONSOLE_ROWS)
        {
            console.text_buffer[console.cursor_y][console.cursor_x] = c;
            draw_char(console.cursor_x, console.cursor_y, c,
                      console.fore_color, console.back_color);
            console.cursor_x++;
        }
    }

    // Move to the next line if end of row is reached
    if (console.cursor_x >= CONSOLE_COLS)
    {
        console.cursor_x = 0;
        console.cursor_y++;
    }

    // Scroll if the cursor moves beyond the last row
    if (console.cursor_y >= CONSOLE_ROWS)
    {
        console_scroll(SCROLL_UP);
        console.cursor_y = CONSOLE_ROWS - 1;
    }
}

void console_ungetchar()
{
    if (console.cursor_x > 0)
    {
        console.cursor_x--;
        console.text_buffer[console.cursor_y][console.cursor_x] = 0;
        draw_char(console.cursor_x, console.cursor_y, ' ',
                  console.fore_color, console.back_color);
    }
}

void console_ungetchar_bound(uint8 n)
{
    while (n-- && console.cursor_x > 0)
    {
        console_ungetchar();
    }
}

void console_gotoxy(uint16 x, uint16 y)
{
    if (x < CONSOLE_COLS && y < CONSOLE_ROWS)
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

void console_refresh()
{
    // serial_printf("Console: Refreshing...\n");
    for (int y = 0; y < CONSOLE_ROWS; y++)
    {
        // serial_printf("Console: Refreshing row %d...\n", y);
        for (int x = 0; x < CONSOLE_COLS; x++)
        {
            char c = console.text_buffer[y][x];

            // Always draw characters, including spaces
            // serial_printf("Console: Drawing character at (%d, %d)...\n", x, y);
            draw_char(x, y, c ? c : ' ', console.fore_color, console.back_color);
        }
    }
    // serial_printf("Console: Refresh complete\n");
    // swap_buffers();
}

void getstr(char *buffer, uint32 max_size)
{
    uint32 i = 0;
    while (i < max_size - 1) // leave space for null terminator
    {
        char c = kb_getchar();
        if (c == '\b') // backspace
        {
            if (i > 0)
            {
                i--;
                console_ungetchar();
            }
        }
        else if (c == '\n') // enter key
        {
            console_putchar('\n'); // print the newline character
            buffer[i] = '\0';      // null terminate the string
            return;
        }
        else
        {
            buffer[i++] = c;
            console_putchar(c);
        }
    }
    buffer[i] = '\0'; // null terminate the string
}

void getstr_bound(char *buffer, uint8 bound)
{
    if (!buffer || bound == 0)
        return;
    uint8 idx = 0;

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

        buffer[idx++] = c;
        console_putchar(c);
    }
    buffer[idx] = 0;
}

#include <stdarg.h>

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
