I386_CC ?= $(shell command -v i686-elf-gcc 2>/dev/null || command -v i686-linux-gnu-gcc 2>/dev/null || command -v gcc 2>/dev/null)
I386_LD ?= $(shell command -v i686-elf-ld 2>/dev/null || command -v i686-linux-gnu-ld 2>/dev/null || command -v ld 2>/dev/null)

ROOT_DIR := ../..
BUILD_DIR := build/i386
SMU_DIR := smu
CFLAGS := -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostdinc -Os -Wall -Wextra -I$(ROOT_DIR)/include

SMU_FS_COMMANDS := cat ls mkdir pwd rm touch ln readlink
SMU_CORE_COMMANDS := echo help history
SMU_SYSTEM_COMMANDS := kconfig reboot shutdown
SMU_COMMANDS := $(SMU_FS_COMMANDS) $(SMU_CORE_COMMANDS) $(SMU_SYSTEM_COMMANDS)

SMU_FS_OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(SMU_FS_COMMANDS)))
SMU_CORE_OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(SMU_CORE_COMMANDS)))
SMU_SYSTEM_OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(SMU_SYSTEM_COMMANDS)))
COMMAND_PRGS := $(addprefix $(BUILD_DIR)/,$(addsuffix .prg,$(SMU_COMMANDS)))
MSH_PRG := $(BUILD_DIR)/msh.prg

COMMON_OBJS := \
	$(BUILD_DIR)/crt0.o \
	$(BUILD_DIR)/unistd.o \
	$(BUILD_DIR)/syscall.o \
	$(BUILD_DIR)/errno.o

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

$(BUILD_DIR)/errno.o: $(ROOT_DIR)/src/lib/user/errno.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD_DIR)/msh.o: $(SMU_DIR)/shell/msh.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(SMU_FS_OBJS): $(BUILD_DIR)/%.o: $(SMU_DIR)/fs/%.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(SMU_CORE_OBJS): $(BUILD_DIR)/%.o: $(SMU_DIR)/core/%.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(SMU_SYSTEM_OBJS): $(BUILD_DIR)/%.o: $(SMU_DIR)/system/%.c | $(BUILD_DIR)
	$(I386_CC) $(CFLAGS) -m32 -c $< -o $@

$(MSH_PRG): linker.shell.i386.ld $(BUILD_DIR)/msh.o $(COMMON_OBJS)
	$(I386_LD) -m elf_i386 -T linker.shell.i386.ld --oformat=binary -o $@ $(BUILD_DIR)/msh.o $(COMMON_OBJS)

$(BUILD_DIR)/%.prg: linker.command.i386.ld $(BUILD_DIR)/%.o $(COMMON_OBJS)
	$(I386_LD) -m elf_i386 -T linker.command.i386.ld --oformat=binary -o $@ $(BUILD_DIR)/$*.o $(COMMON_OBJS)

check: all
	@for f in $(MSH_PRG) $(COMMAND_PRGS); do \
		test "$$(od -An -tx1 -N4 $$f | tr -d ' \n')" = "53505247" || exit 1; \
		size="$$(stat -c '%s' $$f)"; \
		test "$$size" -le 65536 || { echo "$$f exceeds SPRG 64 KiB file limit ($$size bytes)"; exit 1; }; \
	done

clean:
	rm -rf build
