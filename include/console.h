#ifndef CONSOLE_H
#define CONSOLE_H

#include "types.h"

// Constants
#define MAXIMUM_PAGES  32
#define SCROLL_UP     1
#define SCROLL_DOWN   2

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define CONSOLE_COLS 128
#define CONSOLE_ROWS 48

#define VESA_COLOR_TYPE uint32

#define VESA_COLOR_BLACK 0x00000000
#define VESA_COLOR_BLUE 0x000000FF
#define VESA_COLOR_GREEN 0x0000FF00
#define VESA_COLOR_CYAN 0x0000FFFF
#define VESA_COLOR_RED 0x00FF0000
#define VESA_COLOR_MAGENTA 0x00FF00FF
#define VESA_COLOR_BROWN 0x00FFA500
#define VESA_COLOR_GREY 0x00C0C0C0
#define VESA_COLOR_DARK_GREY 0x00808080
#define VESA_COLOR_BRIGHT_BLUE 0x000000FF
#define VESA_COLOR_BRIGHT_GREEN 0x0000FF00
#define VESA_COLOR_BRIGHT_CYAN 0x0000FFFF
#define VESA_COLOR_BRIGHT_RED 0x00FF0000
#define VESA_COLOR_BRIGHT_MAGENTA 0x00FF00FF
#define VESA_COLOR_YELLOW 0x00FFFF00
#define VESA_COLOR_WHITE 0x00FFFFFF

// Function declarations
void draw_char(int x, int y, char c, uint32 fg, uint32 bg);
void console_clear(VESA_COLOR_TYPE fore_color, VESA_COLOR_TYPE back_color);
void console_init(VESA_COLOR_TYPE fore_color, VESA_COLOR_TYPE back_color);
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