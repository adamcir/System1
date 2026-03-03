I386_CC      ?= $(shell command -v i686-elf-gcc 2>/dev/null || command -v i686-linux-gnu-gcc 2>/dev/null)
I386_LD      ?= $(shell command -v i686-elf-ld 2>/dev/null || command -v i686-linux-gnu-ld 2>/dev/null)
I386_OBJCOPY ?= $(shell command -v i686-elf-objcopy 2>/dev/null || command -v i686-linux-gnu-objcopy 2>/dev/null || command -v objcopy 2>/dev/null)
X64_CC       ?= $(shell command -v x86_64-elf-gcc 2>/dev/null || command -v x86_64-linux-gnu-gcc 2>/dev/null)
X64_LD       ?= $(shell command -v x86_64-elf-ld 2>/dev/null || command -v x86_64-linux-gnu-ld 2>/dev/null)
AR           ?= ar
QEMU32       ?= qemu-system-i386
QEMU64       ?= qemu-system-x86_64

BUILD_DIR := build

CFLAGS_COMMON := -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc -Wall -Wextra

KDIR_I386 := kernel/i386
KDIR_X64  := kernel/x86_64
KDIR_FLP  := kernel/i386-floppy

MB2_SRC        := boot/multiboot2_header.S
ENTRY32_SRC    := arch/i386/entry_i386.S
ENTRY64_SRC    := arch/x86_64/entry_x86_64_bridge.S
ENTRYFLP_SRC   := arch/i386/entry_floppy_i386.S
LDS32          := linker/linker.i386.ld
LDS64          := linker/linker.x86_64.ld
LDSFLP         := linker/linker.floppy.i386.ld

KERNEL32_ELF := $(BUILD_DIR)/kernel32.elf
KERNEL64_ELF := $(BUILD_DIR)/kernel64.elf
KERNELFLP_ELF := $(BUILD_DIR)/kernel32-floppy.elf
KERNELFLP_BIN := $(BUILD_DIR)/kernel32-floppy.bin

ISO32 := $(BUILD_DIR)/system1-iso-32.iso
ISO64 := $(BUILD_DIR)/system1-iso-x86_64.iso
IMG32 := $(BUILD_DIR)/system1-img-32.img

I386_INCLUDES := -Iinclude -I$(KDIR_I386)/modules/vga -I$(KDIR_I386)/modules/panic -I$(KDIR_I386)/modules/bootlog
X64_INCLUDES  := -Iinclude -I$(KDIR_X64)/modules/vga -I$(KDIR_X64)/modules/panic -I$(KDIR_X64)/modules/bootlog
FLP_INCLUDES  := -Iinclude -I$(KDIR_FLP)/modules/vga -I$(KDIR_FLP)/modules/panic -I$(KDIR_FLP)/modules/bootlog

I386_MOD_VGA_SRC := $(KDIR_I386)/modules/vga/vga.c
I386_MOD_PANIC_SRC := $(KDIR_I386)/modules/panic/panic.c
I386_MOD_BOOTLOG_SRC := $(KDIR_I386)/modules/bootlog/bootlog.c

X64_MOD_VGA_SRC := $(KDIR_X64)/modules/vga/vga.c
X64_MOD_PANIC_SRC := $(KDIR_X64)/modules/panic/panic.c
X64_MOD_BOOTLOG_SRC := $(KDIR_X64)/modules/bootlog/bootlog.c

FLP_MOD_VGA_SRC := $(KDIR_FLP)/modules/vga/vga.c
FLP_MOD_PANIC_SRC := $(KDIR_FLP)/modules/panic/panic.c
FLP_MOD_BOOTLOG_SRC := $(KDIR_FLP)/modules/bootlog/bootlog.c

I386_MOD_VGA_LIB := $(BUILD_DIR)/modules/i386/vga/libvga.a
I386_MOD_PANIC_LIB := $(BUILD_DIR)/modules/i386/panic/libpanic.a
I386_MOD_BOOTLOG_LIB := $(BUILD_DIR)/modules/i386/bootlog/libbootlog.a
I386_MODULE_LIBS := $(I386_MOD_VGA_LIB) $(I386_MOD_PANIC_LIB) $(I386_MOD_BOOTLOG_LIB)

X64_MOD_VGA_LIB := $(BUILD_DIR)/modules/x86_64/vga/libvga.a
X64_MOD_PANIC_LIB := $(BUILD_DIR)/modules/x86_64/panic/libpanic.a
X64_MOD_BOOTLOG_LIB := $(BUILD_DIR)/modules/x86_64/bootlog/libbootlog.a
X64_MODULE_LIBS := $(X64_MOD_VGA_LIB) $(X64_MOD_PANIC_LIB) $(X64_MOD_BOOTLOG_LIB)

