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
CC_FLAGS = $(INCLUDE) $(DEFINES) -m32 -std=gnu99 -ffreestanding -Wall -Wextra
# linker flags, for linker add linker.ld file too
LD_FLAGS = -m elf_i386 -T $(CONFIG)/linker.ld -nostdlib

# target file to create in linking
TARGET=$(OUT)/kernel.bin

# iso file target to create
TARGET_ISO=$(OUT)/os.iso
ISO_DIR=$(OUT)/isodir

OBJECTS = $(ASM_OBJ)/entry.o $(ASM_OBJ)/load_gdt.o\
		$(ASM_OBJ)/load_idt.o $(ASM_OBJ)/exception.o $(ASM_OBJ)/irq.o $(ASM_OBJ)/bios32_call.o\
		$(OBJ)/io.o $(OBJ)/vga.o\
		$(OBJ)/string.o $(OBJ)/console.o\
		$(OBJ)/gdt.o $(OBJ)/idt.o $(OBJ)/isr.o $(OBJ)/8259_pic.o\
		$(OBJ)/keyboard.o $(OBJ)/timer.o\
		$(OBJ)/pmm.o $(OBJ)/kheap.o \
		$(OBJ)/paging.o  $(OBJ)/snake.o \
		$(OBJ)/vesa.o $(OBJ)/fpu.o \
		$(OBJ)/bios32.o \
		$(OBJ)/kernel.o

all: $(OBJECTS)
	@printf "[ linking... ]\n"
	$(LD) $(LD_FLAGS) -o $(TARGET) $(OBJECTS)
	grub2-file --is-x86-multiboot $(TARGET)
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

$(OBJ)/io.o : $(SRC)/io.c
	@printf "[ $(SRC)/io.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/io.c -o $(OBJ)/io.o
	@printf "\n"

$(OBJ)/vga.o : $(SRC)/vga.c
	@printf "[ $(SRC)/vga.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/vga.c -o $(OBJ)/vga.o
	@printf "\n"

$(OBJ)/string.o : $(SRC)/string.c
	@printf "[ $(SRC)/string.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/string.c -o $(OBJ)/string.o
	@printf "\n"

$(OBJ)/console.o : $(SRC)/console.c
	@printf "[ $(SRC)/console.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/console.c -o $(OBJ)/console.o
	@printf "\n"

$(OBJ)/gdt.o : $(SRC)/gdt.c
	@printf "[ $(SRC)/gdt.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/gdt.c -o $(OBJ)/gdt.o
	@printf "\n"

$(OBJ)/idt.o : $(SRC)/idt.c
	@printf "[ $(SRC)/idt.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/idt.c -o $(OBJ)/idt.o
	@printf "\n"

$(OBJ)/isr.o : $(SRC)/isr.c
	@printf "[ $(SRC)/isr.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/isr.c -o $(OBJ)/isr.o
	@printf "\n"

$(OBJ)/8259_pic.o : $(SRC)/8259_pic.c
	@printf "[ $(SRC)/8259_pic.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/8259_pic.c -o $(OBJ)/8259_pic.o
	@printf "\n"

$(OBJ)/keyboard.o : $(SRC)/keyboard.c
	@printf "[ $(SRC)/keyboard.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/keyboard.c -o $(OBJ)/keyboard.o
	@printf "\n"
$(OBJ)/timer.o : $(SRC)/timer.c
	@printf "[ $(SRC)/timer.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/timer.c -o $(OBJ)/timer.o
	@printf "\n"

$(OBJ)/pmm.o : $(SRC)/pmm.c
	@printf "[ $(SRC)/pmm.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/pmm.c -o $(OBJ)/pmm.o
	@printf "\n"

$(OBJ)/kernel.o : $(SRC)/kernel.c
	@printf "[ $(SRC)/kernel.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/kernel.c -o $(OBJ)/kernel.o
	@printf "\n"

$(OBJ)/kheap.o : $(SRC)/kheap.c
	@printf "[ $(SRC)/kheap.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/kheap.c -o $(OBJ)/kheap.o
	@printf "\n"

$(OBJ)/paging.o : $(SRC)/paging.c
	@printf "[ $(SRC)/paging.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/paging.c -o $(OBJ)/paging.o
	@printf "\n"

$(OBJ)/snake.o : $(SRC)/snake.c
	@printf "[ $(SRC)/snake.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/snake.c -o $(OBJ)/snake.o
	@printf "\n"

$(OBJ)/bios32.o : $(SRC)/bios32.c
	@printf "[ $(SRC)/bios32.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/bios32.c -o $(OBJ)/bios32.o
	@printf "\n"

$(OBJ)/vesa.o : $(SRC)/vesa.c
	@printf "[ $(SRC)/vesa.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/vesa.c -o $(OBJ)/vesa.o
	@printf "\n"

$(OBJ)/fpu.o : $(SRC)/fpu.c
	@printf "[ $(SRC)/fpu.c ]\n"
	$(CC) $(CC_FLAGS) -c $(SRC)/fpu.c -o $(OBJ)/fpu.o
	@printf "\n"

clean:
	rm -f $(OBJ)/*.o
	rm -f $(ASM_OBJ)/*.o
	rm -rf $(OUT)/*