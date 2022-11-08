#include <stdio.h>
#include <iostream>
#include "command.h"
#include "message.h"
#include "sock.h"
#include <signal.h>
#include <sys/wait.h>

void shell_init()
{
    setenv("PATH", "bin:.", 1);
    signal(SIGCHLD, sig_handler);
    sigemptyset(&_sigset);
    sigaddset(&_sigset, SIGCHLD);
}
int shell_run_single_command(user_t user)
{
    // tokens = { ls -al | cat |2 ls | cat !1 ls > text.txt}
    vector<string> tokens = cmd_read(user.sock_fd);
    if (!tokens.size())
    {
        msg_prompt(user.sock_fd);
        return 0;
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

        cmdline.line_idx = usr_line_idx_plus(user.id, 1);
        cmdlines.push_back(cmdline);
    }
    if (!cmdlines[0].cmds[0].name.compare("exit"))
    {
        return -1;
    }
    for (auto cmdline : cmdlines)
    {
        cmd_exec(cmdline, user);
    }

    msg_prompt(user.sock_fd);
    return 0;
}