FLP_MOD_VGA_LIB := $(BUILD_DIR)/modules/i386-floppy/vga/libvga.a
FLP_MOD_PANIC_LIB := $(BUILD_DIR)/modules/i386-floppy/panic/libpanic.a
FLP_MOD_BOOTLOG_LIB := $(BUILD_DIR)/modules/i386-floppy/bootlog/libbootlog.a
FLP_MODULE_LIBS := $(FLP_MOD_VGA_LIB) $(FLP_MOD_PANIC_LIB) $(FLP_MOD_BOOTLOG_LIB)

.PHONY: all help modules-i386 modules-x86_64 modules-i386-floppy iso-32 iso-64 iso-x86_64 floppy-kernel32 img-32 run-iso-32 run-iso-64 run-iso-x86_64 run-img-32 clean

all: iso-32 iso-64 img-32

help:
	@echo "Targets:"
	@echo "  iso-32            Build i386 GRUB ISO ($(ISO32))"
	@echo "  iso-64            Build x86_64 GRUB ISO ($(ISO64))"
	@echo "  img-32            Build i386 floppy image ($(IMG32))"
	@echo "  run-iso-32        Run i386 ISO in QEMU"
	@echo "  run-iso-64        Run x86_64 ISO in QEMU"
	@echo "  run-img-32        Run floppy image in QEMU"
	@echo "  modules-i386      Build i386 module libs"
	@echo "  modules-x86_64    Build x86_64 module libs"
	@echo "  modules-i386-floppy Build i386-floppy module libs"
	@echo "  clean             Remove build directory"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/modules/i386/vga $(BUILD_DIR)/modules/i386/panic $(BUILD_DIR)/modules/i386/bootlog \
$(BUILD_DIR)/modules/x86_64/vga $(BUILD_DIR)/modules/x86_64/panic $(BUILD_DIR)/modules/x86_64/bootlog \
$(BUILD_DIR)/modules/i386-floppy/vga $(BUILD_DIR)/modules/i386-floppy/panic $(BUILD_DIR)/modules/i386-floppy/bootlog:
	mkdir -p $@

$(BUILD_DIR)/modules/i386/vga/vga.o: $(I386_MOD_VGA_SRC) $(KDIR_I386)/modules/vga/vga.h include/types.h | $(BUILD_DIR)/modules/i386/vga
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(BUILD_DIR)/modules/i386/panic/panic.o: $(I386_MOD_PANIC_SRC) $(KDIR_I386)/modules/panic/panic.h $(KDIR_I386)/modules/vga/vga.h | $(BUILD_DIR)/modules/i386/panic
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(BUILD_DIR)/modules/i386/bootlog/bootlog.o: $(I386_MOD_BOOTLOG_SRC) $(KDIR_I386)/modules/bootlog/bootlog.h $(KDIR_I386)/modules/vga/vga.h | $(BUILD_DIR)/modules/i386/bootlog
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_MOD_VGA_LIB): $(BUILD_DIR)/modules/i386/vga/vga.o
	$(AR) rcs $@ $<

$(I386_MOD_PANIC_LIB): $(BUILD_DIR)/modules/i386/panic/panic.o
	$(AR) rcs $@ $<

$(I386_MOD_BOOTLOG_LIB): $(BUILD_DIR)/modules/i386/bootlog/bootlog.o
	$(AR) rcs $@ $<

modules-i386: $(I386_MODULE_LIBS)

$(BUILD_DIR)/modules/x86_64/vga/vga.o: $(X64_MOD_VGA_SRC) $(KDIR_X64)/modules/vga/vga.h include/types.h | $(BUILD_DIR)/modules/x86_64/vga
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -c $< -o $@

$(BUILD_DIR)/modules/x86_64/panic/panic.o: $(X64_MOD_PANIC_SRC) $(KDIR_X64)/modules/panic/panic.h $(KDIR_X64)/modules/vga/vga.h | $(BUILD_DIR)/modules/x86_64/panic
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -c $< -o $@

$(BUILD_DIR)/modules/x86_64/bootlog/bootlog.o: $(X64_MOD_BOOTLOG_SRC) $(KDIR_X64)/modules/bootlog/bootlog.h $(KDIR_X64)/modules/vga/vga.h | $(BUILD_DIR)/modules/x86_64/bootlog
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -c $< -o $@

$(X64_MOD_VGA_LIB): $(BUILD_DIR)/modules/x86_64/vga/vga.o
	$(AR) rcs $@ $<

$(X64_MOD_PANIC_LIB): $(BUILD_DIR)/modules/x86_64/panic/panic.o
	$(AR) rcs $@ $<

$(X64_MOD_BOOTLOG_LIB): $(BUILD_DIR)/modules/x86_64/bootlog/bootlog.o
	$(AR) rcs $@ $<

modules-x86_64: $(X64_MODULE_LIBS)

