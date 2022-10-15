#include "command.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unordered_map>

// <which_line, cur_process>
// unordered_map<int, unordered_map<pid_t, int>> whole_process;

// <which_pid, pipe to which number>
unordered_map<pid_t, int> cur_process;

sigset_t _sigset2;
bool use_sh_wait = true;
const char special_symbols_icons[] = {'>', '|', 'x', '!'};

#define handle_special_symbol(str, new_cmd)               \
    {                                                     \
        if ((str[0] == '|' && str.size() == 1))           \
        {                                                 \
            new_cmd->symbol_type = pipe_ordinary;         \
            new_cmd->pipe_num = 1;                        \
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

static void print_cur_cmds(vector<cmt_t> &cmds)
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

void sig_handler(int signum)
{
    pid_t pid;
    if (!use_sh_wait)
    {
        return;
    }
    pid = wait(NULL);
    cur_process.erase(pid);
}

// disable signal handler
static void disable_sh()
{
    sigset_t oldset;
    sigprocmask(SIG_BLOCK, &_sigset2, &oldset);

    use_sh_wait = false;

    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

// enable signal handler
static void enable_sh()
{
    sigset_t oldset;
    sigprocmask(SIG_BLOCK, &_sigset2, &oldset);

    use_sh_wait = true;

    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

static void cur_proc_insert(pid_t pid, int pipe_num)
{
    disable_sh();
    cur_process.insert(pair<pid_t,int>(pid, pipe_num));
    enable_sh();
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
    // cout << endl;
    return ret;
}

void cmd_parse(cmdline_t &cmdline, vector<string> line)
{
    if (is_built_in_cmd(line))
    {
        return;
    }

    vector<cmt_t> cmds;
    bool is_ready_counter_cmd = true;
    size_t cur_idx = 0;
    string cur_token = "";
    cmt_t *new_cmd;

    while (cur_idx < line.size())
    {
        new_cmd = new cmt_t();
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

void cmd_exec(cmdline_t cmdline)
{
    // printf("Line %d\n", cmdline.line_idx);
    // print_cur_cmds(cmdline.cmds);
    // cout << "\n";
    pid_t pid;
    int cmd_idx = 0;
    int read_pipe = -1;
    cur_process.clear();

    enable_sh();
    for (auto cmd = cmdline.cmds.begin(); cmd != cmdline.cmds.end(); cmd++, cmd_idx++)
    {
        // printf("line %d: cmd=%s\n", cmdline.line_idx, cmd->name.c_str());
        char **argv;

        // handle special symbol
        int pipe_fd[2] = {-1, -1};
        switch (cmd->symbol_type)
        {
        case pipe_ordinary:
            if (pipe(pipe_fd))
            {
                printf("Error : pipe after cmd_exec\n");
            }
            break;
        }

        // ready to fork
        if ((pid = fork()) > 0)
        {
            // parent process
            cur_proc_insert(pid, cmd->pipe_num);

            // input pipe
            if (read_pipe != -1)
            {
                close(read_pipe);
            }

            // close output_pipe and save read_pipe to next cmd
            if (pipe_fd[1] != -1)
            {
                close(pipe_fd[1]);
                read_pipe = pipe_fd[0]; // assign for next cmd read_pipe
            }
            else
            {
                read_pipe = -1;
            }
        }
        else if (pid == 0)
        {
            // child process

            // input pipe


            if (read_pipe != -1)
            {
                dup2(read_pipe, STDIN_FILENO);
            }

            // output pipe
            switch (cmd->symbol_type)
            {
            case pipe_ordinary:
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                break;
            }

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
            // can not fork more process (number limited)
            disable_sh();
            printf("fork error\n");
            // inorder to replay cmd
            cmd--;
            enable_sh();
        }
    }

    disable_sh();
    wait_for_cmdline();
    enable_sh();

    return;
}

void wait_for_cmdline()
{
    int status;
    for (auto proc : cur_process)
    {
        waitpid(proc.first, &status, 0);
    }
}