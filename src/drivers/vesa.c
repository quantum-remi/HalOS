#include "vesa.h"
#include "string.h"
#include "serial.h"
#include "bios32.h"
#include "isr.h"
#include "io.h"
#include "console.h"

// vbe information
VBE20_INFOBLOCK g_vbe_infoblock;
VBE20_MODEINFOBLOCK g_vbe_modeinfoblock;

uint32_t g_width = 0, g_height = 0, g_pitch = 0, g_bpp = 0;
uint32_t *g_vbe_buffer = NULL;

// set rgb values in 32 bit number
uint32_t vbe_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    uint32_t color = red;
    color <<= 16;
    color |= (green << 8);
    color |= blue;
    return color;
}

static inline uint32_t* pixel_address(int x, int y) {
    uint32_t pitch_pixels = g_pitch / sizeof(uint32_t); // Convert bytes to 32-bit words
    return g_vbe_buffer + (y * pitch_pixels) + x;
}

// put the pixel on the given x,y point
void vbe_putpixel(int x, int y, int color) {
    if(x < 0 || x >= g_width || y < 0 || y >= g_height) return;
    uint32_t *location = g_vbe_buffer + (y * (g_pitch / 4)) + x;
    *location = color;
}

uint32_t vbe_getpixel(int x, int y) {
    if(x < 0 || x >= g_width || y < 0 || y >= g_height) return 0;
    return *pixel_address(x, y);
}

int vesa_init(uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp) {
    g_vbe_buffer = framebuffer;
    g_width = width;
    g_height = height;
    g_pitch = pitch;
    g_bpp = bpp;
    serial_printf("VESA: Initialized %dx%d (%d bpp)\n", 
        width, height, bpp);
    serial_printf("initializing vesa vbe 2.0\n");
    return 0;
}