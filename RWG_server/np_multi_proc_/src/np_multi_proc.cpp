#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "np_multi_proc.h"
#include "shell.h"
#include "user.h"
#include "fd.h"
#include "message.h"
#include "command.h"
using namespace std;

#define MAX_ONLINE_USER 30

vector<pid_t> client_shell;

static void sigint_child_hand(int signum)
{
    if (signum != SIGINT)
        return;

    usr_remove(cur_uid);

    // If pid equals 0, then sig is sent to every process in the process group of the calling process.
    // i.e. kill all cmd in the child shell
    kill(0, SIGKILL);
}

static void sigint_handler(int signum)
{
    if (signum != SIGINT)
        return;

    for (auto i = client_shell.begin(); i != client_shell.end(); i++)
    {
        kill(*i, SIGINT);
    }
    users_release();
    msg_release();

    // close the server
    exit(0);
}
static void sigchld_handler(int signum)
{
    if (signum != SIGCHLD)
        return;

    int pid;
    pid = wait(NULL);
    for (auto i = client_shell.begin(); i != client_shell.end(); i++)
    {
        if (*i == pid)
        {
            client_shell.erase(i);
            return;
        }
    }
}
static void init()
{
    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchld_handler);

    // prevent the login msg broadcast to myself
    signal(SIGUSR1, SIG_IGN);

    users_init();
    msg_init();
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "[error] wrong argument\n";
        exit(-1);
    }
    int port = atoi(argv[1]);

    init();
    int server_sock;
    struct sockaddr_in sock_addr;
    pid_t pid;

    // Local communication, TCP, Internet protocol
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "[error] fail to call socket\n";
        exit(-1);
    }
    int enable = 1;
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

            client_shell.push_back(pid);
            close(client_sock);
        }
        else
        {
            // child process

            // new session, prevent kill(0, SIGKILL) kill the parent(server)
            if (setsid() < 0)
            {
                cerr << "[error] fail to call setsid\n";
                exit(-1);
            }

            // if receive ctrl+c
            signal(SIGINT, sigint_child_hand);

            int uid = usr_add(client_addr);
            cur_uid = uid;
            cur_sock_fd = client_sock;
            msg_welcome(client_sock);

            // broadcast the new user login
            char msg[MSG_SIZE_MAX];
            sprintf(msg, "*** User '%s' entered from %s:%d. ***\n",
                    shm_users.users[uid].name,
                    shm_users.users[uid].ip,
                    shm_users.users[uid].port);
            usr_broadcast(msg, MSG_NONE);
            msg_tell(client_sock, string(msg));

            // the broadcast will move the write_offset, so fix the read_offset
            shm_msg.read_offset = shm_msg.msg->write_offset;

            close(server_sock);

            shell_run(client_sock, uid);

            signal(SIGUSR1, SIG_IGN);
            sprintf(msg, "*** User '%s' left. ***\n", shm_users.users[uid].name);
            usr_broadcast(msg, MSG_LOGOUT);

            usr_remove(uid);

            // kill all cmd in the child shell
            kill(0, SIGKILL);
            exit(0);
        }
    }
}