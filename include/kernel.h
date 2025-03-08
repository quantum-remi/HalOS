#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

void panic(char *msg);

// symbols from linker.ld for section addresses
extern uint32_t __kernel_physical_start;
extern uint32_t __kernel_physical_end;
extern uint32_t __kernel_text_section_start;
extern uint32_t __kernel_text_section_end;
extern uint32_t __kernel_data_section_start;
extern uint32_t __kernel_data_section_end;
extern uint32_t __kernel_bss_section_start;
extern uint32_t __kernel_bss_section_end;
struct resolution
{
    size_t x;
    size_t y;
};

typedef struct
{
    struct
    {
        size_t k_start_addr;
        size_t k_end_addr;
        size_t k_len;
        size_t text_start_addr;
        size_t text_end_addr;
        size_t text_len;
        size_t data_start_addr;
        size_t data_end_addr;
        size_t data_len;
        size_t rodata_start_addr;
        size_t rodata_end_addr;
        size_t rodata_len;
        size_t bss_start_addr;
        size_t bss_end_addr;
        size_t bss_len;
    } kernel;

    struct
    {
        size_t total_memory;
    } system;

    struct
    {
        size_t start_addr;
        size_t end_addr;
        size_t size;
    } available;
} KERNEL_MEMORY_MAP;

extern KERNEL_MEMORY_MAP g_kmap;

#endif
