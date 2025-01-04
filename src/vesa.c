#include "vesa.h"
#include "console.h"
#include "pmm.h"
#include "string.h"
#include "multiboot.h"
#include "io.h"

#define BIOS_CONVENTIONAL_MEMORY 0x7E00

// Global state
static VBE_CONTROLLER_INFO g_vbe_info;
static VBE_MODE_INFO g_mode_info; 
static uint16 g_selected_mode = 0;
static uint32 g_width = 0, g_height = 0;
static uint32 *g_framebuffer = NULL;

uint16 vbe_get_controller_info(void) {
    REGISTERS16 regs = {0};
    
    // Set up VBE call using conventional memory
    regs.ax = 0x4F00;
    regs.di = BIOS_CONVENTIONAL_MEMORY;
    
    // Write VBE2 signature
    char *sig = (char*)BIOS_CONVENTIONAL_MEMORY;
    sig[0] = 'V'; sig[1] = 'B'; sig[2] = 'E'; sig[3] = '2';
    
    int86(0x10, &regs, &regs);
    
    if (regs.ax != 0x004F) {
        return 0;
    }

    // Copy info from conventional memory
    memcpy(&g_vbe_info, (void*)BIOS_CONVENTIONAL_MEMORY, 
           sizeof(VBE_CONTROLLER_INFO));
           
    return 1;
}

uint16 vbe_get_mode_info(uint16 mode) {
    REGISTERS16 regs = {0};
    
    regs.ax = 0x4F01;
    regs.cx = mode;
    regs.di = BIOS_CONVENTIONAL_MEMORY + 1024;
    
    int86(0x10, &regs, &regs);
    
    if (regs.ax != 0x004F) {
        return 0;
    }
    
    memcpy(&g_mode_info, (void*)(BIOS_CONVENTIONAL_MEMORY + 1024),
           sizeof(VBE_MODE_INFO));
           
    return 1;
}

void vbe_set_mode(uint16 mode) {
    REGISTERS16 regs = {0};
    regs.ax = 0x4F02;
    regs.bx = mode | 0x4000; // Set LFB bit
    int86(0x10, &regs, &regs);
}

uint16 vbe_find_mode(uint32 width, uint32 height, uint32 bpp) {
    uint16 *mode_list = (uint16*)g_vbe_info.video_modes_ptr;
    uint16 mode = *mode_list++;
    
    while (mode != 0xFFFF) {
        if (vbe_get_mode_info(mode)) {
            if (g_mode_info.x_resolution == width &&
                g_mode_info.y_resolution == height && 
                g_mode_info.bits_per_pixel == bpp) {
                return mode;
            }
        }
        mode = *mode_list++;
    }
    return 0;
}

void vbe_draw_pixel(int x, int y, uint32 color) {
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
        return;
    }
    uint32 offset = y * g_width + x;
    g_framebuffer[offset] = color;
}

uint32 vbe_rgb(uint8 red, uint8 green, uint8 blue) {
    uint32 color = red;
    color <<= 16;
    color |= (green << 8);
    color |= blue;
    return color;
}

int vbe_init(uint32 width, uint32 height, uint32 bpp) {
    printf("Initializing VESA VBE 2.0\n");
    
    if (!vbe_get_controller_info()) {
        printf("No VESA VBE 2.0 detected\n");
        return -1;
    }

    g_selected_mode = vbe_find_mode(width, height, bpp);
    if (!g_selected_mode) {
        printf("Failed to find mode %dx%dx%d\n", width, height, bpp);
        return -1;
    }

    printf("Found compatible mode: 0x%x\n", g_selected_mode);
    
    // Get final mode info
    vbe_get_mode_info(g_selected_mode);
    
    // Set global state
    g_width = g_mode_info.x_resolution;
    g_height = g_mode_info.y_resolution;
    g_framebuffer = (uint32*)g_mode_info.phys_base_ptr;
    
    // Set the mode
    vbe_set_mode(g_selected_mode);
    
    return 0;
}