#include "command.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unordered_map>
#include <cstring>
#include <unordered_set>
#include <fcntl.h>
#include "sock.h"
// store the fd in use
vector<fd_t> remain_fd;

// <which_pid, pipe to which number>
unordered_map<pid_t, int> cur_process;

sigset_t _sigset;
bool use_sh_wait = true;
const char special_symbols_icons[] = {'>', '|', 'x', '!'};
#define MAX_LINE_SIZE 15000

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

static bool is_built_in_cmd(vector<cmd_t> &cmds, int client_sock)
{
    if (!cmds[0].name.compare("setenv"))
    {
        string var = cmds[0].argv[0];
        string value = cmds[0].argv[1];

        setenv(var.c_str(), value.c_str(), 1);

        return true;
    }
    else if (!cmds[0].name.compare("printenv"))
    {
        string var = cmds[0].argv[0];
        char *value = getenv(var.c_str());

        if (value)
        {
            sock_write(client_sock, value, strlen(value));
            sock_write(client_sock, "\n", 1);
        }

        return true;
    }
    else if (!cmds[0].name.compare("exit"))
    {
        exit(0);
    }

    return false;
}

void sig_handler(int signum)
{
    pid_t pid;
    if (!use_sh_wait)
    {
        return;
    }
    pid = wait(NULL);
    if (cur_process.find(pid) != cur_process.end())
    {
        cur_process.erase(pid);
    }
}

// disable signal handler
static void disable_sh()
{
    use_sh_wait = false;
}

// enable signal handler
static void enable_sh()
{
    use_sh_wait = true;
}

