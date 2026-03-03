I386_CC      ?= $(shell command -v i686-elf-gcc 2>/dev/null || command -v i686-linux-gnu-gcc 2>/dev/null)
I386_LD      ?= $(shell command -v i686-elf-ld 2>/dev/null || command -v i686-linux-gnu-ld 2>/dev/null)
I386_OBJCOPY ?= $(shell command -v i686-elf-objcopy 2>/dev/null || command -v i686-linux-gnu-objcopy 2>/dev/null || command -v objcopy 2>/dev/null)
X64_CC       ?= $(shell command -v x86_64-elf-gcc 2>/dev/null || command -v x86_64-linux-gnu-gcc 2>/dev/null)
X64_LD       ?= $(shell command -v x86_64-elf-ld 2>/dev/null || command -v x86_64-linux-gnu-ld 2>/dev/null)
AR           ?= ar
QEMU32       ?= qemu-system-i386
QEMU64       ?= qemu-system-x86_64

BUILD_DIR := build
BUILD_OBJ := $(BUILD_DIR)/obj
BUILD_OUT := $(BUILD_DIR)/artifacts
IMAGE_OUT_DIR := $(BUILD_OUT)/images
I386_OUT_DIR := $(BUILD_OUT)/i386
X64_OUT_DIR := $(BUILD_OUT)/x86_64
FLP_OUT_DIR := $(BUILD_OUT)/i386-floppy

CFLAGS_COMMON := -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc -Wall -Wextra

KDIR_I386 := kernel/i386
KDIR_X64  := kernel/x86_64
KDIR_FLP  := kernel/i386-floppy

COMMON_MODULE_DIR := kernel/common/modules
COMMON_VGA_DIR := $(COMMON_MODULE_DIR)/vga
COMMON_PANIC_DIR := $(COMMON_MODULE_DIR)/panic
COMMON_BOOTLOG_DIR := $(COMMON_MODULE_DIR)/bootlog
COMMON_INTERRUPTS_DIR := $(COMMON_MODULE_DIR)/interrupts
COMMON_KEYBOARD_DIR := $(COMMON_MODULE_DIR)/keyboard
COMMON_TTY_DIR := $(COMMON_MODULE_DIR)/tty

MB2_SRC        := boot/multiboot2_header.S
ENTRY32_SRC    := arch/i386/entry_i386.S
ENTRY64_SRC    := arch/x86_64/entry_x86_64_bridge.S
ENTRYFLP_SRC   := arch/i386/entry_floppy_i386.S
ISR32_SRC      := arch/i386/isr_i386.S
ISR64_SRC      := arch/x86_64/isr_x86_64.S
ISRFLP_SRC     := arch/i386/isr_floppy_i386.S
LDS32          := linker/linker.i386.ld
LDS64          := linker/linker.x86_64.ld
LDSFLP         := linker/linker.floppy.i386.ld

KERNEL32_ELF := $(I386_OUT_DIR)/kernel.bin
KERNEL64_ELF := $(X64_OUT_DIR)/kernel.bin
KERNELFLP_ELF := $(FLP_OUT_DIR)/kernel.elf
KERNELFLP_BIN := $(FLP_OUT_DIR)/kernel.bin

ISO32 := $(IMAGE_OUT_DIR)/system1-iso-32.iso
ISO64 := $(IMAGE_OUT_DIR)/system1-iso-x86_64.iso
IMG32 := $(IMAGE_OUT_DIR)/system1-img-32.img

COMMON_INCLUDES := -Iinclude -I$(COMMON_VGA_DIR) -I$(COMMON_PANIC_DIR) -I$(COMMON_BOOTLOG_DIR) -I$(COMMON_INTERRUPTS_DIR) -I$(COMMON_KEYBOARD_DIR) -I$(COMMON_TTY_DIR)
I386_INCLUDES := $(COMMON_INCLUDES) -I$(KDIR_I386)/modules/vga -I$(KDIR_I386)/modules/panic -I$(KDIR_I386)/modules/bootlog -I$(KDIR_I386)/modules/interrupts -I$(KDIR_I386)/modules/keyboard -I$(KDIR_I386)/modules/tty
X64_INCLUDES  := $(COMMON_INCLUDES) -I$(KDIR_X64)/modules/vga -I$(KDIR_X64)/modules/panic -I$(KDIR_X64)/modules/bootlog -I$(KDIR_X64)/modules/interrupts -I$(KDIR_X64)/modules/keyboard -I$(KDIR_X64)/modules/tty
FLP_INCLUDES  := $(COMMON_INCLUDES) -I$(KDIR_FLP)/modules/vga -I$(KDIR_FLP)/modules/panic -I$(KDIR_FLP)/modules/bootlog -I$(KDIR_FLP)/modules/interrupts -I$(KDIR_FLP)/modules/keyboard -I$(KDIR_FLP)/modules/tty

