#ifndef __SHELL_H
#define __SHELL_H

void shell_run(int client_sock,int uid);
extern int cur_uid;
extern int cur_sock_fd;
#endif