#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stddef.h>
#include "vesa.h"

#define PSF2_FONT_MAGIC 0x864ab572

typedef struct
{
    uint32_t magic;         /* magic bytes to identify PSF */
    uint32_t version;       /* zero */
    uint32_t headersize;    /* offset of bitmaps in file, 32 */
    uint32_t flags;         /* 0 if there's no unicode table */
    uint32_t numglyph;      /* number of glyphs */
    uint32_t bytesperglyph; /* size of each glyph */
    uint32_t height;        /* height in pixels */
    uint32_t width;         /* width in pixels */
} PSF2_Header;

// Constants
#define MAXIMUM_PAGES 32
#define SCROLL_UP 1
#define SCROLL_DOWN 2

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define CONSOLE_COLS 128
#define CONSOLE_ROWS 48

#define VESA_COLOR_TYPE uint32_t

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
void draw_char(int x, int y, char c);
void console_clear(void);
void console_init(uint32_t fg, uint32_t bg);
void console_scroll(int lines);
void console_putchar(char ch);
void console_printf(const char *format, ...);
void console_ungetchar(void);
void console_gotoxy(uint32_t x, uint32_t y);
void console_putstr(const char *str);
void console_refresh(void);
void getstr(char *buffer, size_t max_size);
void getstr_bound(char *buffer, uint8_t bound);

#endif