I386_MOD_VGA_SRC := $(COMMON_VGA_DIR)/vga.c
I386_MOD_PANIC_SRC := $(COMMON_PANIC_DIR)/panic.c
I386_MOD_BOOTLOG_SRC := $(COMMON_BOOTLOG_DIR)/bootlog.c
I386_MOD_INTERRUPTS_SRC := $(KDIR_I386)/modules/interrupts/interrupts.c
I386_MOD_KEYBOARD_SRC := $(COMMON_KEYBOARD_DIR)/keyboard.c
I386_MOD_TTY_SRC := $(COMMON_TTY_DIR)/tty.c

X64_MOD_VGA_SRC := $(COMMON_VGA_DIR)/vga.c
X64_MOD_PANIC_SRC := $(COMMON_PANIC_DIR)/panic.c
X64_MOD_BOOTLOG_SRC := $(COMMON_BOOTLOG_DIR)/bootlog.c
X64_MOD_INTERRUPTS_SRC := $(KDIR_X64)/modules/interrupts/interrupts.c
X64_MOD_KEYBOARD_SRC := $(COMMON_KEYBOARD_DIR)/keyboard.c
X64_MOD_TTY_SRC := $(COMMON_TTY_DIR)/tty.c

FLP_MOD_VGA_SRC := $(COMMON_VGA_DIR)/vga.c
FLP_MOD_PANIC_SRC := $(COMMON_PANIC_DIR)/panic.c
FLP_MOD_BOOTLOG_SRC := $(COMMON_BOOTLOG_DIR)/bootlog.c
FLP_MOD_INTERRUPTS_SRC := $(KDIR_FLP)/modules/interrupts/interrupts.c
FLP_MOD_KEYBOARD_SRC := $(COMMON_KEYBOARD_DIR)/keyboard.c
FLP_MOD_TTY_SRC := $(COMMON_TTY_DIR)/tty.c

COMMON_VGA_CORE_SRC := $(COMMON_VGA_DIR)/vga_core.c
COMMON_VGA_CORE_HDR := $(COMMON_VGA_DIR)/vga_core.h
COMMON_PANIC_CORE_SRC := $(COMMON_PANIC_DIR)/panic_core.c
COMMON_PANIC_CORE_HDR := $(COMMON_PANIC_DIR)/panic_core.h
COMMON_BOOTLOG_CORE_SRC := $(COMMON_BOOTLOG_DIR)/bootlog_core.c
COMMON_BOOTLOG_CORE_HDR := $(COMMON_BOOTLOG_DIR)/bootlog_core.h
COMMON_INTERRUPTS_CORE_SRC := $(COMMON_INTERRUPTS_DIR)/interrupts_common.c
COMMON_INTERRUPTS_CORE_HDR := $(COMMON_INTERRUPTS_DIR)/interrupts_common.h
COMMON_KEYBOARD_CORE_SRC := $(COMMON_KEYBOARD_DIR)/keyboard_core.c
COMMON_KEYBOARD_CORE_HDR := $(COMMON_KEYBOARD_DIR)/keyboard_core.h
COMMON_TTY_CORE_SRC := $(COMMON_TTY_DIR)/tty-core.c
COMMON_TTY_CORE_HDR := $(COMMON_TTY_DIR)/tty-core.h

