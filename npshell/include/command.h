#ifndef _COMMAND_H
#define _COMMAND_H
#include <vector>
#include <string>
using namespace std;

typedef enum
{
    redirect,              // 0
    pipe_ordinary,         // 1
    pipe_numbered_out,     // 2
    pipe_numbered_out_err, // 3
} special_symbol_idx;

typedef struct cmt_t
{
    string name;
    vector<string> argv;
    int symbol_type, pipe_num;
    string file_name;
} cmt_t;

typedef struct cmdline_t
{
    int line_idx;
    vector<cmt_t> cmds;
} cmdline_t;




vector<string> cmd_read();
void cmd_parse(cmdline_t &cmdline, vector<string> line);
void cmd_exec(cmdline_t cmdline);

vector<vector<string>> cmd_split_line(vector<string> tokens);
void sig_handler(int signum);
void wait_for_cmdline();

#endif