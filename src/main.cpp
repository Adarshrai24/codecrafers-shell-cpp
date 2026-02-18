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
#include <fcntl.h>

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
  std::string word;
  bool in_single_quotes = false;
  bool in_double_quotes = false;
  for (int i = 0; i < line.size(); i++)
  {
    char c = line[i];

    if (c == '\\' && !in_single_quotes)
    {
      if (i < line.size()-1)
      {
        word += line[i+1];
        i++;
      }
    }
    else if (c == '\"' && !in_single_quotes)
    {
      if (!in_double_quotes)
      {
        if (i < line.size()-1 && line[i+1] == '\"')
        {
          i++;
        }
        else
        {
          in_double_quotes = true;
        }
      }
      else
      {
        if (i < line.size()-1 && line[i+1] == '\"')
        {
          i++;
        }
        else
        {
          in_double_quotes = false;
        }
      }
    }
    else if (c == '\'' && !in_double_quotes)
    {
      if (!in_single_quotes)
      {
        if (i < line.size() - 1 && line[i+1] == '\'')
        {
          i++;
        }
        else
        {
          in_single_quotes = true;
        }
      }
      else
      {
        if (i < line.size() - 1 && line[i+1] == '\'')
        {
          i++;
        }
        else
        {
          in_single_quotes = false;
        }
      }
    }
    else if (std::isspace(c) && (!in_single_quotes && !in_double_quotes))
    {
      if (!word.empty())
      {
        token.push_back(word);
        word.clear();
      }
    }
    else
    {
      word += c;
    }
  }

  if (!word.empty())
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

    int stdout_index = -1;
    int stderr_index = -1;
    std::string stdout_file;
    std::string stderr_file;

    for (int i = 0; i < tokens.size(); i++)
    {
        if ((tokens[i] == ">" || tokens[i] == "1>") && i + 1 < tokens.size())
        {
            stdout_index = i;
            stdout_file = tokens[i + 1];
        }
        else if (tokens[i] == "2>" && i + 1 < tokens.size())
        {
            stderr_index = i;
            stderr_file = tokens[i + 1];
        }
    }

    std::vector<std::string> clean_tokens;
    for (int i = 0; i < tokens.size(); i++) 
    {
      if ((stdout_index != -1 && (i == stdout_index || i-1 == stdout_index) || (stderr_index != -1 && (i == stderr_index || i-1 == stderr_index)))) 
      {
        continue;
      }
      clean_tokens.push_back(tokens[i]);
    }
    std::string command = clean_tokens[0];

    if (builtin.count(command))
    {
      int saved_stdout = -1;
      int saved_stderr = -1;
      if (stdout_index != -1)
      {
        int fd = open(stdout_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }

      if (stderr_index != -1) 
      {
        int fd = open(stderr_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        saved_stderr = dup(STDERR_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      }

      if (command == "exit")
      {
        break;
      }
      if (command == "echo")
      {
        for (int i = 1; i < clean_tokens.size(); i++)
        {
          std::cout << clean_tokens[i] << ' ';
        }
        std::cout << std::endl;
      }
      else if (command == "type")
      {
        std::string check = "";
        if (clean_tokens.size() > 1)
        {
          check = clean_tokens[1];
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
        if (clean_tokens.size() < 2)
        {
          continue;
        }
        if (clean_tokens[1] == "~")
        {
          clean_tokens[1] = getenv("HOME");
        }
        if (chdir(clean_tokens[1].c_str()) != 0)
        {
          std::cout << clean_tokens[1].c_str() << ": No such file or directory" << std::endl;
        }
      }

      if (stdout_index != -1)
      {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
      }
      if (stderr_index != -1)
      {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
      }
    }
    else
    {
      fs::path file_path = findPath(command);
      if (!file_path.empty())
      {
        std::vector<char *> argv;
        for (int i = 0; i < clean_tokens.size(); i++)
        {
          argv.push_back(const_cast<char *>(clean_tokens[i].c_str()));
        }
        argv.push_back(nullptr);

        pid_t pid = fork();
        if (pid == 0)
        {
          if (stdout_index != -1)
          {
            int fd = open(stdout_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
          }
          if (stderr_index != -1)
          {
            int fd = open(stderr_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, STDERR_FILENO);
            close(fd);
          }
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
