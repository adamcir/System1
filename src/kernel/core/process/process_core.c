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
    process->ppid = 0u;
    process->state = PROCESS_STATE_UNUSED;
    process->exit_status = 0;
    process->cwd[0] = '\0';
    fd_core_table_reset(&process->fd_table);
    process->address_space = 0u;
    process->entry_ip = 0u;
    process->user_sp = 0u;
    process->kernel_stack = 0u;
    process->image_arch = 0u;
    process->image_segment_count = 0u;
    process->image_file_size = 0u;
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
    initial->ppid = 0u;
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

void process_core_record_exec_image(process_t* process, const char* path, uint32_t arch, uint32_t entry, uint32_t segment_count, uint32_t file_size) {
    if (process == 0) {
        return;
    }

    process->image_arch = arch;
    process->entry_ip = (uintptr_t)entry;
    process->image_segment_count = segment_count;
    process->image_file_size = file_size;
    process_copy_string(process->name, PROCESS_NAME_CAP, path);
}


static process_t* process_find_pid(uint32_t pid) {
    uint32_t i;

    for (i = 0u; i < PROCESS_MAX; ++i) {
        if (g_processes[i].state != PROCESS_STATE_UNUSED &&
            g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }

    return 0;
}

process_t* process_core_spawn_exec(const char* path, uint32_t arch, uint32_t entry,
                                   uint32_t segment_count, uint32_t file_size,
                                   uintptr_t user_sp) {
    process_t* parent = process_core_current();
    uint32_t i;

    for (i = 0u; i < PROCESS_MAX; ++i) {
        process_t* child = &g_processes[i];

        if (child->state != PROCESS_STATE_UNUSED) {
            continue;
        }

        process_clear(child);
        child->pid = g_next_pid++;
        child->ppid = (parent != 0) ? parent->pid : 0u;
        child->state = PROCESS_STATE_RUNNING;
        child->user_sp = user_sp;
        if (parent != 0) {
            process_copy_string(child->cwd, FS_PATH_CAP, parent->cwd);
        } else {
            process_copy_string(child->cwd, FS_PATH_CAP, "/");
        }
        if (parent != 0) {
            if (fd_core_table_clone(&child->fd_table, &parent->fd_table) < 0) {
                process_clear(child);
                return 0;
            }
        } else {
            fd_core_table_init(&child->fd_table);
        }
        process_core_record_exec_image(child, path, arch, entry, segment_count, file_size);
        g_current_process = child;
        return child;
    }

    return 0;
}

void process_core_exit_current(int status) {
    process_t* current = process_core_current();
    process_t* parent;

    if (current == 0) {
        return;
    }

    current->exit_status = status;
    current->state = PROCESS_STATE_ZOMBIE;

    parent = process_find_pid(current->ppid);
    if (parent != 0) {
        parent->state = PROCESS_STATE_RUNNING;
        g_current_process = parent;
    }
}

void process_core_reap(process_t* process) {
    if (process == 0 || process->state != PROCESS_STATE_ZOMBIE) {
        return;
    }

    fd_core_table_destroy(&process->fd_table);
    process_clear(process);
}
