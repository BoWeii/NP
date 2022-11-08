#ifndef __SOCK_H
#define __SOCK_H

#include <unistd.h>
using namespace std;

typedef struct user_ user_t;


typedef struct fd_
{
    int target_line;
    pid_t pid;
    int pipe_fd[2];
} fd_t;


fd_t fd_find_by_line_idx(user_t user, int line_idx);
void fd_remove(user_t user, fd_t target);
void fd_add(user_t user, fd_t new_fd);
#endif