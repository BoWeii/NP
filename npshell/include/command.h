#ifndef _COMMAND_HPP
#define _COMMAND_HPP
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

// char special_symbols[] = {'>', '|', '!'};

typedef enum
{
    redirect, // 0
    pipe_ordinary, // 1
    pipe_numbered, // 2
} special_symbol_idx;

typedef struct cmd
{
    string name;
    vector<string> argv;
    int symbol_type, pipe_num;
    string file_name;
} cmd;

vector<string> cmd_read();
void cmd_parse(vector<string> cmdline);
void cmd_exec();
#endif