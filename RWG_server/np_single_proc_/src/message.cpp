#include <iostream>
#include <string>
#include <stdio.h>

#include "message.h"
#include "sock.h"
#include "user.h"

using namespace std;

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
    sprintf(msg, "*** User '%s' entered from %s:%d. ***\n", user.name.c_str(), user.ip.c_str(), user.port);
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
void msg_who(user_t me)
{
    char msg[MSG_SIZE_MAX];

    msg_tell(me.sock_fd, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
    for (auto user : users)
    {
        sprintf(msg, "%d\t%s\t%s:%d\t", user.id, user.name.c_str(), user.ip.c_str(), user.port);
        msg_tell(me.sock_fd, string(msg));

        if (me.id == user.id)
        {
            msg_tell(me.sock_fd, "<-me");
        }

        msg_tell(me.sock_fd, "\n");
    }
}

void msg_up_not_exist(user_t from, user_t to)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", from.id, to.id);
    msg_tell(to.sock_fd, string(msg));
}

void msg_user_not_exist(int id, user_t to)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", id);
    msg_tell(to.sock_fd, string(msg));
}

void msg_up_exist(user_t from, user_t to)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** Error: the pipe #%d->#%d already exists. ***\n", from.id, to.id);
    msg_tell(from.sock_fd, string(msg));
}

void msg_broadcast_recv_up(user_t from, user_t to, string cmdline)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", to.name.c_str(), to.id, from.name.c_str(), from.id, cmdline.c_str());
    msg_broadcast(string(msg));
}

void msg_broadcast_recv_send(user_t from, user_t to, string cmdline)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", from.name.c_str(), from.id, cmdline.c_str(), to.name.c_str(), to.id);
    msg_broadcast(string(msg));
}