I386_MOD_VGA_LIB := $(I386_OUT_DIR)/modules/vga/libvga.a
I386_MOD_PANIC_LIB := $(I386_OUT_DIR)/modules/panic/libpanic.a
I386_MOD_BOOTLOG_LIB := $(I386_OUT_DIR)/modules/bootlog/libbootlog.a
I386_MOD_INTERRUPTS_LIB := $(I386_OUT_DIR)/modules/interrupts/libinterrupts.a
I386_MOD_KEYBOARD_LIB := $(I386_OUT_DIR)/modules/keyboard/libkeyboard.a
I386_MOD_TTY_LIB := $(I386_OUT_DIR)/modules/tty/libtty.a
I386_MODULE_LIBS := $(I386_MOD_VGA_LIB) $(I386_MOD_PANIC_LIB) $(I386_MOD_BOOTLOG_LIB) $(I386_MOD_INTERRUPTS_LIB) $(I386_MOD_KEYBOARD_LIB) $(I386_MOD_TTY_LIB)

X64_MOD_VGA_LIB := $(X64_OUT_DIR)/modules/vga/libvga.a
X64_MOD_PANIC_LIB := $(X64_OUT_DIR)/modules/panic/libpanic.a
X64_MOD_BOOTLOG_LIB := $(X64_OUT_DIR)/modules/bootlog/libbootlog.a
X64_MOD_INTERRUPTS_LIB := $(X64_OUT_DIR)/modules/interrupts/libinterrupts.a
X64_MOD_KEYBOARD_LIB := $(X64_OUT_DIR)/modules/keyboard/libkeyboard.a
X64_MOD_TTY_LIB := $(X64_OUT_DIR)/modules/tty/libtty.a
X64_MODULE_LIBS := $(X64_MOD_VGA_LIB) $(X64_MOD_PANIC_LIB) $(X64_MOD_BOOTLOG_LIB) $(X64_MOD_INTERRUPTS_LIB) $(X64_MOD_KEYBOARD_LIB) $(X64_MOD_TTY_LIB)

FLP_MOD_VGA_LIB := $(FLP_OUT_DIR)/modules/vga/libvga.a
FLP_MOD_PANIC_LIB := $(FLP_OUT_DIR)/modules/panic/libpanic.a
FLP_MOD_BOOTLOG_LIB := $(FLP_OUT_DIR)/modules/bootlog/libbootlog.a
FLP_MOD_INTERRUPTS_LIB := $(FLP_OUT_DIR)/modules/interrupts/libinterrupts.a
FLP_MOD_KEYBOARD_LIB := $(FLP_OUT_DIR)/modules/keyboard/libkeyboard.a
FLP_MOD_TTY_LIB := $(FLP_OUT_DIR)/modules/tty/libtty.a
FLP_MODULE_LIBS := $(FLP_MOD_VGA_LIB) $(FLP_MOD_PANIC_LIB) $(FLP_MOD_BOOTLOG_LIB) $(FLP_MOD_INTERRUPTS_LIB) $(FLP_MOD_KEYBOARD_LIB) $(FLP_MOD_TTY_LIB)

.PHONY: all help modules-i386 modules-x86_64 modules-i386-floppy iso-32 iso-64 iso-x86_64 floppy-kernel32 img-32 run-32 run-64 run-x86_64 run-img-32 clean

all: iso-32 iso-64 img-32

help:
	@echo "Targets:"
	@echo "  iso-32            Build i386 GRUB ISO ($(ISO32))"
	@echo "  iso-64            Build x86_64 GRUB ISO ($(ISO64))"
	@echo "  img-32            Build i386 floppy image ($(IMG32))"
	@echo "  run-32        Run i386 ISO in QEMU"
	@echo "  run-64        Run x86_64 ISO in QEMU"
	@echo "  run-img-32        Run floppy image in QEMU"
	@echo "  modules-i386      Build i386 module libs"
	@echo "  modules-x86_64    Build x86_64 module libs"
	@echo "  modules-i386-floppy Build i386-floppy module libs"
	@echo "  clean             Remove build directory"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_OBJ) $(IMAGE_OUT_DIR):
	mkdir -p $@

$(I386_OUT_DIR) $(X64_OUT_DIR) $(FLP_OUT_DIR):
	mkdir -p $@

