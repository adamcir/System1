#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"
#include "redir.h"

/* MSh file-descriptor redirection. Keep original stream references until
 * the command completes; execve currently runs synchronously in System/1.
 */
void msh_redir_init(msh_redir_state_t* state) {
    unsigned i;
    if (!state) return;
    for (i = 0u; i < 3u; ++i) state->saved[i] = -1;
}

int msh_redir_apply(msh_redir_state_t* state, const char* op, const char* path) {
    int fd;
    int target;
    int flags;
    if (!state || !op || !path) return -1;

    if (u_streq(op, "<")) {
        target = STDIN_FILENO;
        flags = O_RDONLY;
    } else if (u_streq(op, ">") || u_streq(op, ">>")) {
        target = STDOUT_FILENO;
        flags = O_CREAT | O_WRONLY | (op[1] == '>' ? O_APPEND : O_TRUNC);
    } else if (u_streq(op, "2>") || u_streq(op, "2>>")) {
        target = STDERR_FILENO;
        flags = O_CREAT | O_WRONLY | (op[2] == '>' ? O_APPEND : O_TRUNC);
    } else {
        return -1;
    }
    fd = open(path, flags);
    if (fd < 0) return -1;
    if (state->saved[target] == -1) {
        state->saved[target] = dup(target);
        if (state->saved[target] < 0) {
            (void)close(fd);
            return -1;
        }
    }
    if (dup2(fd, target) < 0) {
        (void)close(fd);
        return -1;
    }
    (void)close(fd);
    return 0;
}

void msh_redir_restore(msh_redir_state_t* state) {
    unsigned i;
    if (!state) return;
    for (i = 0u; i < 3u; ++i) {
        if (state->saved[i] >= 0) {
            (void)dup2(state->saved[i], (int)i);
            (void)close(state->saved[i]);
            state->saved[i] = -1;
        }
    }
}
