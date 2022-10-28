#include "np_simple.h"
#include "shell.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>

using namespace std;

#define MAX_ONLINE_USER 30


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "[error] wrong argument\n";
        exit(-1);
    }
    int port = atoi(argv[1]);


    int server_sock;
    struct sockaddr_in sock_addr;
    pid_t pid;

    // Local communication, TCP, Internet protocol
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "[error] fail to call socket\n";
        exit(-1);
    }

    bzero(&sock_addr, sizeof(sock_addr)); // init the struct's bit to all zero
    sock_addr.sin_family = AF_INET;       // ipv4
    sock_addr.sin_port = htons(port);     // transfer local's endian to network's endian
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
    while (1)
    {
        int client_sock;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) == -1)
        {
            cerr << "[error] fail to call accept\n";
            exit(-1);
        }

        // accept a client
        if ((pid = fork()) > 0)
        {
            // parent process

            close(client_sock);
            wait(NULL);
        }
        else
        {
            // child process
            close(server_sock);
            
            shell_run(client_sock);
        }
    }
}