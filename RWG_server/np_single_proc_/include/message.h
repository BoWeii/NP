#ifndef __MESSAGE_H
#define __MESSAGE_H

#include <iostream>
#include "user.h"
#include <string>

void msg_broadcast(std::string msg);
void msg_welcome(int sock_fd);
void msg_tell(int sock_fd,std::string msg);
void msg_broadcast_login(user_t user);
void msg_broadcast_logout(user_t user);
void msg_prompt(int client_sock);
#endif