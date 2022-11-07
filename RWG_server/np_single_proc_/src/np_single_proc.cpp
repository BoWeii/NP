
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include "np_single_proc.h"
#include "shell.h"
#include "user.h"
#include "message.h"
using namespace std;

#define MAX_ONLINE_USER 30
#ifndef FD_COPY
#define FD_COPY(dest, src) memcpy((dest), (src), sizeof *(dest))
#endif

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "[error] wrong argument\n";
        exit(-1);
    }
    int port = atoi(argv[1]);

    int server_sock;
    int enable = 1;
    struct sockaddr_in sock_addr;

    // Local communication, TCP, Internet protocol
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "[error] fail to call socket\n";
        exit(-1);
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        cerr << "[error] fail to setsockopt\n";
        exit(-1);
    }

    bzero(&sock_addr, sizeof(sock_addr)); // init the struct's bit to all zero
    sock_addr.sin_family = AF_INET;       // ipv4
    sock_addr.sin_port = htons(port);     // transfer host's endian to network's endian
    sock_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
    {
        cerr << "[error] fail to call bind\n";
    }

    if (listen(server_sock, MAX_ONLINE_USER) != 0)
    {
        cerr << "[error] fail to call listen\n";
        exit(-1);
    }

    fd_set read_fds;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_ZERO(&read_fds);
    FD_SET(server_sock, &all_fds);

    int max_fd = server_sock;
    shell_init();
    while (1)
    {
        read_fds = all_fds;

        // The file descriptors in this read_fds are watched to see if they are ready for reading.
        while (select(max_fd + 1, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
        {
            //  the file descriptor sets are unmodified
        }

        if (FD_ISSET(server_sock, &read_fds))
        {
            // a new client loggin
            int client_sock;
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);

            if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) == -1)
            {
                cerr << "[error] fail to call accept\n";
                exit(-1);
            }
            user_t user = usr_add(client_sock, client_addr);

            if (client_sock > max_fd)
            {
                max_fd = client_sock;
            }
            // add new client to be monitored
            FD_SET(client_sock, &all_fds);

            msg_welcome(client_sock);
            msg_broadcast_login(user);
            msg_prompt(client_sock);
        }

        // resolve client command
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (fd != server_sock && FD_ISSET(fd, &read_fds))
            {
                user_t user = usr_find(fd);
                if (user.id == -1)
                {
                    cerr << "[error] fail to find user_fd:" << fd << endl;
                    exit(-1);
                }
                if (shell_run_single_command(user))
                {
                    // client call the exit
                    usr_remove(user.id);
                    msg_broadcast_logout(user);

                    FD_CLR(fd, &all_fds);

                    close(fd);
                }
            }
        }
    }
}