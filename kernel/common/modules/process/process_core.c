#include "process_core.h"

static process_t g_processes[PROCESS_MAX];
static process_t* g_current_process = 0;
static uint32_t g_next_pid = 1u;

static void process_copy_string(char* dst, uint32_t cap, const char* src) {
    uint32_t i = 0u;

    if (cap == 0u) {
        return;
    }

    while (src != 0 && src[i] != '\0' && (i + 1u) < cap) {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}

static void process_clear(process_t* process) {
    process->pid = 0u;
    process->state = PROCESS_STATE_UNUSED;
    process->exit_status = 0;
    process->cwd[0] = '\0';
    fd_core_table_init(&process->fd_table);
    process->address_space = 0u;
    process->entry_ip = 0u;
    process->user_sp = 0u;
    process->kernel_stack = 0u;
    process->name[0] = '\0';
}

void process_core_init(void) {
    uint32_t i;
    process_t* initial;

    g_next_pid = 1u;

    for (i = 0u; i < PROCESS_MAX; ++i) {
        process_clear(&g_processes[i]);
    }

    initial = &g_processes[0];
    initial->pid = g_next_pid++;
    initial->state = PROCESS_STATE_RUNNING;
    process_copy_string(initial->cwd, FS_PATH_CAP, "/");
    fd_core_table_init(&initial->fd_table);
    process_copy_string(initial->name, PROCESS_NAME_CAP, "kernel-shell");

    g_current_process = initial;
}

process_t* process_core_current(void) {
    if (g_current_process == 0) {
        process_core_init();
    }

    return g_current_process;
}

void process_core_set_cwd(process_t* process, const char* cwd) {
    if (process == 0 || cwd == 0) {
        return;
    }

    process_copy_string(process->cwd, FS_PATH_CAP, cwd);
}
