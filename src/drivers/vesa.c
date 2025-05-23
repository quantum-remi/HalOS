#include "vesa.h"
#include "string.h"
#include "serial.h"
#include "isr.h"
#include "console.h"
#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "liballoc.h"
#include "io.h"

uint32_t g_width = 0, g_height = 0, g_pitch = 0, g_bpp = 0;
uint32_t *g_vbe_buffer = NULL;
uint32_t *g_back_buffer = NULL;

static bool g_vsync_enabled = true;
static bool g_vsync_supported = false;

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
    uint32_t pitch_pixels = g_pitch / sizeof(uint32_t); 
    return g_vbe_buffer + (y * pitch_pixels) + x;
}

void vbe_putpixel(int x, int y, int color)
{
    if (x < 0 || (uint32_t)x >= g_width || y < 0 || (uint32_t)y >= g_height)
        return;
    uint32_t *location = g_back_buffer + (y * (g_pitch / 4)) + x; 
    *location = color;
}

uint32_t vbe_getpixel(int x, int y)
{
    if (x < 0 || (uint32_t)x >= g_width || y < 0 || (uint32_t)y >= g_height)
        return 0;
    return g_back_buffer[y * (g_pitch / 4) + x]; 
}

void vesa_wait_for_vsync(void)
{
    if (!g_vsync_enabled || !g_vsync_supported)
        return;

    while (inportb(0x3DA) & 0x08);
    
    while (!(inportb(0x3DA) & 0x08));
}

void vesa_swap_buffers()
{
    vesa_wait_for_vsync();
    memcpy(g_vbe_buffer, g_back_buffer, g_height * g_pitch);
}

bool vesa_is_vsync_supported(void)
{
    return g_vsync_supported;
}

void vesa_enable_vsync(bool enable)
{
    g_vsync_enabled = enable;
}

int vesa_init(uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp)
{
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
    // pls dont ask me why simple mm is made like that but it works :3
    if (buffer_size > (1024 * 1024 * 32)) {
        serial_printf("VESA: Requested buffer size too large: %d bytes\n", buffer_size);
        return -1;
    }

    uint32_t aligned_size = (buffer_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t pages_needed = aligned_size / PAGE_SIZE;
    
    serial_printf("VESA: Allocating %d pages (%d bytes) for back buffer\n", 
                 pages_needed, aligned_size);

    const uint32_t CHUNK_SIZE = 32; 
    uint32_t remaining_pages = pages_needed;
    uint32_t current_offset = 0;

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
            for (int i = 0; i < chunk_count; i++) {
                pmm_free_blocks(phys_chunks[i], CHUNK_SIZE);
            }
            free(phys_chunks);
            return -1;
        }

        if (current_offset == 0) {
            g_back_buffer = (uint32_t*)virt_chunk;
        }

        memset(virt_chunk, 0, chunk_pages * PAGE_SIZE);
        
        remaining_pages -= chunk_pages;
        current_offset += chunk_pages * PAGE_SIZE;
    }

    free(phys_chunks);
    
    g_vsync_supported = true;
    uint8_t status = inportb(0x3DA);
    if (status == 0xFF) {
        g_vsync_supported = false;
        serial_printf("VESA: VSync not supported\n");
    } else {
        serial_printf("VESA: VSync supported\n");
    }

    serial_printf("VESA: Back buffer initialized at V:0x%x\n", (uint32_t)g_back_buffer);
    return 0;
}