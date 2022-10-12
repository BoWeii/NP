#include <stdio.h>
#include <iostream>
#include "command.h"


int main()
{
    while (true)
    {
        printf("%% ");
        vector<string> cmds = cmd_read();
        if (!cmds.size())
        {
            continue;
        }
        // cmd_parse();
        // cmd_exec();
    }
}