#ifndef SYSTEM1_COMMON_PROCESS_CORE_H
#define SYSTEM1_COMMON_PROCESS_CORE_H

#include "fd_core.h"
#include "fs_core.h"
#include "types.h"

#define PROCESS_MAX 16u
#define PROCESS_NAME_CAP 24u

typedef enum {
    PROCESS_STATE_UNUSED = 0,
    PROCESS_STATE_RUNNABLE = 1,
    PROCESS_STATE_RUNNING = 2,
    PROCESS_STATE_BLOCKED = 3,
    PROCESS_STATE_ZOMBIE = 4
} process_state_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    int exit_status;
    char cwd[FS_PATH_CAP];
    fd_table_t fd_table;
    uintptr_t address_space;
    uintptr_t entry_ip;
    uintptr_t user_sp;
    uintptr_t kernel_stack;
    uint32_t image_arch;
    uint32_t image_segment_count;
    uint32_t image_file_size;
    char name[PROCESS_NAME_CAP];
} process_t;

void process_core_init(void);
process_t* process_core_current(void);
void process_core_set_cwd(process_t* process, const char* cwd);
void process_core_record_exec_image(process_t* process, const char* path, uint32_t arch, uint32_t entry, uint32_t segment_count, uint32_t file_size);

#endif
