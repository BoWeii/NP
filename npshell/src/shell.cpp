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
    while (true)
    {
        printf("%% ");

        vector<string> cmdline = cmd_read();
        if (!cmdline.size())
        {
            continue;
        }

        cmd_parse(cmdline);

        // cmd_exec();
    }
}