$(I386_OUT_DIR)/modules/vga $(I386_OUT_DIR)/modules/panic $(I386_OUT_DIR)/modules/bootlog $(I386_OUT_DIR)/modules/interrupts $(I386_OUT_DIR)/modules/keyboard $(I386_OUT_DIR)/modules/tty \
$(X64_OUT_DIR)/modules/vga $(X64_OUT_DIR)/modules/panic $(X64_OUT_DIR)/modules/bootlog $(X64_OUT_DIR)/modules/interrupts $(X64_OUT_DIR)/modules/keyboard $(X64_OUT_DIR)/modules/tty \
$(FLP_OUT_DIR)/modules/vga $(FLP_OUT_DIR)/modules/panic $(FLP_OUT_DIR)/modules/bootlog $(FLP_OUT_DIR)/modules/interrupts $(FLP_OUT_DIR)/modules/keyboard $(FLP_OUT_DIR)/modules/tty:
	mkdir -p $@

$(I386_OUT_DIR)/modules/vga/vga.o: $(I386_MOD_VGA_SRC) $(KDIR_I386)/modules/vga/vga.h $(COMMON_VGA_CORE_HDR) include/types.h | $(I386_OUT_DIR)/modules/vga
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/vga/vga_core.o: $(COMMON_VGA_CORE_SRC) $(COMMON_VGA_CORE_HDR) include/types.h | $(I386_OUT_DIR)/modules/vga
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/panic/panic.o: $(I386_MOD_PANIC_SRC) $(KDIR_I386)/modules/panic/panic.h $(COMMON_PANIC_CORE_HDR) | $(I386_OUT_DIR)/modules/panic
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/panic/panic_core.o: $(COMMON_PANIC_CORE_SRC) $(COMMON_PANIC_CORE_HDR) | $(I386_OUT_DIR)/modules/panic
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/bootlog/bootlog.o: $(I386_MOD_BOOTLOG_SRC) $(KDIR_I386)/modules/bootlog/bootlog.h $(COMMON_BOOTLOG_CORE_HDR) | $(I386_OUT_DIR)/modules/bootlog
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -DSYSTEM1_BOOTLOG_I386 -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/bootlog/bootlog_core.o: $(COMMON_BOOTLOG_CORE_SRC) $(COMMON_BOOTLOG_CORE_HDR) | $(I386_OUT_DIR)/modules/bootlog
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/interrupts/interrupts.o: $(I386_MOD_INTERRUPTS_SRC) $(KDIR_I386)/modules/interrupts/interrupts.h $(COMMON_INTERRUPTS_CORE_HDR) | $(I386_OUT_DIR)/modules/interrupts
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/interrupts/interrupts_common.o: $(COMMON_INTERRUPTS_CORE_SRC) $(COMMON_INTERRUPTS_CORE_HDR) include/types.h | $(I386_OUT_DIR)/modules/interrupts
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/keyboard/keyboard.o: $(I386_MOD_KEYBOARD_SRC) $(KDIR_I386)/modules/keyboard/keyboard.h $(COMMON_KEYBOARD_CORE_HDR) | $(I386_OUT_DIR)/modules/keyboard
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/keyboard/keyboard_core.o: $(COMMON_KEYBOARD_CORE_SRC) $(COMMON_KEYBOARD_CORE_HDR) include/types.h | $(I386_OUT_DIR)/modules/keyboard
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/tty/tty.o: $(I386_MOD_TTY_SRC) $(KDIR_I386)/modules/tty/tty.h $(COMMON_TTY_CORE_HDR) | $(I386_OUT_DIR)/modules/tty
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_OUT_DIR)/modules/tty/tty-core.o: $(COMMON_TTY_CORE_SRC) $(COMMON_TTY_CORE_HDR) $(KDIR_I386)/modules/tty/tty.h $(KDIR_I386)/modules/keyboard/keyboard.h $(KDIR_I386)/modules/vga/vga.h | $(I386_OUT_DIR)/modules/tty
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(I386_MOD_VGA_LIB): $(I386_OUT_DIR)/modules/vga/vga.o $(I386_OUT_DIR)/modules/vga/vga_core.o
	$(AR) rcs $@ $^

$(I386_MOD_PANIC_LIB): $(I386_OUT_DIR)/modules/panic/panic.o $(I386_OUT_DIR)/modules/panic/panic_core.o
	$(AR) rcs $@ $^

$(I386_MOD_BOOTLOG_LIB): $(I386_OUT_DIR)/modules/bootlog/bootlog.o $(I386_OUT_DIR)/modules/bootlog/bootlog_core.o
	$(AR) rcs $@ $^

$(I386_MOD_INTERRUPTS_LIB): $(I386_OUT_DIR)/modules/interrupts/interrupts.o $(I386_OUT_DIR)/modules/interrupts/interrupts_common.o
	$(AR) rcs $@ $^

