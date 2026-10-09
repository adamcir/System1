#include "procfs.h"
#include "mm.h"
#include "interrupts.h"
#include "module.h"

/* Small, bounded, dynamically generated /proc snapshots. */
#define PROC_BUF_SIZE 480u

typedef struct {
    char* buf;
    uint32_t cap;
    uint32_t len;
} proc_writer_t;

static void proc_char(proc_writer_t* w, char ch) {
    if (w->len + 1u < w->cap) w->buf[w->len++] = ch;
}
static void proc_text(proc_writer_t* w, const char* str) {
    while (str && *str) proc_char(w, *str++);
}
static void proc_num(proc_writer_t* w, uint32_t n) {
    char digits[11];
    uint32_t count = 0u;
    do { digits[count++] = (char)('0' + n % 10u); n /= 10u; } while (n && count < 11u);
    while (count) proc_char(w, digits[--count]);
}
static uint32_t proc_build(uint32_t which, char* buf, uint32_t cap) {
    proc_writer_t w = { buf, cap, 0u };
    if (which == 0u) {
        proc_text(&w, "System/1 (Adava Software) SMOD ABI 3\n");
    } else if (which == 1u) {
        uint32_t ticks = (uint32_t)timer_ticks_get();
        proc_num(&w, (uint32_t)(ticks / 100u));
        proc_char(&w, '.');
        proc_char(&w, (char)('0' + (ticks % 100u) / 10u));
        proc_char(&w, (char)('0' + ticks % 10u));
        proc_char(&w, '\n');
    } else if (which == 2u) {
        mm_stats_t st;
        mm_get_stats(&st);
        proc_text(&w, "MemTotal: "); proc_num(&w, st.total_pages * 4u); proc_text(&w, " kB\n");
        proc_text(&w, "MemFree: "); proc_num(&w, st.free_pages * 4u); proc_text(&w, " kB\n");
        proc_text(&w, "HeapUsed: "); proc_num(&w, st.heap_used_bytes); proc_text(&w, " B\n");
    } else if (which == 3u) {
        uint32_t i, count = smod_module_count();
        for (i = 0u; i < count; ++i) {
            const char* name = smod_module_name(i);
            if (name) { proc_text(&w, name); proc_char(&w, '\n'); }
        }
    }
    if (cap) buf[w.len] = '\0';
    return w.len;
}

int procfs_read(uint32_t which, uint32_t offset, char* out, uint32_t cap, uint32_t* read) {
    char snapshot[PROC_BUF_SIZE];
    uint32_t n, i;
    if (which >= 4u || !out || !read) return FS_ERR_INVALID;
    n = proc_build(which, snapshot, sizeof(snapshot));
    *read = 0u;
    if (offset >= n) return FS_OK;
    if (cap > n - offset) cap = n - offset;
    for (i = 0u; i < cap; ++i) out[i] = snapshot[offset + i];
    *read = cap;
    return FS_OK;
}
int procfs_size(uint32_t which, uint32_t* out) {
    char snapshot[PROC_BUF_SIZE];
    if (which >= 4u || !out) return FS_ERR_INVALID;
    *out = proc_build(which, snapshot, sizeof(snapshot));
    return FS_OK;
}
