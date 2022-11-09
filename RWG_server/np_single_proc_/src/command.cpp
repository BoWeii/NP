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
#include <algorithm>
#include <string>

#include "sock.h"
#include "user.h"
#include "message.h"
#include "fd.h"
using namespace std;

// store the fd in use
// vector<np_fd_t> remain_fd;

// <which_pid, pipe to which number>
unordered_map<pid_t, int> cur_process;

sigset_t _sigset;
bool use_sh_wait = true;
const char special_symbols_icons[] = {'>', '|', 'x', '!', '<'};
#define MAX_LINE_SIZE 15000

#define handle_special_symbol(str, new_cmd)              \
    {                                                    \
        if ((str[0] == '|' && str.size() == 1))          \
        {                                                \
            new_cmd->symbol_type |= PIPE_ORDINARY;       \
            new_cmd->pipe_num = 1;                       \
            break;                                       \
        }                                                \
        else if (str[0] == '|' && str.size() != 1)       \
        {                                                \
            new_cmd->symbol_type |= PIPE_NUMBER_OUT;     \
            new_cmd->pipe_num = atoi(&str[1]);           \
            break;                                       \
        }                                                \
        else if (str[0] == '>' && str.size() == 1)       \
        {                                                \
            new_cmd->symbol_type |= REDIRECT;            \
            new_cmd->file_name = line[cur_idx++];        \
            continue;                                    \
        }                                                \
        else if (str[0] == '!' && str.size() != 1)       \
        {                                                \
            new_cmd->symbol_type |= PIPE_NUMBER_OUT_ERR; \
            new_cmd->pipe_num = atoi(&str[1]);           \
            break;                                       \
        }                                                \
        else if (str[0] == '>' && str.size() != 1)       \
        {                                                \
            new_cmd->symbol_type |= PIPE_USER_OUT;       \
            new_cmd->to_uid = atoi(&str[1]);             \
            continue;                                    \
        }                                                \
        else if (str[0] == '<' && str.size() != 1)       \
        {                                                \
            new_cmd->symbol_type |= PIPE_USER_IN;        \
            new_cmd->from_uid = atoi(&str[1]);           \
            continue;                                    \
        }                                                \
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

    /*
        tell and yell should parse like the below pattern
        -> tell 3 "hello    world"
        -> yell  "hello  world"
    */
    if (ret.size() && !ret[0].compare("tell"))
    {
        string cmd = ret[0];
        string who = ret[1];
        ret.clear();
        ret.push_back(cmd);
        ret.push_back(who);

        // find string after who's position
        size_t str_idx_start = line.find(ret[2], line.find(who) + who.size());
        string msg = line.substr(str_idx_start);
        ret.push_back(msg);
    }
    else if (ret.size() && !ret[0].compare("yell"))
    {
        string cmd = ret[0];
        ret.clear();
        ret.push_back(cmd);

        // find string after cmd's position
        size_t str_idx_start = line.find(ret[1], cmd.size());
        string msg = line.substr(str_idx_start);
        ret.push_back(msg);
    }

    return ret;
}

