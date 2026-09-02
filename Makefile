BUILD_DIR := build
BOOT_SOURCE := Source/Assembly/Boot/Boot64.asm
ENTRY_SOURCE := Source/Assembly/Kernel/Entry.asm
KERNEL_DIR := Source/C/Kaon_System
LINKER_SCRIPT := $(KERNEL_DIR)/linker.ld

CC := gcc
LD := ld
NASM := nasm
OBJCOPY := objcopy
QEMU := qemu-system-x86_64
QEMU_MEMORY := 264M
DISK_IMAGE_SIZE := 1M

CPPFLAGS := -I$(KERNEL_DIR)/include
CFLAGS := -std=c11 -m64 -mcmodel=small -ffreestanding -fno-pie \
	-fno-stack-protector -fno-asynchronous-unwind-tables -fcf-protection=none \
	-mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
	-Wall -Wextra -Wpedantic -Werror -MMD -MP
LDFLAGS := -m elf_x86_64 -nostdlib -z max-page-size=0x1000 \
	-T $(LINKER_SCRIPT) -Map=$(BUILD_DIR)/Kaon.map

KERNEL_SOURCES := $(KERNEL_DIR)/System.c $(KERNEL_DIR)/Console.c \
	$(KERNEL_DIR)/Disk.c $(KERNEL_DIR)/Keyboard.c $(KERNEL_DIR)/Librarian.c \
	$(KERNEL_DIR)/memory.c $(KERNEL_DIR)/serial.c $(KERNEL_DIR)/vga.c
KERNEL_C_OBJECTS := $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(KERNEL_SOURCES))
KERNEL_OBJECTS := $(BUILD_DIR)/Entry.o $(KERNEL_C_OBJECTS)
DEPENDENCIES := $(KERNEL_OBJECTS:.o=.d)

.PHONY: all clean run debug test help

all: $(BUILD_DIR)/Kaon.img

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/Entry.o: $(ENTRY_SOURCE) | $(BUILD_DIR)
	$(NASM) -Wall -Wno-reloc-rel-dword -f elf64 $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(KERNEL_OBJECTS) $(LINKER_SCRIPT)
	$(LD) $(LDFLAGS) $(KERNEL_OBJECTS) -o $@

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/Boot64.bin: $(BOOT_SOURCE) $(BUILD_DIR)/kernel.bin
	@kernel_size=$$(stat -c '%s' $(BUILD_DIR)/kernel.bin); \
	kernel_sectors=$$(( (kernel_size + 511) / 512 )); \
	if [ $$kernel_sectors -lt 1 ] || [ $$kernel_sectors -gt 127 ]; then \
		echo "error: kernel is $$kernel_sectors sectors; bootstrap limit is 127" >&2; \
		exit 1; \
	fi; \
	$(NASM) -Wall -Werror -Wno-reloc-abs-word -Wno-reloc-abs-dword \
		-f bin -D KERNEL_SECTORS=$$kernel_sectors $< -o $@

$(BUILD_DIR)/Kaon.img: $(BUILD_DIR)/Boot64.bin $(BUILD_DIR)/kernel.bin
	truncate -s $(DISK_IMAGE_SIZE) $@
	dd if=$(BUILD_DIR)/Boot64.bin of=$@ bs=512 seek=0 conv=notrunc status=none
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc,sync status=none

run: $(BUILD_DIR)/Kaon.img
	$(QEMU) -m $(QEMU_MEMORY) -drive format=raw,file=$< -serial stdio \
		-no-reboot -no-shutdown

debug: $(BUILD_DIR)/Kaon.img
	$(QEMU) -m $(QEMU_MEMORY) -drive format=raw,file=$< -serial stdio \
		-no-reboot -no-shutdown -s -S

test: $(BUILD_DIR)/Kaon.img
	@set +e; \
	output=$$(timeout 3s $(QEMU) -m $(QEMU_MEMORY) \
		-drive format=raw,file=$< -display none \
		-monitor none -serial stdio -no-reboot -no-shutdown 2>&1); \
	status=$$?; \
	set -e; \
	printf '%s\n' "$$output"; \
	if [ $$status -ne 0 ] && [ $$status -ne 124 ]; then exit $$status; fi; \
	printf '%s' "$$output" | grep -Fq 'Kaon C kernel is running.'; \
	echo 'PASS: bootloader transferred control to the C kernel.'

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo 'make        Build build/Kaon.img'
	@echo 'make run    Boot Kaon in QEMU'
	@echo 'make test   Headless boot test'
	@echo 'make debug  Wait for GDB on localhost:1234'
	@echo 'make clean  Remove generated files'

-include $(DEPENDENCIES)
