#include "shell_core.h"
#include "types.h"
#include "interrupts.h"
#include "signals.h"
#include "tty.h"
#include "vga.h"
#include "bootlog.h"

#define SHELL_LINE_CAP 256u
#define SHELL_ARGV_MAX 16u

uint16_t row;
uint16_t col;

static int shell_streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (int)(*a == '\0' && *b == '\0');
}

static uint32_t shell_tokenize(char* line, char** argv, uint32_t argv_cap) {
    uint32_t argc = 0u;
    char* p = line;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            ++p;
        }

        if (*p == '\0') {
            break;
        }

        if (argc >= argv_cap) {
            break;
        }

        argv[argc++] = p;

        while (*p != '\0' && *p != ' ' && *p != '\t') {
            ++p;
        }

        if (*p == '\0') {
            break;
        }

        *p = '\0';
        ++p;
    }

    return argc;
}

static void shell_put_u64_hex(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";
    int nibble;

    vga_puts("0x");
    for (nibble = 15; nibble >= 0; --nibble) {
        vga_putc(hex[(uint8_t)((value >> (nibble * 4)) & 0xFu)]);
    }
}

static void shell_cmd_help(void) {
    vga_puts("help clear echo reboot shutdown ticks version\n");
}

static void shell_cmd_clear(void) {
    vga_init();
    vga_set_color(WHITE);
}

static void shell_cmd_echo(char** argv, uint32_t argc) {
    uint32_t i;

    for (i = 1u; i < argc; ++i) {
        if (i > 1u) {
            vga_putc(' ');
        }
        vga_puts(argv[i]);
    }
    vga_putc('\n');
}

static void shell_cmd_reboot(void) {
    vga_puts("Rebooting...\n");
    signal_raise(HW_RESET);
}

static void shell_cmd_shutdown(void) {
    vga_puts("Shutting down...\n");
    signal_raise(HW_PWR_DOWN);
}

static void shell_cmd_ticks(void) {
    uint64_t ticks = timer_ticks_get();

    vga_puts("ticks ");
    shell_put_u64_hex(ticks);
    vga_putc('\n');
}

static void shell_version(void){
	vga_puts("System/1 by Adava (AdavaSoftware) (C) 2026\n");
	vga_puts("All rights reserved.\n");
}

void shell_core_run(void) {
	bootlog_info("shell", "Starting shell...\n");
	vga_set_color(WHITE);
    vga_get_cursor(&row, &col);
    vga_text_begin(row, col);
    char line[SHELL_LINE_CAP];
    char* argv[SHELL_ARGV_MAX];
    uint32_t argc;

    for (;;) {
        vga_puts("kernel> ");
        tty_readline(line, SHELL_LINE_CAP);
        argc = shell_tokenize(line, argv, SHELL_ARGV_MAX);

        if (argc == 0u) {
            continue;
        }

        if (shell_streq(argv[0], "help")) {
            shell_cmd_help();
            continue;
        }

        if (shell_streq(argv[0], "clear")) {
            shell_cmd_clear();
            continue;
        }

        if (shell_streq(argv[0], "echo")) {
            shell_cmd_echo(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "reboot")) {
            shell_cmd_reboot();
            continue;
        }

        if (shell_streq(argv[0], "shutdown")) {
            shell_cmd_shutdown();
            continue;
        }

        if (shell_streq(argv[0], "ticks")) {
            shell_cmd_ticks();
            continue;
        }
        
        if (shell_streq(argv[0], "version")){
			shell_version();
			continue;
		}
		vga_set_color(RED);
        vga_puts("unknown command: ");
        vga_set_color(WHITE);
        vga_puts(argv[0]);
        vga_puts("\n");
    }
}
