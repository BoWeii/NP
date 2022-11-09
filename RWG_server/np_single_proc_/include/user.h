#ifndef __USER_H
#define __USER_H

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <unordered_map>
#include "fd.h"

using namespace std;

typedef struct user_
{
    int id;
    int sock_fd;
    int port;
    int line_idx;
    string name;
    string ip;
    unordered_map<string, string> env_vars;

    // store the fd in use
    vector<np_fd_t> remain_fd;

    // store the user pipe to 
    // key=uid
    unordered_map<int,up_fd_t> up_fd;
} user_t;

user_t usr_add(int client_sock, struct sockaddr_in client_addr);
user_t usr_find_by_sockfd(int client_sock);
user_t usr_find_by_id(int id);
user_t usr_find_by_name(string name);
void usr_update_name(int id, string new_name);
void usr_set_env_var(int id, string var, string value);
void usr_print_env_var(user_t _user, string var);
void usr_remove(int id);
int usr_line_idx_plus(int id, int num);
extern std::vector<user_t> users;
#endif