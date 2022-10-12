#ifndef _COMMAND_HPP
#define _COMMAND_HPP
#include <vector>
#include <string>
using namespace std;


vector<string> cmd_read();
void cmd_parse(vector<string> cmds);
void cmd_exec();
#endif