#ifndef _COMMAND_HPP
#define _COMMAND_HPP
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;


typedef enum
{
    redirect,              // 0
    pipe_ordinary,         // 1
    pipe_numbered_out,     // 2
    pipe_numbered_out_err, // 3
} special_symbol_idx;

typedef struct s_cmd
{
    string name;
    vector<string> argv;
    int symbol_type, pipe_num;
    string file_name;
} s_cmd;

typedef struct s_cmdline
{
    int line_idx;
    vector<s_cmd> cmds;
} s_cmdline;

vector<string> cmd_read();
void cmd_parse(s_cmdline &cmdline, vector<string> line);
void cmd_exec(s_cmdline cmdline);

vector<vector<string>> cmd_split_line(vector<string> tokens);
#endif