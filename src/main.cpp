#include <iostream>
#include <string>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;
// finding the file path, in PATH environment variable
fs::path findPath(const std::string &filename)
{
  std::string path_env = std::getenv("PATH");
#ifdef _WIN32
  const char delimiter = ';';
#else
  const char delimiter = ':';
#endif

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

std::vector<std::string> parse(const std::string &line)
{
  std::vector<std::string> token;
  std::istringstream iss(line);
  std::string word;
  while (iss >> word)
  {
    token.push_back(word);
  }
  return token;
}

int main()
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::unordered_set<std::string> builtin = {"exit", "type", "echo", "pwd", "cd"}; // builtin commands list
  while (true)
  {
    std::cout << "$ ";
    std::string input;
    std::getline(std::cin, input);
    auto tokens = parse(input);
    if (tokens.empty())
    {
      continue;
    }
    std::string command = tokens[0];
    if (builtin.count(command))
    {
      if (command == "exit")
      {
        break;
      }
      if (command == "echo")
      {
        for (int i = 1; i < tokens.size(); i++)
        {
          std::cout << tokens[i] << ' ';
        }
        std::cout << std::endl;
      }
      else if (command == "type")
      {
        std::string check = "";
        if (tokens.size() > 1)
        {
          check = tokens[1];
        }
        if (builtin.count(check))
        {
          std::cout << check << ' ' << "is a shell builtin" << std::endl;
        }
        else
        {
          fs::path file_path = findPath(check);
          if (!file_path.empty())
          {
            std::cout << check << " is " << file_path.string() << std::endl;
          }
          else
          {
            std::cout << check << ": not found" << std::endl;
          }
        }
      }
      else if (command == "pwd")
      {
        std::cout << fs::current_path().c_str() << std::endl;
      }
      else if (command == "cd")
      {
        if (tokens.size() < 2) 
        {
          continue;
        }
        if (tokens[1] == "~") 
        {
          tokens[1] = getenv("HOME");
        }
        if (chdir(tokens[1].c_str()) != 0)
        {
          std::cout << tokens[1].c_str() << ": No such file or directory" << std::endl;
        }
      }
    }
    else
    {
      fs::path file_path = findPath(command);
      if (!file_path.empty())
      {
        std::vector<char *> argv;
        for (int i = 0; i < tokens.size(); i++)
        {
          argv.push_back(const_cast<char *>(tokens[i].c_str()));
        }
        argv.push_back(nullptr);
        pid_t pid = fork();
        if (pid == 0)
        {
          execvp(file_path.c_str(), argv.data());
          perror("exec failed");
          exit(1);
        }
        else if (pid > 0)
        {
          int status;
          waitpid(pid, &status, 0);
        }
        else
        {
          perror("fork failed");
          continue;
        }
      }
      else
      {
        std::cout << command << ": command not found" << std::endl;
      }
    }
  }
}
