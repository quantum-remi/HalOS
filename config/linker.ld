ENTRY(_start)

SECTIONS {
    /* Place the Multiboot header at the VERY START of the kernel */
    . = 0x100000;  /* Load address = 1MB (standard for x86 kernels) */

    /* Multiboot header section (must be first!) */
    .multiboot : {
        *(.multiboot)
    }

    /* Text section (code) */
    .text : ALIGN(4096) {
        __kernel_text_section_start = .;
        *(.text)
        *(.rodata*)
        __kernel_text_section_end = .;
    }

    /* Read-only data */
    .rodata : ALIGN(4096) {
        __kernel_rodata_section_start = .;
        *(.rodata)
        __kernel_rodata_section_end = .;
    }

    /* Initialized data */
    .data : ALIGN(4096) {
        __kernel_data_section_start = .;
        *(.data)
        __kernel_data_section_end = .;
    }

    /* Uninitialized data (BSS) */
    .bss : ALIGN(4096) {
        __kernel_bss_section_start = .;
        *(COMMON)
        *(.bss)
        __kernel_bss_section_end = .;
    }

    /* Define kernel memory regions */
    __kernel_physical_start = 0x100000;
    __kernel_physical_end = .;
}