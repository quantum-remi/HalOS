#ifndef VESA_H
#define VESA_H

#include <stdint.h>
#include <stddef.h>

int vesa_init(uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
uint32_t vbe_rgb(uint8_t red, uint8_t green, uint8_t blue);
void vbe_putpixel(int x, int y, int color);
uint32_t vbe_getpixel(int x, int y);
void vesa_swap_buffers();

#define VBE_RGB(r, g, b) vbe_rgb(r, g, b)

#endif
