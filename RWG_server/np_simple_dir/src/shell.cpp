#include <stdio.h>
#include <iostream>
#include "command.h"
#include "sock.h"
#include <signal.h>
#include <sys/wait.h>

static inline void shell_init()
{
    setenv("PATH", "bin:.", 1);
    signal(SIGCHLD, sig_handler);
    sigemptyset(&_sigset);
    sigaddset(&_sigset, SIGCHLD);
}
static void prompt(int client_sock)
{
    const char _prompt[] = "% ";
    sock_write(client_sock, _prompt, 2);
}

void shell_run(int client_sock)
{
    shell_init();
    int line_idx = 0;
    while (true)
    {
        prompt(client_sock);

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
        for (auto cmdline : cmdlines)
        {
            cmd_exec(cmdline, client_sock);
        }
    }
}