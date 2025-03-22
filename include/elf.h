#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define EI_NIDENT 16

// e_ident indices
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define EI_OSABI      7
#define EI_ABIVERSION 8

// ELF classes
#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2

// ELF data encodings
#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

// ELF types
#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

// Machines
#define EM_NONE  0
#define EM_386   3  // x86
#define EM_X86_64 62 // x86-64

// Version
#define EV_CURRENT 1

// ELF magic
#define ELF_MAGIC "\x7F" "ELF"

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

int elf_load(const char *path, uint32_t *entry_point, uint32_t *user_stack);
void elf_execute(int entry);

#endif