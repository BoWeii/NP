#include "command.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <stdio.h>

#define check_end_of_cmd(str, new_cmd)               \
    {                                                \
        if ((str[0] == '|' && str.size() == 1))      \
        {                                            \
            new_cmd->symbol_type = pipe_ordinary;    \
            break;                                   \
        }                                            \
        else if (str[0] == '|' && str.size() == 2)   \
        {                                            \
            new_cmd->symbol_type = pipe_numbered;    \
            new_cmd->pipe_num = str[1] - '0';        \
            break;                                   \
        }                                            \
        else if (str[0] == '>')                      \
        {                                            \
            new_cmd->symbol_type = redirect;         \
            new_cmd->file_name = cmdline[cur_idx++]; \
        }                                            \
    }

static vector<string> tokenize(string line)
{
    vector<string> ret;
    stringstream ss(line);
    string token;

    while (ss >> token)
    {
        ret.push_back(token);
    }

    return ret;
}

vector<string> cmd_read()
{
    string cmdline;
    getline(cin, cmdline);

    vector<string> tokens = tokenize(cmdline);

    return tokens;
}

static bool is_built_in_cmd(vector<string> &cmds)
{
    if (!cmds[0].compare("setenv"))
    {
        string var = cmds[1];
        string value = cmds[2];

        setenv(var.c_str(), value.c_str(), 1);

        return true;
    }
    else if (!cmds[0].compare("printenv"))
    {
        string var = cmds[1];
        string value = getenv(var.c_str());

        if (value.size())
        {
            cout << value << endl;
        }

        return true;
    }
    else if (!cmds[0].compare("exit"))
    {
        exit(0);
    }

    return false;
}

void print_cur_cmds(vector<cmd> &cmds)
{
    for (auto cmd : cmds)
    {
        printf("cmd=%s, symbol_type=%d, pipe_num=%d, file_name=%s ,argv=", cmd.name.c_str(), cmd.symbol_type, cmd.pipe_num, cmd.file_name.c_str());
        for (auto _argv : cmd.argv)
        {
            printf("%s ", _argv.c_str());
        }
        cout << endl;
    }
}

void cmd_parse(vector<string> cmdline)
{
    if (is_built_in_cmd(cmdline))
    {
        return;
    }

    // ls -al |2 cat | ls > text.txt
    vector<cmd> cmds;
    bool is_ready_counter_cmd = true;
    size_t cur_idx = 0;
    string cur_token = "";
    cmd *new_cmd;

    while (cur_idx < cmdline.size())
    {
        new_cmd = new cmd();
        new_cmd->symbol_type = new_cmd->pipe_num = -1;

        while (cur_idx < cmdline.size())
        {
            cur_token = cmdline[cur_idx++];
            // printf("cur_token=%s\n", cur_token.c_str());
            if (is_ready_counter_cmd)
            {
                new_cmd->name = cur_token;
                is_ready_counter_cmd = false;
                continue;
            }
            // cout<<"before check\n";
            check_end_of_cmd(cur_token, new_cmd);
            (new_cmd->argv).push_back(cur_token);
        }
        printf("push %s,\n", new_cmd->name.c_str());

        cmds.push_back(*new_cmd);
        delete new_cmd;
        is_ready_counter_cmd = true;
    }
    print_cur_cmds(cmds);
}