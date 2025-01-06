#ifndef CONSOLE_H
#define CONSOLE_H

#include "vga.h"
#include "types.h"

// Constants
#define MAXIMUM_PAGES  32
#define SCROLL_UP     1
#define SCROLL_DOWN   2

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define CONSOLE_COLS 128
#define CONSOLE_ROWS 48

// Function declarations
void draw_char(int x, int y, char c, uint32 fg, uint32 bg);
void console_clear(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color);
void console_init(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color);
void console_scroll(int line_count);
void console_putchar(char ch);
void console_printf(const char *format, ...);
void console_ungetchar(void);
void console_gotoxy(uint16 x, uint16 y);
void console_putstr(const char *str);
void console_refresh(void);
void getstr(char *buffer);
void getstr_bound(char *buffer, uint8 bound);

#endif