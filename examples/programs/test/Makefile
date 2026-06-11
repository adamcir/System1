I386_CC ?= $(shell command -v i686-elf-gcc 2>/dev/null || command -v i686-linux-gnu-gcc 2>/dev/null || command -v gcc 2>/dev/null)
I386_LD ?= $(shell command -v i686-elf-ld 2>/dev/null || command -v i686-linux-gnu-ld 2>/dev/null || command -v ld 2>/dev/null)
X64_CC  ?= $(shell command -v x86_64-elf-gcc 2>/dev/null || command -v x86_64-linux-gnu-gcc 2>/dev/null || command -v gcc 2>/dev/null)
X64_LD  ?= $(shell command -v x86_64-elf-ld 2>/dev/null || command -v x86_64-linux-gnu-ld 2>/dev/null || command -v ld 2>/dev/null)

ROOT_DIR := ../../..
BUILD_DIR := build

CFLAGS_COMMON := -ffreestanding -fno-pic -fno-pie -nostdlib -nostdinc -Wall -Wextra -I$(ROOT_DIR)/include
I386_CFLAGS := $(CFLAGS_COMMON) -m32
X64_CFLAGS := $(CFLAGS_COMMON) -m64 -mno-red-zone

I386_OBJS := $(BUILD_DIR)/test.i386.o $(BUILD_DIR)/unistd.i386.o $(BUILD_DIR)/syscall.i386.o
X64_OBJS := $(BUILD_DIR)/test.x86_64.o $(BUILD_DIR)/unistd.x86_64.o $(BUILD_DIR)/syscall.x86_64.o

I386_PRG := $(BUILD_DIR)/program-i386.prg
X64_PRG := $(BUILD_DIR)/program-x86_64.prg

.PHONY: all check clean

all: $(I386_PRG) $(X64_PRG)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test.i386.o: test.c | $(BUILD_DIR)
	$(I386_CC) $(I386_CFLAGS) -c $< -o $@

$(BUILD_DIR)/unistd.i386.o: $(ROOT_DIR)/lib/user/unistd.c | $(BUILD_DIR)
	$(I386_CC) $(I386_CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.i386.o: $(ROOT_DIR)/lib/user/syscall.c | $(BUILD_DIR)
	$(I386_CC) $(I386_CFLAGS) -c $< -o $@

$(BUILD_DIR)/test.x86_64.o: test.c | $(BUILD_DIR)
	$(X64_CC) $(X64_CFLAGS) -c $< -o $@

$(BUILD_DIR)/unistd.x86_64.o: $(ROOT_DIR)/lib/user/unistd.c | $(BUILD_DIR)
	$(X64_CC) $(X64_CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.x86_64.o: $(ROOT_DIR)/lib/user/syscall.c | $(BUILD_DIR)
	$(X64_CC) $(X64_CFLAGS) -c $< -o $@

$(I386_PRG): linker.i386.ld $(I386_OBJS)
	$(I386_LD) -m elf_i386 -T linker.i386.ld --oformat=binary -o $@ $(I386_OBJS)

$(X64_PRG): linker.x86_64.ld $(X64_OBJS)
	$(X64_LD) -m elf_x86_64 -T linker.x86_64.ld --oformat=binary -o $@ $(X64_OBJS)

check: all
	test "$$(od -An -tx1 -N4 $(I386_PRG) | tr -d ' \n')" = "53505247"
	test "$$(od -An -tx1 -N4 $(X64_PRG) | tr -d ' \n')" = "53505247"

clean:
	rm -rf $(BUILD_DIR)
