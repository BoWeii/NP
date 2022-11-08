#include <iostream>
#include <vector>
#include "user.h"
#include "fd.h"
using namespace std;

void fd_add(user_t user, fd_t new_fd)
{
    for (auto _user = users.begin(); _user != users.end(); _user++)
    {
        if (_user->id == user.id)
        {
            _user->remain_fd.push_back(new_fd);
        }
    }
}

void fd_remove(user_t user, fd_t target)
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

fd_t fd_find_by_line_idx(user_t user, int line_idx)
{
    fd_t ret;
    // check exist or not;
    ret.target_line = -1;
    for (auto _user =users.begin(); _user != users.end(); _user++)
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
                }
            }
        }
    }

    return ret;
}