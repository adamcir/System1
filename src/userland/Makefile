I386_CC ?= $(shell command -v i686-elf-gcc 2>/dev/null || command -v i686-linux-gnu-gcc 2>/dev/null || command -v gcc 2>/dev/null)
I386_LD ?= $(shell command -v i686-elf-ld 2>/dev/null || command -v i686-linux-gnu-ld 2>/dev/null || command -v ld 2>/dev/null)

ROOT_DIR := ../..
BUILD_DIR := build/i386
CFLAGS := -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostdinc -Os -Wall -Wextra -I$(ROOT_DIR)/include

COMMANDS := echo pwd cat mkdir touch rm ln readlink ls help history kconfig reboot shutdown
COMMAND_PRGS := $(addprefix $(BUILD_DIR)/,$(addsuffix .prg,$(COMMANDS)))
MSH_PRG := $(BUILD_DIR)/msh.prg

COMMON_OBJS := \
	$(BUILD_DIR)/crt0.o \
	$(BUILD_DIR)/unistd.o \
	$(BUILD_DIR)/syscall.o

.PHONY: all check clean

all: $(MSH_PRG) $(COMMAND_PRGS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/crt0.o: crt0.i386.S | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD_DIR)/unistd.o: $(ROOT_DIR)/src/lib/user/unistd.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD_DIR)/syscall.o: $(ROOT_DIR)/src/lib/user/syscall.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD_DIR)/msh.o: msh/msh.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD_DIR)/%.o: bin/%.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(MSH_PRG): linker.shell.i386.ld $(BUILD_DIR)/msh.o $(COMMON_OBJS)
	$(I386_LD) -m elf_i386 -T linker.shell.i386.ld --oformat=binary -o $@ $(BUILD_DIR)/msh.o $(COMMON_OBJS)

$(BUILD_DIR)/%.prg: linker.command.i386.ld $(BUILD_DIR)/%.o $(COMMON_OBJS)
	$(I386_LD) -m elf_i386 -T linker.command.i386.ld --oformat=binary -o $@ $(BUILD_DIR)/$*.o $(COMMON_OBJS)

check: all
	@for f in $(MSH_PRG) $(COMMAND_PRGS); do \
		test "$$(od -An -tx1 -N4 $$f | tr -d ' \n')" = "53505247" || exit 1; \
		test "$(stat -c '%s' $f)" -le 65536 || { echo "$f exceeds SPRG 64 KiB file limit"; exit 1; }; \
	done

clean:
	rm -rf build
