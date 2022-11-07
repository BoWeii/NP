#ifndef __USER_H
#define __USER_H

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>

typedef struct user_t
{
    int id;
    int sock_fd;
    int port;
    int line_idx;
    std::string name;
    std::string ip;
} user_t;

user_t usr_add(int client_sock, struct sockaddr_in client_addr);
user_t usr_find(int client_sock);
void usr_remove(int id);


extern std::vector<user_t> users;
#endif