static bool is_built_in_cmd(vector<cmd_t> &cmds, user_t user)
{
    if (!cmds[0].name.compare("setenv"))
    {
        string var = cmds[0].argv[0];
        string value = cmds[0].argv[1];
        usr_set_env_var(user.id, var, value);

        return true;
    }
    else if (!cmds[0].name.compare("printenv"))
    {
        string var = cmds[0].argv[0];
        usr_print_env_var(user, var);

        return true;
    }
    else if (!cmds[0].name.compare("who"))
    {
        msg_who(user);
        return true;
    }
    else if (!cmds[0].name.compare("tell"))
    {
        char msg[MSG_SIZE_MAX] = {0};
        string who_str = cmds[0].argv[0];
        int who = atoi(&who_str[0]);
        string msg_str = cmds[0].argv[1];
        user_t target = usr_find_by_id(who);

        if (target.id != -1)
        {
            sprintf(msg, "*** %s told you ***: %s\n", user.name.c_str(), msg_str.c_str());
            msg_tell(target.sock_fd, msg);
        }
        else
        {
            sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", who);
            msg_tell(user.sock_fd, msg);
        }
        return true;
    }
    else if (!cmds[0].name.compare("yell"))
    {
        char msg[MSG_SIZE_MAX] = {0};
        string msg_str = cmds[0].argv[0];

        sprintf(msg, "*** %s yelled ***: %s\n", user.name.c_str(), msg_str.c_str());
        msg_broadcast(string(msg));
        return true;
    }
    else if (!cmds[0].name.compare("name"))
    {
        char msg[MSG_SIZE_MAX] = {0};
        string new_name = cmds[0].argv[0];

        user_t duplicate_user = usr_find_by_name(new_name);
        if (duplicate_user.id == -1)
        {
            usr_update_name(user.id, new_name);
            sprintf(msg, "*** User from %s:%d is named '%s'. ***\n", user.ip.c_str(), user.port, new_name.c_str());
            msg_broadcast(msg);
        }
        else
        {
            sprintf(msg, "*** User '%s' already exists. ***\n", new_name.c_str());
            msg_tell(user.sock_fd, msg);
        }

        return true;
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

    // for broadcast the raw string
    if (tokens.size())
    {
        tokens.push_back(cmdline);
    }

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
        new_cmd->symbol_type = 0;
        new_cmd->pipe_num = new_cmd->to_uid = new_cmd->from_uid = -1;
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

void cmd_exec(cmdline_t cmdline, user_t user)
{
    if (is_built_in_cmd(cmdline.cmds, user))
    {
        return;
    }

    pid_t pid;
    int cmd_idx = 0;
    int read_pipe = -1;
    np_fd_t target;
    np_fd_t pre_pipe = remainfd_find_by_lineidx(user, cmdline.line_idx);
    cur_process.clear();

    // output to/input from several pipes will not happened
    // The read user_pipe must occur at first cmd of cmdline
    // The write user_pipe must occur at last cmd of cmdline
    up_fd_t from_up, to_up;
    from_up.uid = to_up.uid = 0;

    // stdin from several pipe(num or user or ordinary) will not happened
    cmd_t first_cmd = cmdline.cmds[0];
    if (pre_pipe.target_line == -1 && IS_PIPE_USER_IN(first_cmd.symbol_type))
    {
        user_t from_user = usr_find_by_id(first_cmd.from_uid);
        if (from_user.id != -1)
        {
            // check the from_user sent a pipe to the user or not?
            from_up = upfd_find_by_uid(from_user, user.id);
            if (from_up.uid != -1)
            {
                // receive the from_up and broadcast
                msg_broadcast_recv_up(from_user, user, cmdline.raw);
            }
            else
            {
                // the from_user didn't pipe to the user
                msg_up_not_exist(from_user, user);
            }
        }
        else
        {
            // the user is not exist
            msg_user_not_exist(first_cmd.from_uid, user);
            from_up.uid = -1;
        }
    }

    enable_sh();
    for (auto cmd = cmdline.cmds.begin(); cmd != cmdline.cmds.end(); cmd++, cmd_idx++)
    {
        char **argv;

        // handle special symbol
        int cur_pipe[2] = {-1, -1}; // using for ordinary_pipe
        int out_pipe[2] = {-1, -1}; // using for number_pipe
        int file_fd = -1;

        if (IS_PIPE_ORDINARY(cmd->symbol_type))
        {
            if (pipe(cur_pipe))
            {
                printf("Error : pipe after cmd_exec\n");
            }
        }
        else if (IS_PIPE_NUMBER_OUT_ERR(cmd->symbol_type) || IS_PIPE_NUMBER_OUT(cmd->symbol_type))
        {
            if (pipe(out_pipe))
            {
                printf("Error : pipe after cmd_exec\n");
            }

            target = remainfd_find_by_lineidx(user, cmdline.line_idx + cmd->pipe_num);
            if (target.target_line == -1)
            {
                np_fd_t fd;
                fd.pipe_fd[0] = out_pipe[0];
                fd.pipe_fd[1] = out_pipe[1];
                fd.target_line = cmdline.line_idx + cmd->pipe_num;
                remainfd_add(user, fd);
            }
            else
            {
                out_pipe[0] = target.pipe_fd[0];
                out_pipe[1] = target.pipe_fd[1];
            }
        }
        else if (IS_REDIRECT(cmd->symbol_type))
        {
            file_fd = open(cmd->file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }
        else if (IS_PIPE_USER_OUT(cmd->symbol_type))
        {
            user_t to_user = usr_find_by_id(cmd->to_uid);
            if (to_user.id != -1)
            {
                to_up = upfd_find_by_uid(user, cmd->to_uid);
                if (to_up.uid == -1)
                {
                    to_up = upfd_add(user, cmd->to_uid);
                    msg_broadcast_recv_send(user, to_user, cmdline.raw);
                }
                else
                {
                    // the user pipe exist and not receive yet
                    msg_up_exist(user, to_user);
                    to_up.uid = -1;
                }
            }
            else
            {
                msg_user_not_exist(cmd->to_uid, user);
                to_up.uid = -1;
            }
        }

        // If the cmd is ready to receive number_pipe, close the write_pipe of both shell&cmd
        np_fd_t target = remainfd_find_by_lineidx(user, cmdline.line_idx);
        if (target.target_line != -1)
        {
            close(target.pipe_fd[1]);
        }

        // ready to fork
        if ((pid = fork()) > 0)
        {
            // parent process

            if (!(IS_PIPE_NUMBER_OUT(cmd->symbol_type) || IS_PIPE_NUMBER_OUT_ERR(cmd->symbol_type)))
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

            // close the shell's user pipe
            if (to_up.uid > 0 && to_up.pipe_fd[1] != -1)
            {
                close(to_up.pipe_fd[1]);
                to_up.pipe_fd[1] = -1;
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
            dup2(user.sock_fd, STDIN_FILENO);

            if (pre_pipe.target_line != -1 && cmd_idx == 0)
            {
                // check whether first cmd has been pipe_numbered or not
                dup2(pre_pipe.pipe_fd[0], STDIN_FILENO);
                remainfd_remove(user, pre_pipe);
            }
            else if (from_up.uid != 0)
            {
                // check whether the first cmd wanna read user_pipe or not
                if (from_up.uid == -1)
                {
                    int EOF_fd = open("/dev/null", O_RDONLY);
                    dup2(EOF_fd, STDIN_FILENO);
                }
                else
                {
                    dup2(from_up.pipe_fd[0], STDIN_FILENO);
                }
            }
            else if (read_pipe != -1)
            {
                dup2(read_pipe, STDIN_FILENO);
            }

            // output pipe
            dup2(user.sock_fd, STDOUT_FILENO);
            dup2(user.sock_fd, STDERR_FILENO);

            if (IS_PIPE_ORDINARY(cmd->symbol_type))
            {
                close(cur_pipe[0]);
                dup2(cur_pipe[1], STDOUT_FILENO);
            }
            else if (IS_PIPE_NUMBER_OUT_ERR(cmd->symbol_type))
            {
                close(out_pipe[0]);
                dup2(out_pipe[1], STDOUT_FILENO);
                dup2(out_pipe[1], STDERR_FILENO);
            }
            else if (IS_PIPE_NUMBER_OUT(cmd->symbol_type))
            {
                close(out_pipe[0]);
                dup2(out_pipe[1], STDOUT_FILENO);
            }
            else if (IS_REDIRECT(cmd->symbol_type))
            {
                dup2(file_fd, STDOUT_FILENO);
            }
            if (to_up.uid != 0)
            {
                // check whether the last cmd wanna write user_pipe or not
                if (to_up.uid == -1)
                {
                    int null_fd = open("/dev/null", O_WRONLY);
                    dup2(null_fd, STDOUT_FILENO);
                }
                else
                {
                    close(to_up.pipe_fd[0]);
                    dup2(to_up.pipe_fd[1], STDOUT_FILENO);
                }
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

            for (auto env : user.env_vars)
            {
                setenv(env.first.c_str(), env.second.c_str(), 1);
            }

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

    // remove the from_user's user_pipe to the user
    if (from_up.uid > 0)
    {
        upfd_remove(first_cmd.from_uid,user.id);
    }

    return;
}