$(I386_MOD_KEYBOARD_LIB): $(I386_OUT_DIR)/modules/keyboard/keyboard.o $(I386_OUT_DIR)/modules/keyboard/keyboard_core.o
	$(AR) rcs $@ $^

$(I386_MOD_TTY_LIB): $(I386_OUT_DIR)/modules/tty/tty.o $(I386_OUT_DIR)/modules/tty/tty-core.o
	$(AR) rcs $@ $^

modules-i386: $(I386_MODULE_LIBS)

$(X64_OUT_DIR)/modules/vga/vga.o: $(X64_MOD_VGA_SRC) $(KDIR_X64)/modules/vga/vga.h $(COMMON_VGA_CORE_HDR) include/types.h | $(X64_OUT_DIR)/modules/vga
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/vga/vga_core.o: $(COMMON_VGA_CORE_SRC) $(COMMON_VGA_CORE_HDR) include/types.h | $(X64_OUT_DIR)/modules/vga
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/panic/panic.o: $(X64_MOD_PANIC_SRC) $(KDIR_X64)/modules/panic/panic.h $(COMMON_PANIC_CORE_HDR) | $(X64_OUT_DIR)/modules/panic
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/panic/panic_core.o: $(COMMON_PANIC_CORE_SRC) $(COMMON_PANIC_CORE_HDR) | $(X64_OUT_DIR)/modules/panic
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/bootlog/bootlog.o: $(X64_MOD_BOOTLOG_SRC) $(KDIR_X64)/modules/bootlog/bootlog.h $(COMMON_BOOTLOG_CORE_HDR) | $(X64_OUT_DIR)/modules/bootlog
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -DSYSTEM1_BOOTLOG_X86_64 -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/bootlog/bootlog_core.o: $(COMMON_BOOTLOG_CORE_SRC) $(COMMON_BOOTLOG_CORE_HDR) | $(X64_OUT_DIR)/modules/bootlog
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/interrupts/interrupts.o: $(X64_MOD_INTERRUPTS_SRC) $(KDIR_X64)/modules/interrupts/interrupts.h $(COMMON_INTERRUPTS_CORE_HDR) | $(X64_OUT_DIR)/modules/interrupts
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/interrupts/interrupts_common.o: $(COMMON_INTERRUPTS_CORE_SRC) $(COMMON_INTERRUPTS_CORE_HDR) include/types.h | $(X64_OUT_DIR)/modules/interrupts
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/keyboard/keyboard.o: $(X64_MOD_KEYBOARD_SRC) $(KDIR_X64)/modules/keyboard/keyboard.h $(COMMON_KEYBOARD_CORE_HDR) | $(X64_OUT_DIR)/modules/keyboard
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/keyboard/keyboard_core.o: $(COMMON_KEYBOARD_CORE_SRC) $(COMMON_KEYBOARD_CORE_HDR) include/types.h | $(X64_OUT_DIR)/modules/keyboard
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/tty/tty.o: $(X64_MOD_TTY_SRC) $(KDIR_X64)/modules/tty/tty.h $(COMMON_TTY_CORE_HDR) | $(X64_OUT_DIR)/modules/tty
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_OUT_DIR)/modules/tty/tty-core.o: $(COMMON_TTY_CORE_SRC) $(COMMON_TTY_CORE_HDR) $(KDIR_X64)/modules/tty/tty.h $(KDIR_X64)/modules/keyboard/keyboard.h $(KDIR_X64)/modules/vga/vga.h | $(X64_OUT_DIR)/modules/tty
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(X64_MOD_VGA_LIB): $(X64_OUT_DIR)/modules/vga/vga.o $(X64_OUT_DIR)/modules/vga/vga_core.o
	$(AR) rcs $@ $^

$(X64_MOD_PANIC_LIB): $(X64_OUT_DIR)/modules/panic/panic.o $(X64_OUT_DIR)/modules/panic/panic_core.o
	$(AR) rcs $@ $^

$(X64_MOD_BOOTLOG_LIB): $(X64_OUT_DIR)/modules/bootlog/bootlog.o $(X64_OUT_DIR)/modules/bootlog/bootlog_core.o
	$(AR) rcs $@ $^

