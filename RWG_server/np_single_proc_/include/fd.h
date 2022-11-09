#ifndef __SOCK_H
#define __SOCK_H

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

// using for user pipe
typedef struct up_fd_
{

    int uid; // 0 == default,-1 == fail
    int pipe_fd[2];
} up_fd_t;


np_fd_t remainfd_find_by_lineidx(user_t user, int line_idx);
void remainfd_remove(user_t user, np_fd_t target);
void remainfd_add(user_t user, np_fd_t new_fd);

up_fd_t upfd_find_by_uid(user_t user,int id);
up_fd_t upfd_add(user_t target, int uid);
void upfd_remove(int from_uid, int to_uid);
#endif