$(BUILD_DIR)/modules/i386-floppy/vga/vga.o: $(FLP_MOD_VGA_SRC) $(KDIR_FLP)/modules/vga/vga.h include/types.h | $(BUILD_DIR)/modules/i386-floppy/vga
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(BUILD_DIR)/modules/i386-floppy/panic/panic.o: $(FLP_MOD_PANIC_SRC) $(KDIR_FLP)/modules/panic/panic.h $(KDIR_FLP)/modules/vga/vga.h | $(BUILD_DIR)/modules/i386-floppy/panic
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(BUILD_DIR)/modules/i386-floppy/bootlog/bootlog.o: $(FLP_MOD_BOOTLOG_SRC) $(KDIR_FLP)/modules/bootlog/bootlog.h $(KDIR_FLP)/modules/vga/vga.h | $(BUILD_DIR)/modules/i386-floppy/bootlog
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_MOD_VGA_LIB): $(BUILD_DIR)/modules/i386-floppy/vga/vga.o
	$(AR) rcs $@ $<

$(FLP_MOD_PANIC_LIB): $(BUILD_DIR)/modules/i386-floppy/panic/panic.o
	$(AR) rcs $@ $<

$(FLP_MOD_BOOTLOG_LIB): $(BUILD_DIR)/modules/i386-floppy/bootlog/bootlog.o
	$(AR) rcs $@ $<

modules-i386-floppy: $(FLP_MODULE_LIBS)

$(KERNEL32_ELF): $(MB2_SRC) $(ENTRY32_SRC) $(KDIR_I386)/kernel.c $(LDS32) $(I386_MODULE_LIBS) | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $(MB2_SRC) -o $(BUILD_DIR)/mb2_32.o
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $(ENTRY32_SRC) -o $(BUILD_DIR)/entry_i386.o
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $(KDIR_I386)/kernel.c -o $(BUILD_DIR)/kernel_i386.o
	$(I386_LD) -m elf_i386 -T $(LDS32) -o $@ \
		$(BUILD_DIR)/mb2_32.o $(BUILD_DIR)/entry_i386.o $(BUILD_DIR)/kernel_i386.o $(I386_MODULE_LIBS)

$(KERNEL64_ELF): $(MB2_SRC) $(ENTRY64_SRC) $(KDIR_X64)/kernel.c $(LDS64) $(X64_MODULE_LIBS) | $(BUILD_DIR)
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -c $(MB2_SRC) -o $(BUILD_DIR)/mb2_64.o
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -c $(ENTRY64_SRC) -o $(BUILD_DIR)/entry_x64.o
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -c $(KDIR_X64)/kernel.c -o $(BUILD_DIR)/kernel_x64.o
	$(X64_LD) -m elf_x86_64 -T $(LDS64) -o $@ \
		$(BUILD_DIR)/mb2_64.o $(BUILD_DIR)/entry_x64.o $(BUILD_DIR)/kernel_x64.o $(X64_MODULE_LIBS)

$(KERNELFLP_ELF): $(ENTRYFLP_SRC) $(KDIR_FLP)/kernel.c $(LDSFLP) $(FLP_MODULE_LIBS) | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $(ENTRYFLP_SRC) -o $(BUILD_DIR)/entry_floppy_i386.o
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $(KDIR_FLP)/kernel.c -o $(BUILD_DIR)/kernel_i386_floppy.o
	$(I386_LD) -m elf_i386 -T $(LDSFLP) -o $@ \
		$(BUILD_DIR)/entry_floppy_i386.o $(BUILD_DIR)/kernel_i386_floppy.o $(FLP_MODULE_LIBS)

$(KERNELFLP_BIN): $(KERNELFLP_ELF)
	$(I386_OBJCOPY) -O binary $< $@

$(ISO32): $(KERNEL32_ELF) scripts/mkiso-i386.sh
	chmod +x scripts/mkiso-i386.sh
	./scripts/mkiso-i386.sh

$(ISO64): $(KERNEL64_ELF) scripts/mkiso-x86_64.sh
	chmod +x scripts/mkiso-x86_64.sh
	./scripts/mkiso-x86_64.sh

$(IMG32): $(KERNELFLP_BIN) scripts/mkimg-32.sh boot/simple32/stage1.asm boot/simple32/stage2.asm
	chmod +x scripts/mkimg-32.sh
	./scripts/mkimg-32.sh

iso-32: $(ISO32)

iso-64: $(ISO64)

iso-x86_64: $(ISO64)

floppy-kernel32: $(KERNELFLP_BIN)

img-32: $(IMG32)

run-iso-32: $(ISO32)
	$(QEMU32) -cdrom $(ISO32)

run-iso-64: $(ISO64)
	$(QEMU64) -cdrom $(ISO64)

run-iso-x86_64: $(ISO64)
	$(QEMU64) -cdrom $(ISO64)

run-img-32: $(IMG32)
	$(QEMU32) -fda $(IMG32)

clean:
	rm -rf $(BUILD_DIR)
