I386_CC      ?= $(shell command -v i686-elf-gcc 2>/dev/null || command -v i686-linux-gnu-gcc 2>/dev/null || command -v gcc 2>/dev/null)
I386_LD      ?= $(shell command -v i686-elf-ld 2>/dev/null || command -v i686-linux-gnu-ld 2>/dev/null || command -v ld 2>/dev/null)
I386_OBJCOPY ?= $(shell command -v i686-elf-objcopy 2>/dev/null || command -v i686-linux-gnu-objcopy 2>/dev/null || command -v objcopy 2>/dev/null)
X64_CC       ?= $(shell command -v x86_64-elf-gcc 2>/dev/null || command -v x86_64-linux-gnu-gcc 2>/dev/null || command -v gcc 2>/dev/null)
X64_LD       ?= $(shell command -v x86_64-elf-ld 2>/dev/null || command -v x86_64-linux-gnu-ld 2>/dev/null || command -v ld 2>/dev/null)
QEMU32       ?= qemu-system-i386 -m 1M
QEMU64       ?= qemu-system-x86_64 -m 1M

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

MODULES ?= $(sort $(notdir $(wildcard $(COMMON_MODULE_DIR)/*)))

MB2_SRC      := boot/multiboot2_header.S
ENTRY32_SRC  := arch/i386/entry_i386.S
ENTRY64_SRC  := arch/x86_64/entry_x86_64_bridge.S
ENTRYFLP_SRC := arch/i386/entry_floppy_i386.S
ISR32_SRC    := arch/i386/isr_i386.S
ISR64_SRC    := arch/x86_64/isr_x86_64.S
ISRFLP_SRC   := arch/i386/isr_floppy_i386.S
LDS32        := linker/linker.i386.ld
LDS64        := linker/linker.x86_64.ld
LDSFLP       := linker/linker.floppy.i386.ld

KERNEL32_ELF  := $(I386_OUT_DIR)/kernel.elf
KERNEL64_ELF  := $(X64_OUT_DIR)/kernel.elf
KERNELFLP_ELF := $(FLP_OUT_DIR)/kernel.elf

ISO32 := $(IMAGE_OUT_DIR)/system1-iso-32.iso
ISO64 := $(IMAGE_OUT_DIR)/system1-iso-x86_64.iso
IMG32 := $(IMAGE_OUT_DIR)/system1-img-32.img

COMMON_MODULE_INCLUDES := $(foreach m,$(MODULES),-I$(COMMON_MODULE_DIR)/$(m))
I386_MODULE_INCLUDES := $(foreach m,$(MODULES),-I$(KDIR_I386)/modules/$(m))
X64_MODULE_INCLUDES := $(foreach m,$(MODULES),-I$(KDIR_X64)/modules/$(m))
FLP_MODULE_INCLUDES := $(foreach m,$(MODULES),-I$(KDIR_FLP)/modules/$(m))

I386_INCLUDES := -Iinclude $(COMMON_MODULE_INCLUDES) $(I386_MODULE_INCLUDES)
X64_INCLUDES  := -Iinclude $(COMMON_MODULE_INCLUDES) $(X64_MODULE_INCLUDES)
FLP_INCLUDES  := -Iinclude $(COMMON_MODULE_INCLUDES) $(FLP_MODULE_INCLUDES)

define module_wrapper_src
$(if $(wildcard $(1)/modules/$(2)/$(2).c),$(1)/modules/$(2)/$(2).c,$(COMMON_MODULE_DIR)/$(2)/$(2).c)
endef

define module_core_src
$(wildcard \
$(COMMON_MODULE_DIR)/$(1)/*_core.c \
$(COMMON_MODULE_DIR)/$(1)/*-core.c \
$(COMMON_MODULE_DIR)/$(1)/*_common.c)
endef

define module_all_srcs
$(foreach m,$(MODULES),$(call module_wrapper_src,$(1),$(m)) $(call module_core_src,$(m)))
endef

I386_MODULE_SRCS := $(call module_all_srcs,$(KDIR_I386))
X64_MODULE_SRCS  := $(call module_all_srcs,$(KDIR_X64))
FLP_MODULE_SRCS  := $(call module_all_srcs,$(KDIR_FLP))

I386_MODULE_OBJS := $(patsubst %.c,$(BUILD_OBJ)/i386/%.o,$(I386_MODULE_SRCS))
X64_MODULE_OBJS  := $(patsubst %.c,$(BUILD_OBJ)/x86_64/%.o,$(X64_MODULE_SRCS))
FLP_MODULE_OBJS  := $(patsubst %.c,$(BUILD_OBJ)/i386-floppy/%.o,$(FLP_MODULE_SRCS))

.PHONY: all help modules-32 modules-64 modules-img-32 iso-32 iso-64 iso-x86_64 floppy-kernel32 img-32 run-32 run-64 run-x86_64 run-img-32 clean

all: iso-32 iso-64 img-32

help:
	@echo "Targets:"
	@echo "  iso-32            Build i386 GRUB ISO ($(ISO32))"
	@echo "  iso-64            Build x86_64 GRUB ISO ($(ISO64))"
	@echo "  img-32            Build i386 floppy image ($(IMG32))"
	@echo "  run-32            Run i386 ISO in QEMU"
	@echo "  run-64            Run x86_64 ISO in QEMU"
	@echo "  run-img-32        Run floppy image in QEMU"
	@echo "  modules-32      Build i386 module objects"
	@echo "  modules-64    Build x86_64 module objects"
	@echo "  modules-img-32 Build i386-floppy module objects"
	@echo "  clean             Remove build directory"
	@echo ""
	@echo "Config:"
	@echo "  MODULES='$(MODULES)'"

$(BUILD_OBJ) $(IMAGE_OUT_DIR) $(I386_OUT_DIR) $(X64_OUT_DIR) $(FLP_OUT_DIR):
	mkdir -p $@

$(BUILD_OBJ)/i386/%.o: %.c | $(BUILD_OBJ)
	mkdir -p $(dir $@)
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(BUILD_OBJ)/x86_64/%.o: %.c | $(BUILD_OBJ)
	mkdir -p $(dir $@)
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(BUILD_OBJ)/i386-floppy/%.o: %.c | $(BUILD_OBJ)
	mkdir -p $(dir $@)
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

modules-32: $(I386_MODULE_OBJS)

modules-64: $(X64_MODULE_OBJS)

modules-img-32: $(FLP_MODULE_OBJS)

$(BUILD_OBJ)/mb2_32.o: $(MB2_SRC) | $(BUILD_OBJ)
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(BUILD_OBJ)/entry_i386.o: $(ENTRY32_SRC) | $(BUILD_OBJ)
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(BUILD_OBJ)/isr_i386.o: $(ISR32_SRC) | $(BUILD_OBJ)
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(BUILD_OBJ)/kernel_i386.o: $(KDIR_I386)/kernel.c | $(BUILD_OBJ)
	$(I386_CC) $(CFLAGS_COMMON) $(I386_INCLUDES) -m32 -c $< -o $@

$(BUILD_OBJ)/mb2_64.o: $(MB2_SRC) | $(BUILD_OBJ)
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(BUILD_OBJ)/entry_x64.o: $(ENTRY64_SRC) | $(BUILD_OBJ)
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(BUILD_OBJ)/isr_x64.o: $(ISR64_SRC) | $(BUILD_OBJ)
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(BUILD_OBJ)/kernel_x64.o: $(KDIR_X64)/kernel.c | $(BUILD_OBJ)
	$(X64_CC) $(CFLAGS_COMMON) $(X64_INCLUDES) -m64 -mno-red-zone -c $< -o $@

$(BUILD_OBJ)/entry_floppy_i386.o: $(ENTRYFLP_SRC) | $(BUILD_OBJ)
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(BUILD_OBJ)/isr_floppy_i386.o: $(ISRFLP_SRC) | $(BUILD_OBJ)
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(BUILD_OBJ)/kernel_i386_floppy.o: $(KDIR_FLP)/kernel.c | $(BUILD_OBJ)
	$(I386_CC) $(CFLAGS_COMMON) $(FLP_INCLUDES) -m32 -c $< -o $@

$(KERNEL32_ELF): $(LDS32) $(BUILD_OBJ)/mb2_32.o $(BUILD_OBJ)/entry_i386.o $(BUILD_OBJ)/isr_i386.o $(BUILD_OBJ)/kernel_i386.o $(I386_MODULE_OBJS) | $(I386_OUT_DIR)
	$(I386_LD) -m elf_i386 -T $(LDS32) -o $@ \
		$(BUILD_OBJ)/mb2_32.o $(BUILD_OBJ)/entry_i386.o $(BUILD_OBJ)/isr_i386.o $(BUILD_OBJ)/kernel_i386.o $(I386_MODULE_OBJS)

$(KERNEL64_ELF): $(LDS64) $(BUILD_OBJ)/mb2_64.o $(BUILD_OBJ)/entry_x64.o $(BUILD_OBJ)/isr_x64.o $(BUILD_OBJ)/kernel_x64.o $(X64_MODULE_OBJS) | $(X64_OUT_DIR)
	$(X64_LD) -m elf_x86_64 -T $(LDS64) -o $@ \
		$(BUILD_OBJ)/mb2_64.o $(BUILD_OBJ)/entry_x64.o $(BUILD_OBJ)/isr_x64.o $(BUILD_OBJ)/kernel_x64.o $(X64_MODULE_OBJS)

$(KERNELFLP_ELF): $(LDSFLP) $(BUILD_OBJ)/entry_floppy_i386.o $(BUILD_OBJ)/isr_floppy_i386.o $(BUILD_OBJ)/kernel_i386_floppy.o $(FLP_MODULE_OBJS) | $(FLP_OUT_DIR)
	$(I386_LD) -m elf_i386 -T $(LDSFLP) -o $@ \
		$(BUILD_OBJ)/entry_floppy_i386.o $(BUILD_OBJ)/isr_floppy_i386.o $(BUILD_OBJ)/kernel_i386_floppy.o $(FLP_MODULE_OBJS)

$(ISO32): $(KERNEL32_ELF) scripts/mkiso-i386.sh | $(IMAGE_OUT_DIR)
	chmod +x scripts/mkiso-i386.sh
	./scripts/mkiso-i386.sh

$(ISO64): $(KERNEL64_ELF) scripts/mkiso-x86_64.sh | $(IMAGE_OUT_DIR)
	chmod +x scripts/mkiso-x86_64.sh
	./scripts/mkiso-x86_64.sh

$(IMG32): $(KERNELFLP_ELF) scripts/mkimg-32.sh boot/simple32/stage1.asm boot/simple32/stage2.asm | $(IMAGE_OUT_DIR)
	chmod +x scripts/mkimg-32.sh
	./scripts/mkimg-32.sh

iso-32: $(ISO32)

iso-64: $(ISO64)

iso-x86_64: $(ISO64)

floppy-kernel32: $(KERNELFLP_ELF)

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