$(X64_MOD_INTERRUPTS_LIB): $(X64_OUT_DIR)/modules/interrupts/interrupts.o $(X64_OUT_DIR)/modules/interrupts/interrupts_common.o
	$(AR) rcs $@ $^

$(X64_MOD_KEYBOARD_LIB): $(X64_OUT_DIR)/modules/keyboard/keyboard.o $(X64_OUT_DIR)/modules/keyboard/keyboard_core.o
	$(AR) rcs $@ $^

$(X64_MOD_TTY_LIB): $(X64_OUT_DIR)/modules/tty/tty.o $(X64_OUT_DIR)/modules/tty/tty-core.o
	$(AR) rcs $@ $^

modules-x86_64: $(X64_MODULE_LIBS)

$(FLP_OUT_DIR)/modules/vga/vga.o: $(FLP_MOD_VGA_SRC) $(KDIR_FLP)/modules/vga/vga.h $(COMMON_VGA_CORE_HDR) include/types.h | $(FLP_OUT_DIR)/modules/vga
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/vga/vga_core.o: $(COMMON_VGA_CORE_SRC) $(COMMON_VGA_CORE_HDR) include/types.h | $(FLP_OUT_DIR)/modules/vga
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/panic/panic.o: $(FLP_MOD_PANIC_SRC) $(KDIR_FLP)/modules/panic/panic.h $(COMMON_PANIC_CORE_HDR) | $(FLP_OUT_DIR)/modules/panic
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/panic/panic_core.o: $(COMMON_PANIC_CORE_SRC) $(COMMON_PANIC_CORE_HDR) | $(FLP_OUT_DIR)/modules/panic
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/bootlog/bootlog.o: $(FLP_MOD_BOOTLOG_SRC) $(KDIR_FLP)/modules/bootlog/bootlog.h $(COMMON_BOOTLOG_CORE_HDR) | $(FLP_OUT_DIR)/modules/bootlog
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -DSYSTEM1_BOOTLOG_I386_FLOPPY -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/bootlog/bootlog_core.o: $(COMMON_BOOTLOG_CORE_SRC) $(COMMON_BOOTLOG_CORE_HDR) | $(FLP_OUT_DIR)/modules/bootlog
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/interrupts/interrupts.o: $(FLP_MOD_INTERRUPTS_SRC) $(KDIR_FLP)/modules/interrupts/interrupts.h $(COMMON_INTERRUPTS_CORE_HDR) | $(FLP_OUT_DIR)/modules/interrupts
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/interrupts/interrupts_common.o: $(COMMON_INTERRUPTS_CORE_SRC) $(COMMON_INTERRUPTS_CORE_HDR) include/types.h | $(FLP_OUT_DIR)/modules/interrupts
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/keyboard/keyboard.o: $(FLP_MOD_KEYBOARD_SRC) $(KDIR_FLP)/modules/keyboard/keyboard.h $(COMMON_KEYBOARD_CORE_HDR) | $(FLP_OUT_DIR)/modules/keyboard
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/keyboard/keyboard_core.o: $(COMMON_KEYBOARD_CORE_SRC) $(COMMON_KEYBOARD_CORE_HDR) include/types.h | $(FLP_OUT_DIR)/modules/keyboard
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/tty/tty.o: $(FLP_MOD_TTY_SRC) $(KDIR_FLP)/modules/tty/tty.h $(COMMON_TTY_CORE_HDR) | $(FLP_OUT_DIR)/modules/tty
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_OUT_DIR)/modules/tty/tty-core.o: $(COMMON_TTY_CORE_SRC) $(COMMON_TTY_CORE_HDR) $(KDIR_FLP)/modules/tty/tty.h $(KDIR_FLP)/modules/keyboard/keyboard.h $(KDIR_FLP)/modules/vga/vga.h | $(FLP_OUT_DIR)/modules/tty
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(FLP_MOD_VGA_LIB): $(FLP_OUT_DIR)/modules/vga/vga.o $(FLP_OUT_DIR)/modules/vga/vga_core.o
	$(AR) rcs $@ $^

$(FLP_MOD_PANIC_LIB): $(FLP_OUT_DIR)/modules/panic/panic.o $(FLP_OUT_DIR)/modules/panic/panic_core.o
	$(AR) rcs $@ $^

