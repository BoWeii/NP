#include "sock.h"
#define MAX_LINE_SIZE 15000

using namespace std;
int sock_write(int sock_fd, const char *buf, size_t len)
{
    int ret = write(sock_fd, buf, len);
    return ret;
}

int sock_read(int sock_fd, char *buf, size_t len)
{
    int ret = read(sock_fd, buf, len);
    return ret;
}

int sock_getline(int sock_fd, string &line, size_t len)
{
    char buf[MAX_LINE_SIZE] = {0};
    if (sock_read(sock_fd, buf, len)<=0)
    {
        return -1;
    }
    line = buf;
    return line.size();
}