#include "user.h"
#include <vector>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <algorithm>
using namespace std;

#define DEFAULT_NAME "(no name)"

vector<user_t> users;

static int get_unuse_id()
{
    int target = 1;

    if (!users.size())
    {
        return target;
    }

    for (auto user : users)
    {
        if (target == user.id)
        {
            target++;
        }
        else
        {
            return target;
        }
    }

    return target;
}

static bool compare_by_id(const user_t &a, const user_t &b)
{
    return a.id < b.id;
}
static void usr_sort_by_id()
{
    sort(users.begin(), users.end(), compare_by_id);
}

user_t usr_add(int client_sock, struct sockaddr_in client_addr)
{
    user_t user;

    user.id = get_unuse_id();
    user.sock_fd = client_sock;
    user.port = ntohs(client_addr.sin_port);
    user.line_idx = 0;
    user.name = DEFAULT_NAME;

    //  converts the Internet host address to string xxx.xxx.xxx.xxx
    user.ip = inet_ntoa(client_addr.sin_addr);

    users.push_back(user);
    usr_sort_by_id();

    return user;
}

user_t usr_find(int client_sock)
{
    user_t ret;
    ret.id = -1;
    for (auto user : users)
    {
        if (user.sock_fd == client_sock)
        {
            ret = user;
        }
    }
    return ret;
}

void usr_remove(int id)
{
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == id)
        {
            users.erase(user);
            return;
        }
    }
}