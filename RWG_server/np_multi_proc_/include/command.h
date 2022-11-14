#ifndef _COMMAND_H
#define _COMMAND_H
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

#define REDIRECT 0b00000001 << 0
#define PIPE_ORDINARY 0b00000001 << 1
#define PIPE_NUMBER_OUT 0b00000001 << 2
#define PIPE_NUMBER_OUT_ERR 0b00000001 << 3
#define PIPE_USER_IN 0b00000001 << 4
#define PIPE_USER_OUT 0b00000001 << 5

#define IS_REDIRECT(symbol_type) (symbol_type & REDIRECT)
#define IS_PIPE_ORDINARY(symbol_type) (symbol_type & PIPE_ORDINARY)
#define IS_PIPE_NUMBER_OUT(symbol_type) (symbol_type & PIPE_NUMBER_OUT)
#define IS_PIPE_NUMBER_OUT_ERR(symbol_type) (symbol_type & PIPE_NUMBER_OUT_ERR)
#define IS_PIPE_USER_IN(symbol_type) (symbol_type & PIPE_USER_IN)
#define IS_PIPE_USER_OUT(symbol_type) (symbol_type & PIPE_USER_OUT)

typedef struct cmd_t
{
    string name;
    vector<string> argv;
    int symbol_type, pipe_num;
    int to_uid, from_uid;
    string file_name;
} cmd_t;

typedef struct cmdline_t
{
    int line_idx;
    vector<cmd_t> cmds;
    string raw;
} cmdline_t;

vector<string> cmd_read();
void cmd_parse(cmdline_t &cmdline, vector<string> line);
void cmd_exec(cmdline_t cmdline);

vector<vector<string>> cmd_split_line(vector<string> tokens);
void sig_handler(int signum);
extern sigset_t _sigset;

#endif