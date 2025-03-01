# assembler
ASM = /usr/bin/nasm
# compiler
CC = i686-elf-gcc
# linker
LD = i686-elf-ld
# grub iso creator
GRUB = /usr/bin/grub2-mkrescue
# sources
SRC = src
ASM_SRC = $(SRC)/asm
# objects
OBJ = obj
ASM_OBJ = $(OBJ)/asm
CONFIG = ./config
OUT = out
INC = ./include
INCLUDE=-I$(INC)

MKDIR= mkdir -p
CP = cp -f
DEFINES=

# assembler flags
ASM_FLAGS = -f elf32
# compiler flags
CC_FLAGS = $(INCLUDE) $(DEFINES) -m32 -g -std=c23 -ffreestanding -Wall -Wextra -mno-sse -mno-sse2 -mno-sse3
# linker flags, for linker add linker.ld file too
LD_FLAGS = -m elf_i386 -T $(CONFIG)/linker.ld -nostdlib
# make flags
MAKEFLAGS += -j$(nproc)

# target file to create in linking
TARGET=$(OUT)/kernel.elf

# iso file target to create
TARGET_ISO=$(OUT)/os.iso
ISO_DIR=$(OUT)/isodir

OBJECTS = $(ASM_OBJ)/entry.o $(ASM_OBJ)/load_gdt.o $(ASM_OBJ)/load_tss.o \
		$(ASM_OBJ)/load_idt.o $(ASM_OBJ)/exception.o $(ASM_OBJ)/irq.o $(ASM_OBJ)/bios32_call.o\
		$(ASM_OBJ)/vesa.o\
		$(OBJ)/io.o \
		$(OBJ)/string.o $(OBJ)/console.o\
		$(OBJ)/gdt.o $(OBJ)/idt.o $(OBJ)/isr.o $(OBJ)/8259_pic.o\
		$(OBJ)/keyboard.o $(OBJ)/timer.o\
		$(OBJ)/pmm.o $(OBJ)/vmm.o \
		$(OBJ)/paging.o  $(OBJ)/snake.o \
		$(OBJ)/vesa.o $(OBJ)/fpu.o \
		$(OBJ)/bios32.o $(OBJ)/shell.o \
		$(OBJ)/serial.o $(OBJ)/printf.o \
		$(OBJ)/tss.o $(OBJ)/liballoc.o $(OBJ)/liballoc_hook.o \
		$(OBJ)/pci.o $(OBJ)/ide.o $(OBJ)/fat.o \
		$(OBJ)/kernel.o

.PHONY: all	


all: $(OBJECTS)
	@printf "[ linking... ]\n"
	$(LD) $(LD_FLAGS) -o $(TARGET) $(OBJECTS)
	grub2-file --is-x86-multiboot $(TARGET) && echo "Valid" || echo "Invalid"
	@printf "\n"
	@printf "[ building ISO... ]\n"
	$(MKDIR) $(ISO_DIR)/boot/grub
	$(CP) $(TARGET) $(ISO_DIR)/boot/
	$(CP) $(CONFIG)/grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB) -o $(TARGET_ISO) $(ISO_DIR)
	rm -f $(TARGET)

$(ASM_OBJ)/entry.o : $(ASM_SRC)/entry.asm
	@printf "[ $(ASM_SRC)/entry.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/entry.asm -o $(ASM_OBJ)/entry.o
	@printf "\n"

$(ASM_OBJ)/load_gdt.o : $(ASM_SRC)/load_gdt.asm
	@printf "[ $(ASM_SRC)/load_gdt.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/load_gdt.asm -o $(ASM_OBJ)/load_gdt.o
	@printf "\n"

$(ASM_OBJ)/load_idt.o : $(ASM_SRC)/load_idt.asm
	@printf "[ $(ASM_SRC)/load_idt.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/load_idt.asm -o $(ASM_OBJ)/load_idt.o
	@printf "\n"

$(ASM_OBJ)/load_tss.o : $(ASM_SRC)/load_tss.asm
	@printf "[ $(ASM_SRC)/load_tss.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/load_tss.asm -o $(ASM_OBJ)/load_tss.o
	@printf "\n"

$(ASM_OBJ)/exception.o : $(ASM_SRC)/exception.asm
	@printf "[ $(ASM_SRC)/exception.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/exception.asm -o $(ASM_OBJ)/exception.o
	@printf "\n"

$(ASM_OBJ)/irq.o : $(ASM_SRC)/irq.asm
	@printf "[ $(ASM_SRC)/irq.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/irq.asm -o $(ASM_OBJ)/irq.o
	@printf "\n"

$(ASM_OBJ)/bios32_call.o : $(ASM_SRC)/bios32_call.asm
	@printf "[ $(ASM_SRC)/bios32_call.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/bios32_call.asm -o $(ASM_OBJ)/bios32_call.o
	@printf "\n"

$(ASM_OBJ)/vesa.o : $(ASM_SRC)/vesa.asm
	@printf "[ $(ASM_SRC)/vesa.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/vesa.asm -o $(ASM_OBJ)/vesa.o
	@printf "\n"

$(OBJ)/io.o : $(SRC)/libs/io.c
	@printf "[ $(SRC)/libs/io.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/libs/io.c -o $(OBJ)/io.o
	@printf "\n"

$(OBJ)/string.o : $(SRC)/libs/string.c
	@printf "[ $(SRC)/libs/string.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/libs/string.c -o $(OBJ)/string.o
	@printf "\n"

$(OBJ)/console.o : $(SRC)/drivers/console.c
	@printf "[ $(SRC)/drivers/console.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/console.c -o $(OBJ)/console.o
	@printf "\n"

