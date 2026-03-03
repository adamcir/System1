I386_CC ?= i686-elf-gcc
I386_LD ?= i686-elf-ld
X64_CC  ?= x86_64-elf-gcc
X64_LD  ?= x86_64-elf-ld
AR      ?= ar
QEMU32  ?= qemu-system-i386
QEMU64  ?= qemu-system-x86_64

BUILD_DIR := build

CFLAGS_COMMON := -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc -Wall -Wextra
INCLUDES := -Iinclude -Ikernel/modules/vga -Ikernel/modules/panic -Ikernel/modules/bootlog

KERNEL32 := $(BUILD_DIR)/kernel32.elf
KERNEL64 := $(BUILD_DIR)/kernel64.elf
ISO32    := $(BUILD_DIR)/system1-iso-32.iso
ISO64    := $(BUILD_DIR)/system1-iso-x86_64.iso
IMG32    := $(BUILD_DIR)/system1-img-32.img

MB2_SRC     := boot/multiboot2_header.S
ENTRY32_SRC := arch/i386/entry_i386.S
ENTRY64_SRC := arch/x86_64/entry_x86_64_bridge.S
KMAIN32_SRC := kernel/core/kmain_i386.c
KMAIN64_SRC := kernel/core/kmain_x86_64.c
LDS32       := linker/linker.i386.ld
LDS64       := linker/linker.x86_64.ld

MOD_VGA_SRC    := kernel/modules/vga/vga.c
MOD_PANIC_SRC  := kernel/modules/panic/panic.c
MOD_BOOTLOG_SRC:= kernel/modules/bootlog/bootlog.c

MOD_VGA_OBJ_32     := $(BUILD_DIR)/modules/i386/vga/vga.o
MOD_PANIC_OBJ_32   := $(BUILD_DIR)/modules/i386/panic/panic.o
MOD_BOOTLOG_OBJ_32 := $(BUILD_DIR)/modules/i386/bootlog/bootlog.o

MOD_VGA_OBJ_64     := $(BUILD_DIR)/modules/x86_64/vga/vga.o
MOD_PANIC_OBJ_64   := $(BUILD_DIR)/modules/x86_64/panic/panic.o
MOD_BOOTLOG_OBJ_64 := $(BUILD_DIR)/modules/x86_64/bootlog/bootlog.o

MOD_VGA_LIB_32     := $(BUILD_DIR)/modules/i386/vga/libvga.a
MOD_PANIC_LIB_32   := $(BUILD_DIR)/modules/i386/panic/libpanic.a
MOD_BOOTLOG_LIB_32 := $(BUILD_DIR)/modules/i386/bootlog/libbootlog.a

MOD_VGA_LIB_64     := $(BUILD_DIR)/modules/x86_64/vga/libvga.a
MOD_PANIC_LIB_64   := $(BUILD_DIR)/modules/x86_64/panic/libpanic.a
MOD_BOOTLOG_LIB_64 := $(BUILD_DIR)/modules/x86_64/bootlog/libbootlog.a

MODULE_LIBS_32 := $(MOD_VGA_LIB_32) $(MOD_PANIC_LIB_32) $(MOD_BOOTLOG_LIB_32)
MODULE_LIBS_64 := $(MOD_VGA_LIB_64) $(MOD_PANIC_LIB_64) $(MOD_BOOTLOG_LIB_64)

.PHONY: all help modules iso-32 iso-x86_64 img-32 run-iso-32 run-iso-x86_64 run-img-32 clean

all: iso-32 iso-x86_64

help:
	@echo "Targets:"
	@echo "  modules        Build kernel module static libraries (.a)"
	@echo "  iso-32         Build i386 GRUB ISO ($(ISO32))"
	@echo "  iso-x86_64     Build x86_64 GRUB ISO ($(ISO64))"
	@echo "  img-32         Build simple 32-bit floppy image ($(IMG32))"
	@echo "  run-iso-32     Run i386 ISO in QEMU"
	@echo "  run-iso-x86_64 Run x86_64 ISO in QEMU"
	@echo "  run-img-32     Run floppy image in QEMU"
	@echo "  clean          Remove build directory"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/modules/i386/vga:
	mkdir -p $@

$(BUILD_DIR)/modules/i386/panic:
	mkdir -p $@

$(BUILD_DIR)/modules/i386/bootlog:
	mkdir -p $@

$(BUILD_DIR)/modules/x86_64/vga:
	mkdir -p $@

$(BUILD_DIR)/modules/x86_64/panic:
	mkdir -p $@

$(BUILD_DIR)/modules/x86_64/bootlog:
	mkdir -p $@

$(MOD_VGA_OBJ_32): $(MOD_VGA_SRC) kernel/modules/vga/vga.h include/types.h | $(BUILD_DIR)/modules/i386/vga
	$(I386_CC) $(CFLAGS_COMMON) $(INCLUDES) -m32 -c $(MOD_VGA_SRC) -o $@

$(MOD_PANIC_OBJ_32): $(MOD_PANIC_SRC) kernel/modules/panic/panic.h kernel/modules/vga/vga.h | $(BUILD_DIR)/modules/i386/panic
	$(I386_CC) $(CFLAGS_COMMON) $(INCLUDES) -m32 -c $(MOD_PANIC_SRC) -o $@

