#include <string>
#include <filesystem>
#include "pathfind.hpp"
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <unordered_set>

namespace fs = std::filesystem;

#ifdef _WIN32
  const char delimiter = ';';
#else
  const char delimiter = ':';
#endif

// finding the file path, in PATH environment variable
fs::path findPath(const std::string &filename)
{
  std::string path_env = std::getenv("PATH");
  int start = 0, end = path_env.find(delimiter);
  while (end != std::string::npos)
  {
    fs::path dir = path_env.substr(start, end - start);
    fs::path file_path = dir / filename;
    if (fs::exists(file_path) && access(file_path.c_str(), X_OK) == 0)
    {
      return file_path;
    }
    start = end + 1;
    end = path_env.find(delimiter, start);
  }
  return {};
}

std::vector<std::string> getPathCommands() 
{
    std::unordered_set<std::string> uniq;
    std::vector<std::string> commands;
    std::string path_env = std::getenv("PATH");
    int start = 0, end = path_env.find(delimiter);
    while (end != std::string::npos)
    {
        fs::path dir = path_env.substr(start, end - start);
        if (fs::exists(dir))
        {
            for (auto &entry : fs::directory_iterator(dir))
            {
                fs::path p = entry.path();
#ifndef _WIN32
                if(!fs::is_directory(p) && access(p.c_str(), X_OK) == 0)
                    uniq.insert(p.filename().string());
#else
                std::string ext = p.extension().string();
                if (ext == ".exe" || ext == ".bat" || ext == ".cmd")
                    uniq.insert(p.stem().string());
#endif
            }
        }
        start = end+1;
        end = path_env.find(delimiter, start);
    }
    commands.assign(uniq.begin(), uniq.end());
    return commands;
}
