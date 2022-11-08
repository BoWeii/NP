#ifndef __FD_T_H
#define __FD_T_H

#include <iostream>
#include <unistd.h>


int sock_write(int sock_fd, const char *buf, size_t len);
int sock_read(int sock, char *buf, size_t len);
int sock_getline(int sock_fd, std::string &line, size_t len);

#endif