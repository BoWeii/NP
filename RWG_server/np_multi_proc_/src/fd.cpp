#include <iostream>
#include <vector>
#include "user.h"
#include "fd.h"
using namespace std;
// store the fd in use
vector<np_fd_t> remain_fd;

void remainfd_remove(np_fd_t target)
{
    for (auto fd = remain_fd.begin(); fd != remain_fd.end(); fd++)
    {
        if (target.target_line == fd->target_line)
        {
            remain_fd.erase(fd);
            return;
        }
    }
}

void remainfd_add(np_fd_t fd)
{
    remain_fd.push_back(fd);
}

np_fd_t remainfd_find_by_lineidx(int line_idx)
{
    np_fd_t ret;
    // check exist or not;
    ret.target_line = -1;

    for (auto fd : remain_fd)
    {
        if (line_idx == fd.target_line)
        {
            ret.pipe_fd[0] = fd.pipe_fd[0];
            ret.pipe_fd[1] = fd.pipe_fd[1];
            ret.target_line = fd.target_line;
        }
    }
    return ret;
}
