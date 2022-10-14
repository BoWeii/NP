#include "command.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <unistd.h>

const char special_symbols_icons[] = {'>', '|', 'x', '!'};

#define handle_special_symbol(str, new_cmd)               \
    {                                                     \
        if ((str[0] == '|' && str.size() == 1))           \
        {                                                 \
            new_cmd->symbol_type = pipe_ordinary;         \
            break;                                        \
        }                                                 \
        else if (str[0] == '|' && str.size() != 1)        \
        {                                                 \
            new_cmd->symbol_type = pipe_numbered_out;     \
            new_cmd->pipe_num = atoi(&str[1]);            \
            break;                                        \
        }                                                 \
        else if (str[0] == '>')                           \
        {                                                 \
            new_cmd->symbol_type = redirect;              \
            new_cmd->file_name = line[cur_idx++];         \
            continue;                                     \
        }                                                 \
        else if (str[0] == '!' && str.size() != 1)        \
        {                                                 \
            new_cmd->symbol_type = pipe_numbered_out_err; \
            new_cmd->pipe_num = atoi(&str[1]);            \
            break;                                        \
        }                                                 \
    }

// helper function
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

static void print_cur_cmds(vector<s_cmd> &cmds)
{
    // using for debugging
    for (auto cmd : cmds)
    {
        printf("cmd = %s, symbol_type = %c, pipe_num = %d, file_name = %s ,argv = ", cmd.name.c_str(), special_symbols_icons[cmd.symbol_type], cmd.pipe_num, cmd.file_name.c_str());
        for (auto _argv : cmd.argv)
        {
            printf("%s ", _argv.c_str());
        }
        cout << endl;
    }
}

// function using in shell
vector<string> cmd_read()
{
    string cmdline;
    getline(cin, cmdline);

    vector<string> tokens = tokenize(cmdline);

    return tokens;
}

vector<vector<string>> cmd_split_line(vector<string> tokens)
{
    vector<vector<string>> ret;
    size_t cur_idx = 0;

    while (cur_idx < tokens.size())
    {
        vector<string> line;
        while (cur_idx < tokens.size())
        {
            line.push_back(tokens[cur_idx]);
            if ((tokens[cur_idx][0] == '|' || tokens[cur_idx][0] == '!') && tokens[cur_idx].size() != 1)
            {
                cur_idx++;
                break;
            }
            cur_idx++;
        }
        ret.push_back(line);
    }
    // printf("[split to]\n");
    // for (auto a : ret)
    // {
    //     for (auto b : a)
    //     {
    //         printf("%s ", b.c_str());
    //     }
    //     cout << endl;
    // }
    cout << endl;
    return ret;
}

void cmd_parse(s_cmdline &cmdline, vector<string> line)
{
    if (is_built_in_cmd(line))
    {
        return;
    }

    vector<s_cmd> cmds;
    bool is_ready_counter_cmd = true;
    size_t cur_idx = 0;
    string cur_token = "";
    s_cmd *new_cmd;

    while (cur_idx < line.size())
    {
        new_cmd = new s_cmd();
        new_cmd->symbol_type = new_cmd->pipe_num = -1;
        is_ready_counter_cmd = true;

        while (cur_idx < line.size())
        {
            cur_token = line[cur_idx++];
            if (is_ready_counter_cmd)
            {
                new_cmd->name = cur_token;
                is_ready_counter_cmd = false;
                continue;
            }

            handle_special_symbol(cur_token, new_cmd);
            (new_cmd->argv).push_back(cur_token);
        }

        cmds.push_back(*new_cmd);
        delete new_cmd;
    }
    cmdline.cmds = cmds;
    return;
}

void cmd_exec(s_cmdline cmdline)
{
    // printf("Line %d\n", cmdline.line_idx);
    // print_cur_cmds(cmdline.cmds);
    // cout << "\n";
    pid_t pid;

    for (auto cmd = cmdline.cmds.begin(); cmd != cmdline.cmds.end(); cmd++)
    {
        // printf("line %d: cmd=%s\n", cmdline.line_idx, cmd->name.c_str());
        char **argv;

        if ((pid = fork()) > 0)
        {
            // parent process
        }
        else if (pid == 0)
        {
            // child process

            // processing argv
            int argv_size = cmd->argv.size() + 2;
            int argv_idx = 1;

            argv = (char **)malloc(sizeof(char *) * argv_size);
            argv[0] = &(cmd->name[0]);
            for (auto _argv : cmd->argv)
            {
                argv[argv_idx++] = &(_argv[0]);
            }
            argv[argv_idx] = NULL;

            execvp(argv[0], argv);

            // execvp fails
            fprintf(stderr, "Unknown command: [%s].\n", cmd->name.c_str());
            exit(-1);
        }
        else
        {
            // fork error
        }
    }
}