$(FLP_MOD_BOOTLOG_LIB): $(FLP_OUT_DIR)/modules/bootlog/bootlog.o $(FLP_OUT_DIR)/modules/bootlog/bootlog_core.o
	$(AR) rcs $@ $^

$(FLP_MOD_INTERRUPTS_LIB): $(FLP_OUT_DIR)/modules/interrupts/interrupts.o $(FLP_OUT_DIR)/modules/interrupts/interrupts_common.o
	$(AR) rcs $@ $^

$(FLP_MOD_KEYBOARD_LIB): $(FLP_OUT_DIR)/modules/keyboard/keyboard.o $(FLP_OUT_DIR)/modules/keyboard/keyboard_core.o
	$(AR) rcs $@ $^

$(FLP_MOD_TTY_LIB): $(FLP_OUT_DIR)/modules/tty/tty.o $(FLP_OUT_DIR)/modules/tty/tty-core.o
	$(AR) rcs $@ $^

modules-i386-floppy: $(FLP_MODULE_LIBS)

$(KERNEL32_ELF): $(MB2_SRC) $(ENTRY32_SRC) $(ISR32_SRC) $(KDIR_I386)/kernel.c $(LDS32) $(I386_MODULE_LIBS) | $(BUILD_OBJ) $(BUILD_OUT)/i386
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $(MB2_SRC) -o $(BUILD_OBJ)/mb2_32.o
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $(ENTRY32_SRC) -o $(BUILD_OBJ)/entry_i386.o
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $(ISR32_SRC) -o $(BUILD_OBJ)/isr_i386.o
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $(KDIR_I386)/kernel.c -o $(BUILD_OBJ)/kernel_i386.o
	$(I386_LD) -m elf_i386 -T $(LDS32) -o $@ \
		$(BUILD_OBJ)/mb2_32.o $(BUILD_OBJ)/entry_i386.o $(BUILD_OBJ)/isr_i386.o $(BUILD_OBJ)/kernel_i386.o $(I386_MODULE_LIBS)

$(KERNEL64_ELF): $(MB2_SRC) $(ENTRY64_SRC) $(ISR64_SRC) $(KDIR_X64)/kernel.c $(LDS64) $(X64_MODULE_LIBS) | $(BUILD_OBJ) $(BUILD_OUT)/x86_64
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $(MB2_SRC) -o $(BUILD_OBJ)/mb2_64.o
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $(ENTRY64_SRC) -o $(BUILD_OBJ)/entry_x64.o
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $(ISR64_SRC) -o $(BUILD_OBJ)/isr_x64.o
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $(KDIR_X64)/kernel.c -o $(BUILD_OBJ)/kernel_x64.o
	$(X64_LD) -m elf_x86_64 -T $(LDS64) -o $@ \
		$(BUILD_OBJ)/mb2_64.o $(BUILD_OBJ)/entry_x64.o $(BUILD_OBJ)/isr_x64.o $(BUILD_OBJ)/kernel_x64.o $(X64_MODULE_LIBS)

$(KERNELFLP_ELF): $(ENTRYFLP_SRC) $(ISRFLP_SRC) $(KDIR_FLP)/kernel.c $(LDSFLP) $(FLP_MODULE_LIBS) | $(BUILD_OBJ) $(BUILD_OUT)/i386-floppy
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $(ENTRYFLP_SRC) -o $(BUILD_OBJ)/entry_floppy_i386.o
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $(ISRFLP_SRC) -o $(BUILD_OBJ)/isr_floppy_i386.o
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $(KDIR_FLP)/kernel.c -o $(BUILD_OBJ)/kernel_i386_floppy.o
	$(I386_LD) -m elf_i386 -T $(LDSFLP) -o $@ \
		$(BUILD_OBJ)/entry_floppy_i386.o $(BUILD_OBJ)/isr_floppy_i386.o $(BUILD_OBJ)/kernel_i386_floppy.o $(FLP_MODULE_LIBS)

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

run-32: $(ISO32)
	$(QEMU32) -cdrom $(ISO32)

run-64: $(ISO64)
	$(QEMU64) -cdrom $(ISO64)

run-x86_64: $(ISO64)
	$(QEMU64) -cdrom $(ISO64)

run-img-32: $(IMG32)
	$(QEMU32) -fda $(IMG32)

clean:
	rm -rf $(BUILD_DIR)
