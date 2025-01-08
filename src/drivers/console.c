#include "console.h"
#include "string.h"
#include "types.h"
#include "vesa.h"
#include "font.h"
#include "serial.h"
#include "keyboard.h"
#include "serial.h"

// Console state
static struct {
    uint32 fore_color;
    uint32 back_color;
    int cursor_x;
    int cursor_y;
    char text_buffer[CONSOLE_ROWS][CONSOLE_COLS];
} console;

void console_init(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color) {
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

void draw_char(int x, int y, char c, uint32 fg, uint32 bg) {
    if ((unsigned char)c >= 128) return;
    
    for(int font_y = 0; font_y < FONT_HEIGHT; font_y++) {
        unsigned char font_row = font8x16[(unsigned char)c][font_y];
        
        for(int font_x = 0; font_x < FONT_WIDTH; font_x++) {
            int pixel_x = x * FONT_WIDTH + font_x;
            int pixel_y = y * FONT_HEIGHT + font_y;
            
            vbe_putpixel(pixel_x, pixel_y, 
                      (font_row & (0x80 >> font_x)) ? fg : bg);
        }
    }
}

void console_clear(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color) {
    serial_printf("Console: Clearing screen with fore %u back %u...\n", fore_color, back_color);
    for(int y = 0; y < CONSOLE_ROWS * FONT_HEIGHT; y++) {
        for(int x = 0; x < CONSOLE_COLS * FONT_WIDTH; x++) {
            vbe_putpixel(x, y, console.back_color);
        }
    }
    serial_printf("Console: Clearing buffer...\n");
    memset(console.text_buffer, 0, sizeof(console.text_buffer));
    console.cursor_x = console.cursor_y = 0;
    serial_printf("Console: Clearing complete\n");
}

void console_scroll(int direction) {
    if(direction == SCROLL_UP) {
        memmove(&console.text_buffer[0], &console.text_buffer[1], 
               (CONSOLE_ROWS-1) * CONSOLE_COLS);
        memset(&console.text_buffer[CONSOLE_ROWS-1], 0, CONSOLE_COLS);
    } else {
        memmove(&console.text_buffer[1], &console.text_buffer[0], 
               (CONSOLE_ROWS-1) * CONSOLE_COLS);
        memset(&console.text_buffer[0], 0, CONSOLE_COLS);
    }
    console_refresh();
}

void console_putchar(char c) {
    if(c == '\n') {
        console.cursor_x = 0;
        console.cursor_y++;
    } else {
        console.text_buffer[console.cursor_y][console.cursor_x] = c;
        draw_char(console.cursor_x, console.cursor_y, c, 
                 console.fore_color, console.back_color);
        console.cursor_x++;
    }

    if(console.cursor_x >= CONSOLE_COLS) {
        console.cursor_x = 0;
        console.cursor_y++;
    }

    if(console.cursor_y >= CONSOLE_ROWS) {
        console_scroll(SCROLL_UP);
        console.cursor_y = CONSOLE_ROWS-1;
    }
}

void console_ungetchar() {
    if(console.cursor_x > 0) {
        console.cursor_x--;
        console.text_buffer[console.cursor_y][console.cursor_x] = 0;
        draw_char(console.cursor_x, console.cursor_y, ' ', 
                 console.fore_color, console.back_color);
    }
}

void console_ungetchar_bound(uint8 n) {
    while(n-- && console.cursor_x > 0) {
        console_ungetchar();
    }
}

void console_gotoxy(uint16 x, uint16 y) {
    if(x < CONSOLE_COLS && y < CONSOLE_ROWS) {
        console.cursor_x = x;
        console.cursor_y = y;
    }
}

void console_putstr(const char *str) {
    while(*str) {
        console_putchar(*str++);
    }
}

void console_refresh() {
    for(int y = 0; y < CONSOLE_ROWS; y++) {
        for(int x = 0; x < CONSOLE_COLS; x++) {
            char c = console.text_buffer[y][x];
            if(c) {
                draw_char(x, y, c, console.fore_color, console.back_color);
            }
        }
    }
}

void getstr(char *buffer) {
    while(1) {
        char c = kb_getchar();
        if(c == '\n') {
            console_putchar('\n');
            *buffer = 0;
            return;
        }
        *buffer++ = c;
        console_putchar(c);
    }
}

void getstr_bound(char *buffer, uint8 bound) {
    if (!buffer) return;
    
    while(1) {
        char c = kb_getchar();
        
        if(c == '\n') {
            console_putchar('\n');
            *buffer = 0;
            return;
        }
        
        if(c == '\b') {
            if(console.cursor_x > bound) {
                buffer--;
                *buffer = 0;
                console_ungetchar();
            }
            continue;
        }
        
        *buffer++ = c;
        console_putchar(c);
    }
}

void console_printf(const char *format, ...) {
    char **arg = (char **)&format;
    char c;
    char buf[32];
    arg++;

    while((c = *format++) != 0) {
        if(c != '%') {
            console_putchar(c);
        } else {
            char *p;
            c = *format++;
            switch(c) {
                case 'd':
                case 'x':
                    itoa(buf, c, *((int *)arg++));
                    p = buf;
                    while(*p) console_putchar(*p++);
                    break;
                case 's':
                    p = *arg++;
                    if(!p) p = "(null)";
                    while(*p) console_putchar(*p++);
                    break;
                default:
                    console_putchar(*((int *)arg++));
                    break;
            }
        }
    }
}