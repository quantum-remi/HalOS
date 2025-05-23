# assembler
ASM = /usr/bin/nasm
# compiler
CC = i686-elf-gcc
# linker
LD = i686-elf-ld
# object copy
OBJCOPY = i686-elf-objcopy
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
CC_FLAGS = $(INCLUDE) $(DEFINES) -m32 -g -std=c23 -ffreestanding -Wall -Wextra -O0
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
		$(ASM_OBJ)/load_idt.o $(ASM_OBJ)/exception.o $(ASM_OBJ)/irq.o $(ASM_OBJ)/tasks.o \
		$(OBJ)/io.o \
		$(OBJ)/string.o $(OBJ)/console.o\
		$(OBJ)/gdt.o $(OBJ)/idt.o $(OBJ)/isr.o $(OBJ)/8259_pic.o\
		$(OBJ)/keyboard.o $(OBJ)/timer.o\
		$(OBJ)/pmm.o $(OBJ)/vmm.o \
		$(OBJ)/paging.o  $(OBJ)/snake.o \
		$(OBJ)/vesa.o $(OBJ)/fpu.o \
		$(OBJ)/shell.o \
		$(OBJ)/serial.o $(OBJ)/printf.o \
		$(OBJ)/tss.o $(OBJ)/liballoc.o $(OBJ)/liballoc_hook.o \
		$(OBJ)/pci.o $(OBJ)/ide.o $(OBJ)/fat.o $(OBJ)/font.o \
		$(OBJ)/rtl8139.o $(OBJ)/arp.o $(OBJ)/eth.o $(OBJ)/network.o $(OBJ)/ipv4.o $(OBJ)/icmp.o \
		$(OBJ)/math.o $(OBJ)/elf.o $(OBJ)/pong.o $(OBJ)/ne2k.o $(OBJ)/tcp.o\
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

$(OBJ)/font.o : $(CONFIG)/output.psf
	@printf "[ $(CONFIG)/output.psf ]\n"
	$(OBJCOPY) -O elf32-i386 -B i386 -I binary $(CONFIG)/output.psf $(OBJ)/font.o
	@printf "\n"

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

$(ASM_OBJ)/tasks.o : $(ASM_SRC)/tasks.asm
	@printf "[ $(ASM_SRC)/tasks.asm ]\n"
	$(ASM) $(ASM_FLAGS) $(ASM_SRC)/tasks.asm -o $(ASM_OBJ)/tasks.o
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

$(OBJ)/pong.o : $(SRC)/shell/pong.c
	@printf "[ $(SRC)/shell/pong.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/shell/pong.c -o $(OBJ)/pong.o
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

$(OBJ)/elf.o : $(SRC)/shell/elf.c
	@printf "[ $(SRC)/shell/elf.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/shell/elf.c -o $(OBJ)/elf.o
	@printf "\n"

$(OBJ)/serial.o : $(SRC)/drivers/serial.c
	@printf "[ $(SRC)/drivers/serial.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/serial.c -o $(OBJ)/serial.o
	@printf "\n"

$(OBJ)/printf.o : $(SRC)/libs/printf.c
	@printf "[ $(SRC)/libs/printf.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/libs/printf.c -o $(OBJ)/printf.o
	@printf "\n"

$(OBJ)/math.o : $(SRC)/libs/math.c
	@printf "[ $(SRC)/libs/math.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/libs/math.c -o $(OBJ)/math.o
	@printf "\n"

$(OBJ)/tss.o : $(SRC)/cpu/tss.c
	@printf "[ $(SRC)/cpu/tss.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/cpu/tss.c -o $(OBJ)/tss.o
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

$(OBJ)/rtl8139.o : $(SRC)/drivers/nic/rtl8139.c
	@printf "[ $(SRC)/drivers/nic/rtl8139.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/nic/rtl8139.c -o $(OBJ)/rtl8139.o
	@printf "\n"

$(OBJ)/ne2k.o : $(SRC)/drivers/nic/ne2k.c
	@printf "[ $(SRC)/drivers/nic/ne2k.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/nic/ne2k.c -o $(OBJ)/ne2k.o
	@printf "\n"

$(OBJ)/eth.o : $(SRC)/drivers/net/eth.c
	@printf "[ $(SRC)/drivers/net/eth.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/net/eth.c -o $(OBJ)/eth.o
	@printf "\n"

$(OBJ)/arp.o : $(SRC)/drivers/net/arp.c
	@printf "[ $(SRC)/drivers/net/arp.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/net/arp.c -o $(OBJ)/arp.o
	@printf "\n"

$(OBJ)/network.o : $(SRC)/drivers/net/network.c
	@printf "[ $(SRC)/drivers/net/network.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/net/network.c -o $(OBJ)/network.o
	@printf "\n"

$(OBJ)/ipv4.o : $(SRC)/drivers/net/ipv4.c
	@printf "[ $(SRC)/drivers/net/ipv4.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/net/ipv4.c -o $(OBJ)/ipv4.o
	@printf "\n"

$(OBJ)/icmp.o : $(SRC)/drivers/net/icmp.c
	@printf "[ $(SRC)/drivers/net/icmp.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/net/icmp.c -o $(OBJ)/icmp.o
	@printf "\n"

$(OBJ)/tcp.o : $(SRC)/drivers/net/tcp.c
	@printf "[ $(SRC)/drivers/net/tcp.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/drivers/net/tcp.c -o $(OBJ)/tcp.o
	@printf "\n"

qemu:
	qemu-system-i386 -m 4G -vga virtio -boot d -cdrom $(TARGET_ISO) \
	-serial stdio -drive id=disk,if=none,format=raw,file=disk.img \
	-device ide-hd,drive=disk -cpu qemu64,+fpu,+sse,+sse2 \
	-netdev user,id=net0,hostfwd=tcp::8080-:8080 -device rtl8139,netdev=net0 \
	-object filter-dump,id=f1,netdev=net0,file=network.pcap

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
	qemu-system-i386 -m 1G -vga virtio -boot d -cdrom $(TARGET_ISO) -serial stdio -drive id=disk,if=none,format=raw,file=disk.img -device ide-hd,drive=disk -cpu qemu64,+fpu,+sse,+sse2 -s -S
clean:
	rm -f $(OBJ)/*.o
	rm -f $(ASM_OBJ)/*.o
	rm -rf $(OUT)/*

distclean:
	rm -rf $(OUT)/*
	rm -rf $(ISO_DIR)
