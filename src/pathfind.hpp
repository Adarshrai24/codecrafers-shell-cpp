#pragma once
#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;
// finding the file path, in PATH environment variable
fs::path findPath(const std::string &filename);
std::vector<std::string> getPathCommands();
extern std::vector<std::string> path_commands;
