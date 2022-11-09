#include <vector>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <algorithm>
#include <unordered_map>

#include "user.h"
#include "sock.h"
using namespace std;

#define DEFAULT_NAME "(no name)"

// use vector is not a good...
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

static bool compare_by_id(const user_t a, const user_t b)
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
    user.env_vars.insert(pair<string, string>("PATH", "bin:."));

    //  converts the Internet host address to string xxx.xxx.xxx.xxx
    user.ip = inet_ntoa(client_addr.sin_addr);

    users.push_back(user);
    usr_sort_by_id();

    return user;
}

user_t usr_find_by_sockfd(int client_sock)
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
user_t usr_find_by_id(int id)
{
    user_t ret;
    ret.id = -1;
    for (auto user : users)
    {
        if (user.id == id)
        {
            ret = user;
            return ret;
        }
    }
    return ret;
}

user_t usr_find_by_name(string name)
{
    user_t ret;
    ret.id = -1;
    for (auto user : users)
    {
        if (!user.name.compare(name))
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

void usr_update_name(int id, string new_name)
{
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == id)
        {
            user->name = new_name;
            return;
        }
    }
}

int usr_line_idx_plus(int id, int num)
{
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == id)
        {
            user->line_idx += num;
            return user->line_idx;
        }
    }
    return -1;
}

void usr_set_env_var(int id, string var, string value)
{
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == id)
        {
            auto env_var = user->env_vars.find(var);

            // replace the origin env
            if (env_var != user->env_vars.end())
            {
                env_var->second = value;
            }
            else
            {
                user->env_vars.insert(pair<string, string>(var, value));
            }
            return;
        }
    }
}
void usr_print_env_var(user_t _user, string var)
{
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == _user.id)
        {
            auto env_var = user->env_vars.find(var);
            if (env_var != user->env_vars.end())
            {
                sock_write(_user.sock_fd, env_var->second.c_str(), env_var->second.size());
                sock_write(_user.sock_fd, "\n", 1);
            }
        }
    }
}
