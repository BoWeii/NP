#include <stdio.h>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>

#include "command.h"
#include "sock.h"
#include "fd.h"
#include "message.h"
static inline void shell_init()
{
    setenv("PATH", "bin:.", 1);
    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);
    signal(SIGCHLD, sig_handler);

    sigemptyset(&_sigset);
    sigaddset(&_sigset, SIGCHLD);
}

// current user
int cur_uid;
// current user's sock_fd
int cur_sock_fd;

void shell_run(int client_sock, int uid)
{
    shell_init();
    cur_uid = uid;
    cur_sock_fd = client_sock;

    int line_idx = 0;
    while (true)
    {
        msg_prompt(client_sock);

        // tokens = { ls -al | cat |2 ls | cat !1 ls > text.txt}
        vector<string> tokens = cmd_read(client_sock);
        if (!tokens.size())
        {
            continue;
        }

        /*
            str_lines={
                { ls, -al, |, cat, |2 },
                { ls, |, cat, !1 },
                { ls, >, text.txt }
            }
        */
        vector<vector<string>> str_lines = cmd_split_line(tokens);
        vector<cmdline_t> cmdlines;
        for (auto str_line : str_lines)
        {
            cmdline_t cmdline;

            cmd_parse(cmdline, str_line);

            cmdline.line_idx = line_idx++;
            cmdlines.push_back(cmdline);
        }
        if (!cmdlines[0].cmds[0].name.compare("exit"))
        {
            return;
        }
        for (auto cmdline : cmdlines)
        {
            cmd_exec(cmdline, client_sock);
        }
    }
}