static void cur_proc_insert(pid_t pid, int pipe_num)
{
    sigset_t oldset;
    sigprocmask(SIG_BLOCK, &_sigset, &oldset);
    cur_process.insert(pair<pid_t, int>(pid, pipe_num));
    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

static void remain_fd_remove(fd_t target)
{
    for (auto fd = remain_fd.begin(); fd != remain_fd.end(); fd++)
    {
        if (target.target_line == fd->target_line)
        {
            remain_fd.erase(fd);
            return;
        }
    }
}

fd_t get_fd_by_line_idx(int line_idx)
{
    fd_t ret;
    // check exist or not;
    ret.target_line = -1;

    for (auto fd : remain_fd)
    {
        if (line_idx == fd.target_line)
        {
            ret.pipe_fd[0] = fd.pipe_fd[0];
            ret.pipe_fd[1] = fd.pipe_fd[1];
            ret.target_line = fd.target_line;
        }
    }
    return ret;
}

static void wait_cur_proc()
{
    int status;
    for (auto proc : cur_process)
    {
        waitpid(proc.first, &status, 0);
    }
}

// function using in shell
vector<string> cmd_read(int client_sock)
{
    string cmdline = "";
    sock_getline(client_sock, cmdline, MAX_LINE_SIZE);
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
    return ret;
}

void cmd_parse(cmdline_t &cmdline, vector<string> line)
{
    vector<cmd_t> cmds;
    bool is_ready_counter_cmd = true;
    size_t cur_idx = 0;
    string cur_token = "";
    cmd_t *new_cmd;

    while (cur_idx < line.size())
    {
        new_cmd = new cmd_t();
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

void cmd_exec(cmdline_t cmdline, int client_sock)
{
    if (is_built_in_cmd(cmdline.cmds, client_sock))
    {
        return;
    }

    pid_t pid;
    int cmd_idx = 0;
    int read_pipe = -1;
    fd_t pre_pipe, target;

    cur_process.clear();

    // If the first cmd is ready to receive number_pipe, close the write_pipe of both shell and first cmd
    pre_pipe = get_fd_by_line_idx(cmdline.line_idx);
    if (pre_pipe.target_line != -1)
    {
        close(pre_pipe.pipe_fd[1]);
    }

    enable_sh();
    for (auto cmd = cmdline.cmds.begin(); cmd != cmdline.cmds.end(); cmd++, cmd_idx++)
    {
        char **argv;

        // handle special symbol
        int cur_pipe[2] = {-1, -1}; // using for ordinary_pipe
        int out_pipe[2] = {-1, -1}; // using for number_pipe
        int file_fd = -1;

        switch (cmd->symbol_type)
        {
        case pipe_ordinary:
            if (pipe(cur_pipe))
            {
                printf("Error : pipe after cmd_exec\n");
            }
            break;
        case pipe_numbered_out_err:
        case pipe_numbered_out:
            if (pipe(out_pipe))
            {
                printf("Error : pipe after cmd_exec\n");
            }

            target = get_fd_by_line_idx(cmdline.line_idx + cmd->pipe_num);
            if (target.target_line == -1)
            {
                fd_t fd;
                fd.pipe_fd[0] = out_pipe[0];
                fd.pipe_fd[1] = out_pipe[1];
                fd.target_line = cmdline.line_idx + cmd->pipe_num;
                remain_fd.push_back(fd);
            }
            else
            {
                out_pipe[0] = target.pipe_fd[0];
                out_pipe[1] = target.pipe_fd[1];
            }
            break;
        case redirect:
            file_fd = open(cmd->file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        default:
            break;
        }

        // ready to fork
        if ((pid = fork()) > 0)
        {
            // parent process

            if (!(cmd->symbol_type == pipe_numbered_out || cmd->symbol_type == pipe_numbered_out_err))
            {
                cur_proc_insert(pid, cmd->pipe_num);
            }

            // input pipe
            if (read_pipe != -1)
            {
                close(read_pipe);
            }

            // only close ordinary output_pipe and save read_pipe to next cmd
            if (cur_pipe[1] != -1)
            {
                close(cur_pipe[1]);
                read_pipe = cur_pipe[0]; // assign for next cmd read_pipe
            }
            else
            {
                read_pipe = -1;
            }

            // redirect
            if (file_fd != -1)
            {
                close(file_fd);
            }
        }
        else if (pid == 0)
        {
            // child process

            // input pipe
            dup2(client_sock, STDIN_FILENO);

            // check first cmd has been pipe_numbered or not
            if (pre_pipe.target_line != -1 && cmd_idx == 0)
            {
                dup2(pre_pipe.pipe_fd[0], STDIN_FILENO);
            }
            else if (read_pipe != -1)
            {
                dup2(read_pipe, STDIN_FILENO);
            }

            // output pipe
            dup2(client_sock, STDOUT_FILENO);
            dup2(client_sock, STDERR_FILENO);

            switch (cmd->symbol_type)
            {
            case pipe_ordinary:
                close(cur_pipe[0]);
                dup2(cur_pipe[1], STDOUT_FILENO);
                break;
            case pipe_numbered_out_err:
                close(out_pipe[0]);
                dup2(out_pipe[1], STDOUT_FILENO);
                dup2(out_pipe[1], STDERR_FILENO);
            case pipe_numbered_out:
                close(out_pipe[0]);
                dup2(out_pipe[1], STDOUT_FILENO);
                break;
            case redirect:
                dup2(file_fd, STDOUT_FILENO);
            default:
                break;
            }

            // processing argv
            int argv_size = cmd->argv.size() + 2;
            int argv_idx = 1;

            argv = (char **)malloc(sizeof(char *) * argv_size);
            argv[0] = &(cmd->name[0]);
            for (size_t i = 0; i < cmd->argv.size(); i++)
            {
                argv[argv_idx++] = &(cmd->argv[i][0]);
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

            pid_t pid2 = wait(NULL);
            cur_process.erase(pid2);

            // inorder to replay cmd
            cmd--;
            cmd_idx--;

            enable_sh();
        }
    }

    disable_sh();
    wait_cur_proc();
    enable_sh();
    
    // remove the number_pipe's fd which already recieved
    if (pre_pipe.target_line != -1)
    {
        remain_fd_remove(pre_pipe);
    }
    return;
}