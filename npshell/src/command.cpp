#include "command.h"
#include <stdio.h>
#include <iostream>
#include <sstream>

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

    vector<string>tokens = tokenize(cmdline);
    for (auto token : tokens)
    {
        cout << token << endl;
    }
    return tokens;
}