$(OBJ)/gdt.o : $(SRC)/cpu/gdt.c
	@printf "[ $(SRC)/cpu/gdt.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/cpu/gdt.c -o $(OBJ)/gdt.o
	@printf "\n"

$(OBJ)/idt.o : $(SRC)/cpu/idt.c
	@printf "[ $(SRC)/cpu/idt.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/cpu/idt.c -o $(OBJ)/idt.o
	@printf "\n"

$(OBJ)/isr.o : $(SRC)/cpu/isr.c
	@printf "[ $(SRC)/cpu/isr.c]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/cpu/isr.c -o $(OBJ)/isr.o
	@printf "\n"

$(OBJ)/8259_pic.o : $(SRC)/drivers/8259_pic.c
	@printf "[ $(SRC)/drivers/8259_pic.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/8259_pic.c -o $(OBJ)/8259_pic.o
	@printf "\n"

$(OBJ)/keyboard.o : $(SRC)/drivers/keyboard.c
	@printf "[ $(SRC)/drivers/keyboard.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/keyboard.c -o $(OBJ)/keyboard.o
	@printf "\n"
$(OBJ)/timer.o : $(SRC)/drivers/timer.c
	@printf "[ $(SRC)/drivers/timer.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/timer.c -o $(OBJ)/timer.o
	@printf "\n"

$(OBJ)/pmm.o : $(SRC)/mm/pmm.c
	@printf "[ $(SRC)/mm/pmm.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/mm/pmm.c -o $(OBJ)/pmm.o
	@printf "\n"

$(OBJ)/vmm.o : $(SRC)/mm/vmm.c
	@printf "[ $(SRC)/mm/vmm.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/mm/vmm.c -o $(OBJ)/vmm.o
	@printf "\n"

$(OBJ)/kernel.o : $(SRC)/kernel.c
	@printf "[ $(SRC)/kernel.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/kernel.c -o $(OBJ)/kernel.o
	@printf "\n"

$(OBJ)/paging.o : $(SRC)/mm/paging.c
	@printf "[ $(SRC)/mm/paging.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/mm/paging.c -o $(OBJ)/paging.o
	@printf "\n"

$(OBJ)/snake.o : $(SRC)/shell/snake.c
	@printf "[ $(SRC)/shell/snake.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/shell/snake.c -o $(OBJ)/snake.o
	@printf "\n"

$(OBJ)/bios32.o : $(SRC)/cpu/bios32.c
	@printf "[ $(SRC)/cpu/bios32.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/cpu/bios32.c -o $(OBJ)/bios32.o
	@printf "\n"

$(OBJ)/vesa.o : $(SRC)/drivers/vesa.c
	@printf "[ $(SRC)/drivers/vesa.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/vesa.c -o $(OBJ)/vesa.o
	@printf "\n"

$(OBJ)/fpu.o : $(SRC)/cpu/fpu.c
	@printf "[ $(SRC)/cpu/fpu.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/cpu/fpu.c -o $(OBJ)/fpu.o
	@printf "\n"

$(OBJ)/shell.o : $(SRC)/shell/shell.c
	@printf "[ $(SRC)/shell/shell.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/shell/shell.c -o $(OBJ)/shell.o
	@printf "\n"

$(OBJ)/serial.o : $(SRC)/drivers/serial.c
	@printf "[ $(SRC)/drivers/serial.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/serial.c -o $(OBJ)/serial.o
	@printf "\n"

$(OBJ)/printf.o : $(SRC)/libs/printf.c
	@printf "[ $(SRC)/libs/printf.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/libs/printf.c -o $(OBJ)/printf.o
	@printf "\n"

$(OBJ)/tss.o : $(SRC)/mm/tss.c
	@printf "[ $(SRC)/mm/tss.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/mm/tss.c -o $(OBJ)/tss.o
	@printf "\n"

$(OBJ)/liballoc.o : $(SRC)/mm/liballoc.c
	@printf "[ $(SRC)/mm/liballoc.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/mm/liballoc.c -o $(OBJ)/liballoc.o
	@printf "\n"

$(OBJ)/liballoc_hook.o : $(SRC)/mm/liballoc_hook.c
	@printf "[ $(SRC)/mm/liballoc_hook.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/mm/liballoc_hook.c -o $(OBJ)/liballoc_hook.o
	@printf "\n"

$(OBJ)/pci.o : $(SRC)/drivers/pci.c
	@printf "[ $(SRC)/drivers/pci.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/pci.c -o $(OBJ)/pci.o
	@printf "\n"

$(OBJ)/ide.o : $(SRC)/drivers/ide.c
	@printf "[ $(SRC)/drivers/ide.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/ide.c -o $(OBJ)/ide.o
	@printf "\n"

$(OBJ)/fat.o : $(SRC)/drivers/fat.c
	@printf "[ $(SRC)/drivers/fat.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/fat.c -o $(OBJ)/fat.o
	@printf "\n"

qemu:
	qemu-system-i386 -m 4G -vga virtio -cdrom $(TARGET_ISO) -serial stdio -drive id=disk,if=none,format=raw,file=disk.img -device ide-hd,drive=disk -cpu qemu64,+fpu,+sse,+sse2

disk:
	qemu-img create disk.img 1G

rm-disk:
	rm -f disk.img
dev:
	make clean
	make
	make qemu

debug:
	make clean
	make
	qemu-system-i386 -m 1G -vga virtio -cdrom $(TARGET_ISO) -serial stdio -drive id=disk,if=none,format=raw,file=disk.img -device ide-hd,drive=disk -cpu qemu64,+fpu,+sse,+sse2 -s -S
clean:
	rm -f $(OBJ)/*.o
	rm -f $(ASM_OBJ)/*.o
	rm -rf $(OUT)/*

distclean:
	rm -rf $(OUT)/*
	rm -rf $(ISO_DIR)
