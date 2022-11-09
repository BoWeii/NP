#include <iostream>
#include <vector>
#include "user.h"
#include "fd.h"
using namespace std;

void remainfd_add(user_t user, np_fd_t new_fd)
{
    for (auto _user = users.begin(); _user != users.end(); _user++)
    {
        if (_user->id == user.id)
        {
            _user->remain_fd.push_back(new_fd);
        }
    }
}

void remainfd_remove(user_t user, np_fd_t target)
{
    for (auto _user = users.begin(); _user != users.end(); _user++)
    {
        if (_user->id == user.id)
        {
            for (auto fd = _user->remain_fd.begin(); fd != _user->remain_fd.end(); fd++)
            {
                if (target.target_line == fd->target_line)
                {
                    _user->remain_fd.erase(fd);
                    return;
                }
            }
        }
    }
}

np_fd_t remainfd_find_by_lineidx(user_t user, int line_idx)
{
    np_fd_t ret;
    // check exist or not;
    ret.target_line = -1;
    for (auto _user = users.begin(); _user != users.end(); _user++)
    {
        if (_user->id == user.id)
        {
            for (auto fd : _user->remain_fd)
            {
                if (line_idx == fd.target_line)
                {
                    ret.pipe_fd[0] = fd.pipe_fd[0];
                    ret.pipe_fd[1] = fd.pipe_fd[1];
                    ret.target_line = fd.target_line;
                    return ret;
                }
            }
        }
    }
    return ret;
}

up_fd_t upfd_add(user_t target, int uid)
{
    up_fd_t ret;
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == target.id)
        {
            up_fd_t up_fd;
            up_fd.uid = uid;
            pipe(up_fd.pipe_fd);

            pair<int, up_fd_t> tmp(uid, up_fd);

            user->up_fd.insert(tmp);
            ret = up_fd;
            break;
        }
    }
    return ret;
}

up_fd_t upfd_find_by_uid(user_t target, int uid)
{
    up_fd_t ret;
    ret.uid = -1;
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == target.id)
        {
            auto fd = user->up_fd.find(uid);
            if (fd != user->up_fd.end())
            {
                ret = fd->second;
                return ret;
            }
        }
    }
    return ret;
}

void upfd_remove(int from_uid, int to_uid)
{
    for (auto user = users.begin(); user != users.end(); user++)
    {
        if (user->id == from_uid)
        {
            user->up_fd.erase(to_uid);
        }
    }
}