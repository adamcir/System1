#ifndef SYSTEM1_MSH_REDIR_H
#define SYSTEM1_MSH_REDIR_H

typedef struct {
    int saved[3];
} msh_redir_state_t;

void msh_redir_init(msh_redir_state_t* state);
int msh_redir_apply(msh_redir_state_t* state, const char* op, const char* path);
void msh_redir_restore(msh_redir_state_t* state);

#endif
