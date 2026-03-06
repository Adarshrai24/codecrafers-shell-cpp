#include "builtin.hpp"
#include <vector>
#include <string>

const std::vector<std::string> builtin_list = {
    "exit",
    "type",
    "echo",
    "pwd",
    "cd"
};

const std::unordered_set<std::string> builtin(builtin_list.begin(), builtin_list.end());

