#ifndef __FD_H
#define __FD_H

#include <unistd.h>
using namespace std;

typedef struct user_ user_t;

// using for numbered pipe
typedef struct np_fd_
{
    int target_line;
    pid_t pid;
    int pipe_fd[2];
} np_fd_t;

np_fd_t remainfd_find_by_lineidx(int line_idx);
void remainfd_remove(np_fd_t target);
void remainfd_add(np_fd_t fd);

extern vector<np_fd_t> remain_fd;

#endif