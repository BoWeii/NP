#include "message.h"
#include "sock.h"
#include "user.h"
#include <iostream>
#include <string>
#include <stdio.h>
using namespace std;

#define MSG_SIZE_MAX 1024

void msg_broadcast(string msg)
{
    for (auto user : users)
    {
        msg_tell(user.sock_fd, msg);
    }
}
void msg_welcome(int sock_fd)
{
    string welcome = "****************************************\n"
                     "** Welcome to the information server. **\n"
                     "****************************************\n";
    msg_tell(sock_fd, welcome);
}

void msg_tell(int sock_fd, string msg)
{
    sock_write(sock_fd, msg.c_str(), msg.size());
}

void msg_broadcast_login(user_t user)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** User '%s' entered from %s:%d. *** id=%d\n", user.name.c_str(), user.ip.c_str(), user.port,user.id);
    msg_broadcast(string(msg));
}
void msg_broadcast_logout(user_t user)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** User '%s' left. ***\n", user.name.c_str());
    msg_broadcast(string(msg));
}

void msg_prompt(int client_sock)
{
    const char _prompt[] = "% ";
    sock_write(client_sock, _prompt, 2);
}