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
#include <readline/readline.h>
#include <readline/history.h>
#include "autocomplete.hpp"
#include "builtin.hpp"
#include "pathfind.hpp"

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

std::vector<std::string> path_commands;

int main()
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
   
  path_commands = getPathCommands();
  rl_bind_key('\t', rl_complete);
  rl_attempted_completion_function = completion;

  while (true)
  {
    char *line = readline("$ ");
    if (!line) {
        break;
    }
    if (*line) {
        add_history(line);
    }
    std::string input(line);
    free(line);
    auto tokens = parse(input);
    if (tokens.empty())
    {
      continue;
    }

    int stdout_index = -1;
    int stderr_index = -1;
    bool stdout_append = false;
    bool stderr_append = false;
    std::string stdout_file;
    std::string stderr_file;

    for (int i = 0; i < tokens.size(); i++)
    {
        if ((tokens[i] == ">" || tokens[i] == "1>") && i + 1 < tokens.size())
        {
            stdout_index = i;
            stdout_file = tokens[i + 1];
        }
        else if ((tokens[i] == ">>" || tokens[i] == "1>>") && i + 1 < tokens.size())
        {
            stdout_index = i;
            stdout_file = tokens[i + 1];
            stdout_append = true;
        }
        else if (tokens[i] == "2>" && i + 1 < tokens.size())
        {
            stderr_index = i;
            stderr_file = tokens[i + 1];
        }
        else if (tokens[i] == "2>>" && i + 1 < tokens.size())
        {
            stderr_index = i;
            stderr_file = tokens[i + 1];
            stderr_append = true;
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
        int flags = O_WRONLY | O_CREAT | (stdout_append ? O_APPEND : O_TRUNC);
        int fd = open(stdout_file.c_str(), flags, 0644);
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }

      if (stderr_index != -1) 
      {
        int flags = O_WRONLY | O_CREAT | (stderr_append ? O_APPEND : O_TRUNC);
        int fd = open(stderr_file.c_str(), flags, 0644);
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
            int flags = O_WRONLY | O_CREAT | (stdout_append ? O_APPEND : O_TRUNC);
            int fd = open(stdout_file.c_str(), flags, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
          }
          if (stderr_index != -1)
          {
            int flags = O_WRONLY | O_CREAT | (stderr_append ? O_APPEND : O_TRUNC);
            int fd = open(stderr_file.c_str(), flags, 0644);
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