$(MOD_BOOTLOG_OBJ_32): $(MOD_BOOTLOG_SRC) kernel/modules/bootlog/bootlog.h kernel/modules/vga/vga.h | $(BUILD_DIR)/modules/i386/bootlog
	$(I386_CC) $(CFLAGS_COMMON) $(INCLUDES) -m32 -c $(MOD_BOOTLOG_SRC) -o $@

$(MOD_VGA_OBJ_64): $(MOD_VGA_SRC) kernel/modules/vga/vga.h include/types.h | $(BUILD_DIR)/modules/x86_64/vga
	$(X64_CC) $(CFLAGS_COMMON) $(INCLUDES) -m64 -c $(MOD_VGA_SRC) -o $@

$(MOD_PANIC_OBJ_64): $(MOD_PANIC_SRC) kernel/modules/panic/panic.h kernel/modules/vga/vga.h | $(BUILD_DIR)/modules/x86_64/panic
	$(X64_CC) $(CFLAGS_COMMON) $(INCLUDES) -m64 -c $(MOD_PANIC_SRC) -o $@

$(MOD_BOOTLOG_OBJ_64): $(MOD_BOOTLOG_SRC) kernel/modules/bootlog/bootlog.h kernel/modules/vga/vga.h | $(BUILD_DIR)/modules/x86_64/bootlog
	$(X64_CC) $(CFLAGS_COMMON) $(INCLUDES) -m64 -c $(MOD_BOOTLOG_SRC) -o $@

$(MOD_VGA_LIB_32): $(MOD_VGA_OBJ_32)
	$(AR) rcs $@ $<

$(MOD_PANIC_LIB_32): $(MOD_PANIC_OBJ_32)
	$(AR) rcs $@ $<

$(MOD_BOOTLOG_LIB_32): $(MOD_BOOTLOG_OBJ_32)
	$(AR) rcs $@ $<

$(MOD_VGA_LIB_64): $(MOD_VGA_OBJ_64)
	$(AR) rcs $@ $<

$(MOD_PANIC_LIB_64): $(MOD_PANIC_OBJ_64)
	$(AR) rcs $@ $<

$(MOD_BOOTLOG_LIB_64): $(MOD_BOOTLOG_OBJ_64)
	$(AR) rcs $@ $<

modules: $(MODULE_LIBS_32) $(MODULE_LIBS_64)

$(KERNEL32): $(MB2_SRC) $(ENTRY32_SRC) $(KMAIN32_SRC) $(LDS32) $(MODULE_LIBS_32) | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS_COMMON) $(INCLUDES) -m32 -c $(MB2_SRC) -o $(BUILD_DIR)/mb2_32.o
	$(I386_CC) $(CFLAGS_COMMON) $(INCLUDES) -m32 -c $(ENTRY32_SRC) -o $(BUILD_DIR)/entry_i386.o
	$(I386_CC) $(CFLAGS_COMMON) $(INCLUDES) -m32 -c $(KMAIN32_SRC) -o $(BUILD_DIR)/kmain_i386.o
	$(I386_LD) -m elf_i386 -T $(LDS32) -o $@ \
		$(BUILD_DIR)/mb2_32.o $(BUILD_DIR)/entry_i386.o $(BUILD_DIR)/kmain_i386.o \
		$(MODULE_LIBS_32)

$(KERNEL64): $(MB2_SRC) $(ENTRY64_SRC) $(KMAIN64_SRC) $(LDS64) $(MODULE_LIBS_64) | $(BUILD_DIR)
	$(X64_CC) $(CFLAGS_COMMON) $(INCLUDES) -m64 -c $(MB2_SRC) -o $(BUILD_DIR)/mb2_64.o
	$(X64_CC) $(CFLAGS_COMMON) $(INCLUDES) -m64 -c $(ENTRY64_SRC) -o $(BUILD_DIR)/entry_x64.o
	$(X64_CC) $(CFLAGS_COMMON) $(INCLUDES) -m64 -c $(KMAIN64_SRC) -o $(BUILD_DIR)/kmain_x64.o
	$(X64_LD) -m elf_x86_64 -T $(LDS64) -o $@ \
		$(BUILD_DIR)/mb2_64.o $(BUILD_DIR)/entry_x64.o $(BUILD_DIR)/kmain_x64.o \
		$(MODULE_LIBS_64)

$(ISO32): $(KERNEL32) scripts/mkiso-i386.sh
	chmod +x scripts/mkiso-i386.sh
	./scripts/mkiso-i386.sh

$(ISO64): $(KERNEL64) scripts/mkiso-x86_64.sh
	chmod +x scripts/mkiso-x86_64.sh
	./scripts/mkiso-x86_64.sh

iso-32: $(ISO32)

iso-x86_64: $(ISO64)

img-32:
	@echo "Target img-32 is reserved for simple bootloader pipeline."
	@echo "Add scripts/mkimg-32.sh and boot/simple32/*, then replace this recipe."
	@exit 1

run-iso-32: $(ISO32)
	$(QEMU32) -cdrom $(ISO32)

run-iso-x86_64: $(ISO64)
	$(QEMU64) -cdrom $(ISO64)

run-img-32: $(IMG32)
	$(QEMU32) -fda $(IMG32)

clean:
	rm -rf $(BUILD_DIR)
