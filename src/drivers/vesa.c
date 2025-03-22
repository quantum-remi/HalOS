#include "vesa.h"
#include "string.h"
#include "serial.h"
#include "isr.h"
#include "console.h"
#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "liballoc.h"

uint32_t g_width = 0, g_height = 0, g_pitch = 0, g_bpp = 0;
uint32_t *g_vbe_buffer = NULL;
uint32_t *g_back_buffer = NULL;

// set rgb values in 32 bit number
uint32_t vbe_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t color = red;
    color <<= 16;
    color |= (green << 8);
    color |= blue;
    return color;
}

static inline uint32_t *pixel_address(int x, int y)
{
    uint32_t pitch_pixels = g_pitch / sizeof(uint32_t); // Convert bytes to 32-bit words
    return g_vbe_buffer + (y * pitch_pixels) + x;
}

// put the pixel on the given x,y point
void vbe_putpixel(int x, int y, int color)
{
    if (x < 0 || x >= g_width || y < 0 || y >= g_height)
        return;
    uint32_t *location = g_back_buffer + (y * (g_pitch / 4)) + x; // Write to back buffer
    *location = color;
}

uint32_t vbe_getpixel(int x, int y)
{
    if (x < 0 || x >= g_width || y < 0 || y >= g_height)
        return 0;
    return g_back_buffer[y * (g_pitch / 4) + x]; // Read from back buffer
}

void vesa_swap_buffers()
{
    // Copy back buffer to front buffer using full pitch size in bytes
    memcpy(g_vbe_buffer, g_back_buffer, g_height * g_pitch);
}

int vesa_init(uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp)
{
    g_vbe_buffer = framebuffer;
    g_width = width;
    g_height = height;
    g_pitch = pitch;
    g_bpp = bpp;
    uint32_t buffer_size = height * pitch; // Total byte size of the backbuffer
    uint32_t pages_needed = (buffer_size + PAGE_SIZE - 1) / PAGE_SIZE;
    // Allocate back buffer using contiguous virtual memory allocation
    g_back_buffer = (uint32_t *)vmm_alloc_contiguous(pages_needed);
    if (!g_back_buffer)
    {
        serial_printf("VESA: Failed to allocate back buffer!\n");
        return -1;
    }
    serial_printf("VESA: Initialized %dx%d (%d bpp)\n", width, height, bpp);
    serial_printf("initializing vesa vbe 2.0\n");
    return 0;
}