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
    // Validate parameters
    if (!framebuffer || width == 0 || height == 0 || pitch == 0 || bpp == 0) {
        serial_printf("VESA: Invalid parameters\n");
        return -1;
    }

    g_vbe_buffer = framebuffer;
    g_width = width;
    g_height = height;
    g_pitch = pitch;
    g_bpp = bpp;
    
    uint32_t buffer_size = height * pitch;
    
    // Validate buffer size isn't unreasonably large
    if (buffer_size > (1024 * 1024 * 32)) { // 32MB max
        serial_printf("VESA: Requested buffer size too large: %d bytes\n", buffer_size);
        return -1;
    }

    uint32_t aligned_size = (buffer_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t pages_needed = aligned_size / PAGE_SIZE;
    
    serial_printf("VESA: Allocating %d pages (%d bytes) for back buffer\n", 
                 pages_needed, aligned_size);

    // Allocate the back buffer in smaller chunks to avoid large contiguous allocations
    const uint32_t CHUNK_SIZE = 32; // Allocate 32 pages at a time
    uint32_t remaining_pages = pages_needed;
    uint32_t current_offset = 0;

    // Create a temporary array to store physical addresses
    void **phys_chunks = malloc(sizeof(void*) * ((pages_needed + CHUNK_SIZE - 1) / CHUNK_SIZE));
    if (!phys_chunks) {
        serial_printf("VESA: Failed to allocate chunk tracking array\n");
        return -1;
    }
    int chunk_count = 0;

    while (remaining_pages > 0) {
        uint32_t chunk_pages = (remaining_pages > CHUNK_SIZE) ? CHUNK_SIZE : remaining_pages;
        void *phys_chunk = pmm_alloc_blocks(chunk_pages);
        
        if (!phys_chunk) {
            serial_printf("VESA: Failed to allocate chunk of %d pages\n", chunk_pages);
            // Clean up previously allocated chunks
            for (int i = 0; i < chunk_count; i++) {
                pmm_free_blocks(phys_chunks[i], CHUNK_SIZE);
            }
            free(phys_chunks);
            return -1;
        }
        
        phys_chunks[chunk_count++] = phys_chunk;
        
        void *virt_chunk = vmm_map_mmio((uintptr_t)phys_chunk, 
                                       chunk_pages * PAGE_SIZE,
                                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_UNCACHED);
        
        if (!virt_chunk) {
            serial_printf("VESA: Failed to map chunk\n");
            // Clean up
            for (int i = 0; i < chunk_count; i++) {
                pmm_free_blocks(phys_chunks[i], CHUNK_SIZE);
            }
            free(phys_chunks);
            return -1;
        }

        // For first chunk, store the base address
        if (current_offset == 0) {
            g_back_buffer = (uint32_t*)virt_chunk;
        }

        // Zero-initialize this chunk
        memset(virt_chunk, 0, chunk_pages * PAGE_SIZE);
        
        remaining_pages -= chunk_pages;
        current_offset += chunk_pages * PAGE_SIZE;
    }

    free(phys_chunks);
    
    serial_printf("VESA: Back buffer initialized at V:0x%x\n", (uint32_t)g_back_buffer);
    return 0;
}