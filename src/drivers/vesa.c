#include <stdint.h>
#include <stddef.h>

#include "vesa.h"
#include "bios32.h"
#include "console.h"
#include "io.h"
#include "isr.h"
#include "string.h"
#include "serial.h"
#include "liballoc.h"

// vbe information
VBE20_INFOBLOCK g_vbe_infoblock;
VBE20_MODEINFOBLOCK g_vbe_modeinfoblock;
// selected mode
int32_t g_selected_mode = -1;
// selected mode width & height
uint32_t g_width = 0, g_height = 0;
// buffer pointer pointing to video memory
uint32_t *g_vbe_buffer = NULL;
uint32_t *back_buffer = NULL;

int vga;
uint32_t vbe_get_width()
{
    return g_width;
}

uint32_t vbe_get_height()
{
    return g_height;
}

// get vbe info
int get_vbe_info()
{
    REGISTERS16 in = {0}, out = {0};
    // set specific value 0x4F00 in ax to get vbe info into bios memory area
    in.ax = 0x4F00;
    // set address pointer to BIOS_CONVENTIONAL_MEMORY where vbe info struct will be stored
    in.di = BIOS_CONVENTIONAL_MEMORY;
    int86(0x10, &in, &out); // call video interrupt 0x10
    // copy vbe info data to our global variable g_vbe_infoblock
    memcpy(&g_vbe_infoblock, (void *)BIOS_CONVENTIONAL_MEMORY, sizeof(VBE20_INFOBLOCK));
    if (out.ax != 0x4F)
    {
        return 0;
    }
    return 1;
}

void get_vbe_mode_info(uint16_t mode, VBE20_MODEINFOBLOCK *mode_info)
{
    REGISTERS16 in = {0}, out = {0};
    // set specific value 0x4F00 in ax to get vbe mode info into some other bios memory area
    in.ax = 0x4F01;
    in.cx = mode;                            // set mode info to get
    in.di = BIOS_CONVENTIONAL_MEMORY + 1024; // address pointer, different than used in get_vbe_info()
    int86(0x10, &in, &out);                  // call video interrupt 0x10
    // copy vbe mode info data to parameter mode_info
    memcpy(mode_info, (void *)BIOS_CONVENTIONAL_MEMORY + 1024, sizeof(VBE20_MODEINFOBLOCK));
}

void vbe_set_mode(uint32_t mode)
{
    REGISTERS16 in = {0}, out = {0};
    // set any given mode, mode is to find by resolution X & Y
    in.ax = 0x4F02;
    in.bx = mode;
    int86(0x10, &in, &out); // call video interrupt 0x10 to set mode
    serial_printf("VBE: Set mode 0x%x, AX=0x%x\n", mode, out.ax); // Debug log
}

// find the vbe mode by width & height & bits per pixel
uint32_t vbe_find_mode(uint32_t width, uint32_t height, uint32_t bpp)
{
    // iterate through video modes list
    uint16_t *mode_list = (uint16_t *)g_vbe_infoblock.VideoModePtr;
    uint16_t mode = *mode_list++;
    while (mode != 0xffff)
    {
        // get each mode info
        get_vbe_mode_info(mode, &g_vbe_modeinfoblock);
        if (g_vbe_modeinfoblock.XResolution == width && g_vbe_modeinfoblock.YResolution == height && g_vbe_modeinfoblock.BitsPerPixel == bpp)
        {
            return mode;
        }
        mode = *mode_list++;
    }
    return -1;
}

void wait_for_vblank()
{
    // serial_printf("wait_for_vblank: waiting for vblank to end\n");
    // Wait for the current VBLANK period to end
    while ((inportb(0x03DA) & 0x08))
    {
        swap_buffers();
        // serial_printf("wait_for_vblank: vblank did not end, swapping buffers\n");
    }
    // serial_printf("wait_for_vblank: vblank ended\n");

    // serial_printf("wait_for_vblank: waiting for vblank to begin\n");
    // Wait for the next VBLANK period to begin
    while (!(inportb(0x03DA) & 0x08))
    {
    }
    // serial_printf("wait_for_vblank: vblank began\n");
}

void swap_buffers()
{
    uint32_t *temp = g_vbe_buffer;
    g_vbe_buffer = back_buffer;
    back_buffer = temp;
    // serial_printf("swap buffers\n");
}
// print availabel modes to console
void vbe_print_available_modes()
{
    VBE20_MODEINFOBLOCK modeinfoblock;

    // iterate through video modes list
    uint16_t *mode_list = (uint16_t *)g_vbe_infoblock.VideoModePtr;
    uint16_t mode = *mode_list++;
    while (mode != 0xffff)
    {
        get_vbe_mode_info(mode, &modeinfoblock);
        // console_printf("Mode: %d, X: %d, Y: %d\n", mode, modeinfoblock.XResolution, modeinfoblock.YResolution);
        mode = *mode_list++;
    }
}

// set rgb values in 32 bit number
uint32_t vbe_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t color = red;
    color <<= 16;
    color |= (green << 8);
    color |= blue;
    return color;
}

void test_vga_buffer() {
    uint8_t *vga_buffer = (uint8_t*)0xA0000;
    for (int i = 0; i < 320 * 200; i++) {
        vga_buffer[i] = 0x0F; // Bright white (8-bit color)
    }
}

// put the pixel on the given x,y point
void vbe_putpixel(int x, int y, int color) {
    if (vga != 0) {
        // VGA Mode 0x13 (320x200x8)
        if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
            serial_printf("VGA: Error - (%d, %d) is out of bounds\n", x, y);
            return;
        }
        uint32_t i = y * g_width + x;
        uint8_t *vga_buffer = (uint8_t*)0xA0000;
        vga_buffer[i] = (uint8_t)color;
        return;
    } else {
        // VESA Mode
        if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
            serial_printf("VBE: Error - (%d, %d) is out of bounds\n", x, y);
            return;
        }
        uint32_t i = y * g_width + x;
        *(back_buffer + i) = color;
    }
}


int vesa_init(uint32_t width, uint32_t height, uint32_t bpp) {
    bios32_init();

    if (!get_vbe_info()) {
        serial_printf("VESA: VBE2 not detected\n");
        goto vga_fallback; // Jump to VGA setup
    }

    g_selected_mode = vbe_find_mode(width, height, bpp);
    if (g_selected_mode == -1) {
        serial_printf("VESA: Mode %dx%d-%dbpp not found\n", width, height, bpp);
        goto vga_fallback;
    }

    // Success: Set VESA mode
    g_width = g_vbe_modeinfoblock.XResolution;
    g_height = g_vbe_modeinfoblock.YResolution;
    g_vbe_buffer = (uint32_t *)g_vbe_modeinfoblock.PhysBasePtr;
    vbe_set_mode(g_selected_mode);
    swap_buffers();

    test_vga_buffer();

    vga = 0;  // Mark VESA as active
    return 0;  // Return 0 on success
vga_fallback:
    REGISTERS16 in = {0}, out = {0};
    in.ax = 0x13;
    int86(0x10, &in, &out);
    if (out.ax != 0x004F) {
        serial_printf("VGA: Failed (AX=0x%x). Falling back to text mode.\n", out.ax);
        in.ax = 0x03; // Text mode 80x25
        int86(0x10, &in, &out);
        return -1;
    }

    // Mark VGA as active
    g_width = 320;
    g_height = 200;
    vga = 1;
    return -1;
}
