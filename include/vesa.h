#ifndef VESA_H
#define VESA_H

#include "types.h"
#include "bios32.h"

// VBE BIOS location
#define BIOS_CONVENTIONAL_MEMORY 0x7E00

// VBE Return Status codes
#define VBE_SUCCESS      0x004F
#define VBE_FAILED      0x014F
#define VBE_UNSUPPORTED 0x024F

// VBE Mode attributes
#define VBE_MODE_LINEAR_FB     0x4000  // Linear framebuffer available

// VBE Controller Information structure
typedef struct {
    char     signature[4];     // Must be "VESA" 
    uint16   version;          // VBE version number
    uint32   oem_string_ptr;   // Pointer to OEM string
    uint32   capabilities;     // Controller capabilities
    uint32   video_modes_ptr;  // Pointer to video mode list
    uint16   total_memory;     // Memory size in 64KB blocks
    uint16   oem_software_rev;
    uint32   oem_vendor_name_ptr;
    uint32   oem_product_name_ptr;
    uint32   oem_product_rev_ptr;
    uint8    reserved[222];    // Reserved for future expansion
    uint8    oem_data[256];    // OEM scratchpad
} __attribute__((packed)) VBE_CONTROLLER_INFO;

// VBE Mode Information structure
typedef struct {
    uint16   mode_attributes;
    uint8    window_a_attributes;
    uint8    window_b_attributes; 
    uint16   window_granularity;
    uint16   window_size;
    uint16   window_a_segment;
    uint16   window_b_segment;
    uint32   window_func_ptr;
    uint16   bytes_per_scanline;
    
    // Resolution and color info
    uint16   x_resolution;
    uint16   y_resolution; 
    uint8    x_char_size;
    uint8    y_char_size;
    uint8    number_of_planes;
    uint8    bits_per_pixel;
    uint8    number_of_banks;
    uint8    memory_model;
    uint8    bank_size;
    
    // Color masks for direct color modes
    uint8    red_mask_size;
    uint8    red_field_position;
    uint8    green_mask_size;
    uint8    green_field_position; 
    uint8    blue_mask_size;
    uint8    blue_field_position;
    uint8    reserved_mask_size;
    uint8    reserved_field_position;
    
    // Direct color mode info
    uint8    direct_color_mode_info;
    
    // Linear framebuffer
    uint32   phys_base_ptr;      // Physical address for flat frame buffer
    uint32   reserved1;
    uint16   reserved2;
    
    uint8    reserved3[206];
} __attribute__((packed)) VBE_MODE_INFO;

// Function prototypes
uint16 vbe_get_controller_info(void);
uint16 vbe_get_mode_info(uint16 mode);
void vbe_set_mode(uint16 mode);
uint16 vbe_find_mode(uint32 width, uint32 height, uint32 bpp);
void vbe_draw_pixel(int x, int y, uint32 color);
uint32 vbe_rgb(uint8 red, uint8 green, uint8 blue);
int vbe_init(uint32 width, uint32 height, uint32 bpp);

#endif // VESA_H