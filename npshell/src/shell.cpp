#include <stdio.h>
#include <iostream>
#include "command.h"

static inline void init_shell()
{
    setenv("PATH", "bin:.", 1);
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
        vector<s_cmdline> cmdlines;
        for (auto str_line : str_lines)
        {
            s_cmdline cmdline;

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