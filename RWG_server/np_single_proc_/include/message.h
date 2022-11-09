#ifndef __MESSAGE_H
#define __MESSAGE_H

#include <iostream>
#include <string>
#include "user.h"
#define MSG_SIZE_MAX 1024

void msg_broadcast(std::string msg);
void msg_welcome(int sock_fd);
void msg_tell(int sock_fd, std::string msg);
void msg_broadcast_login(user_t user);
void msg_broadcast_logout(user_t user);
void msg_prompt(int client_sock);
void msg_who(user_t user);
void msg_tell(user_t user);
void msg_yell(user_t user);

void msg_up_not_exist(user_t from, user_t to);
void msg_user_not_exist(int id, user_t to);
void msg_up_exist(user_t from, user_t to);

void msg_broadcast_recv_up(user_t from, user_t to, string cmdline);
void msg_broadcast_recv_send(user_t from, user_t to, string cmdline);
#endif