// #include "elf.h"
// #include "fat.h"
// #include "pmm.h"
// #include "vmm.h"
// #include "serial.h"
// #include "string.h"
// #include "paging.h"
// #include "liballoc.h"

// #define MIN(a,b) ((a) < (b) ? (a) : (b))


// #define ELF_DEBUG 1

// extern FAT32_Volume fat_volume;

// static uint32_t elf_flags_to_vmm(uint32_t elf_flags) {
//     uint32_t flags = PAGE_PRESENT | PAGE_USER;
//     if(elf_flags & PF_W) flags |= PAGE_WRITABLE;
//     if(elf_flags & PF_X) flags |= PAGE_EXECUTABLE;
//     return flags;
// }

// static int validate_elf(Elf32_Ehdr *ehdr)
// {
//     // Check ELF magic
//     if (memcmp(ehdr->e_ident, ELF_MAGIC, 4) != 0)
//     {
//         serial_printf("ELF: Invalid magic\n");
//         return -1;
//     }

//     // Check 32-bit ELF
//     if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
//     {
//         serial_printf("ELF: Not 32-bit\n");
//         return -1;
//     }

//     // Check x86 architecture
//     if (ehdr->e_machine != EM_386)
//     {
//         serial_printf("ELF: Not i386\n");
//         return -1;
//     }

//     // Check executable type
//     if (ehdr->e_type != ET_EXEC)
//     {
//         serial_printf("ELF: Not executable\n");
//         return -1;
//     }

//     return 0;
// }

// int elf_load(const char *path, uint32_t *entry_point, uint32_t *user_stack)
// {
//     FAT32_File file;
//     Elf32_Ehdr ehdr;

// #if ELF_DEBUG
//     serial_printf("ELF: Loading file %s\n", path);
// #endif

//     if (!fat32_find_file(&fat_volume, path, &file))
//     {
//         serial_printf("ELF: File not found: %s\n", path);
//         return -1;
//     }

//     // Read and validate ELF header
//     fat32_read_file(&fat_volume, &file, (uint8_t *)&ehdr, sizeof(ehdr), 0);
//     if (validate_elf(&ehdr) < 0)
//         return -1;

// #if ELF_DEBUG
//     serial_printf("ELF: Valid ELF file found, entry point at 0x%x\n", ehdr.e_entry);
//     serial_printf("ELF: %d program headers at offset 0x%x\n", ehdr.e_phnum, ehdr.e_phoff);
// #endif

//     // Process program headers
//     for (int i = 0; i < ehdr.e_phnum; i++)
//     {
//         Elf32_Phdr phdr;
//         uint32_t phdr_offset = ehdr.e_phoff + (i * sizeof(Elf32_Phdr));
//         fat32_read_file(&fat_volume, &file, (uint8_t *)&phdr, sizeof(phdr), phdr_offset);

//         if (phdr.p_type != PT_LOAD)
//             continue;

//         uint32_t mem_pages = (phdr.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
//         uint32_t file_pages = (phdr.p_filesz + PAGE_SIZE - 1) / PAGE_SIZE;
//         uint32_t file_remaining = phdr.p_filesz;
//         uint32_t file_offset = phdr.p_offset;

// #if ELF_DEBUG
//         serial_printf("ELF: Loading segment %d: vaddr=0x%x, filesz=%d, memsz=%d\n",
//                       i, phdr.p_vaddr, phdr.p_filesz, phdr.p_memsz);
// #endif

//         // Load file content pages
//         for (uint32_t j = 0; j < file_pages; j++)
//         {
//             void *phys = pmm_alloc_block();
//             if (!phys)
//             {
//                 serial_printf("ELF: Physical allocation failed\n");
//                 return -1;
//             }

//             uint32_t virt = phdr.p_vaddr + j * PAGE_SIZE;
//             uint32_t flags = elf_flags_to_vmm(phdr.p_flags) | PAGE_USER;

//             // Map the page
//             if (!paging_map_page((uint32_t)phys, virt, flags))
//             {
//                 serial_printf("ELF: Mapping failed at 0x%x\n", virt);
//                 return -1;
//             }

//             // Read data from file
//             uint32_t read_size = MIN(PAGE_SIZE, file_remaining);
//             fat32_read_file(&fat_volume, &file, (uint8_t *)phys, read_size, file_offset);

//             // Zero-fill remainder if needed
//             if (read_size < PAGE_SIZE)
//             {
//                 memset((uint8_t *)phys + read_size, 0, PAGE_SIZE - read_size);
//             }

//             file_offset += read_size;
//             file_remaining -= read_size;

// #if ELF_DEBUG
//             serial_printf("ELF: Mapped file page 0x%x (phys 0x%x) with %d bytes\n",
//                           virt, phys, read_size);
// #endif
//         }

//         // Zero-initialize BSS pages
//         for (uint32_t j = file_pages; j < mem_pages; j++)
//         {
//             void *phys = pmm_alloc_block();
//             if (!phys)
//             {
//                 serial_printf("ELF: Physical allocation failed\n");
//                 return -1;
//             }

//             uint32_t virt = phdr.p_vaddr + j * PAGE_SIZE;
//             uint32_t flags = elf_flags_to_vmm(phdr.p_flags) | PAGE_USER;

//             // Map the page
//             if (!paging_map_page((uint32_t)phys, virt, flags))
//             {
//                 serial_printf("ELF: Mapping failed at 0x%x\n", virt);
//                 return -1;
//             }

//             // Zero the entire page
//             memset(phys, 0, PAGE_SIZE);

// #if ELF_DEBUG
//             serial_printf("ELF: Mapped zero page 0x%x (phys 0x%x)\n", virt, phys);
// #endif
//         }
//     }

//     // Allocate user stack (16KB)
//     *user_stack = (uint32_t)vmm_alloc_userspace_pages(4);
//     if (!*user_stack)
//     {
//         serial_printf("ELF: Stack allocation failed\n");
//         return -1;
//     }

// #if ELF_DEBUG
//     serial_printf("ELF: User stack allocated at 0x%x\n", *user_stack);
// #endif

//     *entry_point = ehdr.e_entry;
//     return 0;
// }

// void elf_execute(int entry_point)
// {
//     void (*entry)() = (void (*)())entry_point;

//     serial_printf("ELF: Jumping to 0x%x\n", entry_point);
//     entry();

//     serial_printf("ELF: Program exited\n");
// }