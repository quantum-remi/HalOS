#include "vesa.h"
#include "bios32.h"
#include "console.h"
#include "io.h"
#include "isr.h"
#include "string.h"
#include "types.h"
#include "serial.h"
#include "liballoc.h"

// vbe information
VBE20_INFOBLOCK g_vbe_infoblock;
VBE20_MODEINFOBLOCK g_vbe_modeinfoblock;
// selected mode
int g_selected_mode = -1;
// selected mode width & height
uint32 g_width = 0, g_height = 0;
// buffer pointer pointing to video memory
uint32 *g_vbe_buffer = NULL;
uint32 *back_buffer = NULL;

uint32 vbe_get_width() {
    return g_width;
}

uint32 vbe_get_height() {
    return g_height;
}

// get vbe info
int get_vbe_info() {
    REGISTERS16 in = {0}, out = {0};
    // set specific value 0x4F00 in ax to get vbe info into bios memory area
    in.ax = 0x4F00;
    // set address pointer to BIOS_CONVENTIONAL_MEMORY where vbe info struct will be stored
    in.di = BIOS_CONVENTIONAL_MEMORY;
    int86(0x10, &in, &out);  // call video interrupt 0x10
    // copy vbe info data to our global variable g_vbe_infoblock
    memcpy(&g_vbe_infoblock, (void *)BIOS_CONVENTIONAL_MEMORY, sizeof(VBE20_INFOBLOCK));
    if (out.ax != 0x4F) {
        return 0;
    }
    return 1;
}

void get_vbe_mode_info(uint16 mode, VBE20_MODEINFOBLOCK *mode_info) {
    REGISTERS16 in = {0}, out = {0};
    // set specific value 0x4F00 in ax to get vbe mode info into some other bios memory area
    in.ax = 0x4F01;
    in.cx = mode; // set mode info to get
    in.di = BIOS_CONVENTIONAL_MEMORY + 1024;  // address pointer, different than used in get_vbe_info()
    int86(0x10, &in, &out);  // call video interrupt 0x10
    // copy vbe mode info data to parameter mode_info
    memcpy(mode_info, (void *)BIOS_CONVENTIONAL_MEMORY + 1024, sizeof(VBE20_MODEINFOBLOCK));
}

void vbe_set_mode(uint32 mode) {
    REGISTERS16 in = {0}, out = {0};
    // set any given mode, mode is to find by resolution X & Y
    in.ax = 0x4F02;
    in.bx = mode;
    int86(0x10, &in, &out);  // call video interrupt 0x10 to set mode
}

// find the vbe mode by width & height & bits per pixel
uint32 vbe_find_mode(uint32 width, uint32 height, uint32 bpp) {
    // iterate through video modes list
    uint16 *mode_list = (uint16 *)g_vbe_infoblock.VideoModePtr;
    uint16 mode = *mode_list++;
    while (mode != 0xffff) {
        // get each mode info
        get_vbe_mode_info(mode, &g_vbe_modeinfoblock);
        if (g_vbe_modeinfoblock.XResolution == width && g_vbe_modeinfoblock.YResolution == height && g_vbe_modeinfoblock.BitsPerPixel == bpp) {
            return mode;
        }
        mode = *mode_list++;
    }
    return -1;
}

void wait_for_vblank() {
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
    while (!(inportb(0x03DA) & 0x08)) {}
    // serial_printf("wait_for_vblank: vblank began\n");
}

void swap_buffers()
{
    uint32 *temp = g_vbe_buffer;
    g_vbe_buffer = back_buffer;
    back_buffer = temp;
    // serial_printf("swap buffers\n");
}
// print availabel modes to console
void vbe_print_available_modes() {
    VBE20_MODEINFOBLOCK modeinfoblock;

    // iterate through video modes list
    uint16 *mode_list = (uint16 *)g_vbe_infoblock.VideoModePtr;
    uint16 mode = *mode_list++;
    while (mode != 0xffff) {
        get_vbe_mode_info(mode, &modeinfoblock);
        console_printf("Mode: %d, X: %d, Y: %d\n", mode, modeinfoblock.XResolution, modeinfoblock.YResolution);
        mode = *mode_list++;
    }
}

// set rgb values in 32 bit number
uint32 vbe_rgb(uint8 red, uint8 green, uint8 blue) {
    uint32 color = red;
    color <<= 16;
    color |= (green << 8);
    color |= blue;
    return color;
}

// put the pixel on the given x,y point
void vbe_putpixel(int x, int y, int color) {
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
        serial_printf("VBE: Error - (%d, %d) is out of bounds\n", x, y);
        return;
    }
    uint32 i = y * g_width + x;
    *(back_buffer + i) = color;
}

int vesa_init(uint32 width, uint32 height, uint32 bpp) {
    serial_printf("VESA: Starting initialization...\n");
    
    // Initialize BIOS32 interface
    serial_printf("VESA: Initializing BIOS32...\n");
    bios32_init();
    serial_printf("VESA: BIOS32 initialized\n");
    
    serial_printf("initializing vesa vbe 2.0\n");
    if (!get_vbe_info()) {
        serial_printf("No VESA VBE 2.0 detected\n");
        return -1;
    }
    // set this to 1 to print all available modes to console
    #define PRINT_MODES 0
    #if PRINT_MODES
        serial_printf("Press UP and DOWN arrow keys to scroll\n");
        serial_printf("Modes:\n");
        vbe_print_available_modes();
        return 1;
    #else
        g_selected_mode = vbe_find_mode(width, height, bpp);
        if (g_selected_mode == -1) {
            serial_printf("failed to find mode for %d-%d\n", width, height);
            return -1;
        }
        serial_printf("\nselected mode: %d \n", g_selected_mode);
        // set selection resolution to width & height
        g_width = g_vbe_modeinfoblock.XResolution;
        g_height = g_vbe_modeinfoblock.YResolution;
        // set selected mode video physical address point to buffer for pixel plotting
        g_vbe_buffer = (uint32 *)g_vbe_modeinfoblock.PhysBasePtr;
        // set the mode to start graphics window
        vbe_set_mode(g_selected_mode);
        swap_buffers();
    #endif
    
    serial_printf("VESA: VBE2 detected successfully\n");
    return 0;
}

