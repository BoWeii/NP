#include <stdio.h>
#include <iostream>
#include "command.h"
#include <signal.h>
#include <sys/wait.h>

static inline void init_shell()
{
    setenv("PATH", "bin:.", 1);
    signal(SIGCHLD, sig_handler);
    sigemptyset(&_sigset2);
    sigaddset(&_sigset2, SIGCHLD);
}

int main()
{
    init_shell();
    int line_idx = 0;
    while (true)
    {
        printf("%% ");

        // tokens = { ls -al | cat |2 ls | cat !1 ls > text.txt}
        vector<string> tokens = cmd_read();
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
            cmd_exec(cmdline);
        }
    }
}