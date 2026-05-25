#include <iostream>
#include <string>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <algorithm>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "autocomplete.hpp"
#include "builtin.hpp"
#include "pathfind.hpp"
#include <array>

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
    else if (c == '|' && (!in_single_quotes && !in_double_quotes)) 
    {
        if (!word.empty()) 
        {
            token.push_back(word);
            word.clear();
        }
        token.push_back("|");
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
std::vector<std::vector<std::string>> history;
int last_append = 0;
bool executeCommand(
    std::vector<std::string> &clean_tokens,
    int stdout_index,
    int stderr_index,
    bool stdout_append,
    bool stderr_append,
    std::string &stdout_file,
    std::string &stderr_file
)
{
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
        char *histfile = std::getenv("HISTFILE");
        if (histfile != nullptr) {
            std::ofstream file(histfile);   
            for (auto &his : history) {
                for (int i = 0; i < his.size(); i++) {
                    if (i) file << ' ';
                    file << his[i];
                }
                file << std::endl;
            }
        }
        return false;
      }
      if (command == "echo")
      {
        for (int i = 1; i < clean_tokens.size(); i++)
        {
          std::cout << clean_tokens[i];
          if (i + 1 < clean_tokens.size()) 
          {
              std::cout << ' ';
          }
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
          return true;
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
      else if (command == "history") 
      {
          int limit_history = history.size();
          if (clean_tokens.size() > 2) {
              
              if (clean_tokens[1] == "-r") {
                  std::ifstream file(clean_tokens[2]);
                  std::string line;
                  while (std::getline(file, line))
                  {
                      if (line.empty())
                          continue;
                      history.push_back(parse(line));
                  }
              }
              else if (clean_tokens[1] == "-w") {
                  std::ofstream file(clean_tokens[2]);
                  for (auto &his : history) {
                      for (int i = 0; i < his.size(); i++) {
                          if (i) file << ' ';
                          file << his[i];
                      }
                      file << std::endl;
                  }
              } 
              else if (clean_tokens[1] == "-a") {
                  std::ofstream file(clean_tokens[2], std::ios::app);
                  for (int j = last_append; j < history.size(); j++) {
                      for (int i = 0; i < history[j].size(); i++) {
                          if (i) file << ' ';
                          file << history[j][i];
                      }
                      file << std::endl;
                  }
                  last_append = history.size();
              }
              return true;
          }
          else if (clean_tokens.size() > 1) {
              limit_history = std::stoi(clean_tokens[1]);
              if (limit_history < 0) {
                  std::cerr << "history : invalid argument" << std::endl;
                  return false;
              }
          }
          
          int start_index = std::max(0, (int)history.size() - limit_history);
          for (int i = start_index; i < history.size(); i++) {
              std::cout << i+1 << ' ';
              for (int j = 0; j < history[i].size(); j++) {
                  std::cout << history[i][j];
                  if (j+1 < history[i].size()) {
                      std::cout << ' ';
                  }
              }
              std::cout << std::endl;
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
        }
      }
      else
      {
        std::cout << command << ": command not found" << std::endl;
      }
    }

    return true;
}

int main()
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
   
  path_commands = getPathCommands();
  rl_bind_key('\t', rl_complete);
  rl_attempted_completion_function = completion;
    
  char *histfile = std::getenv("HISTFILE");

  if (histfile != nullptr) {
      std::ifstream file(histfile);

      std::string line;
      while (std::getline(file, line)) {
          if (line.empty())
              continue;
          history.push_back(parse(line));
      }
  }

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
    
    history.push_back(tokens);
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
    bool is_pipes = false;
    for (auto tk : clean_tokens) 
    {
        if (tk == "|") 
        {
            is_pipes = true;
            break;
        }
    }

    if (is_pipes) 
    {
        std::vector<std::vector<std::string>> commands;
        std::vector<std::string> current; 
        for (int i = 0; i < clean_tokens.size(); i++) 
        {
            if (clean_tokens[i] == "|")
            {
                commands.push_back(current);
                current.clear();
            }
            else 
            {
                current.push_back(clean_tokens[i]);
            }
        }
        if (!current.empty())
        {
            commands.push_back(current);
        }
        
        int n = commands.size();

        std::vector<std::array<int, 2>> pipes(n - 1);

        for (int i = 0; i < n - 1; i++)
        {
            if (pipe(pipes[i].data()) == -1)
            {
                perror("pipe");
                continue;
            }
        }

        std::vector<pid_t> pids;

        for (int i = 0; i < n; i++)
        {
            pid_t pid = fork();

            if (pid == 0)
            {
                if (i > 0)
                {
                    dup2(pipes[i - 1][0], STDIN_FILENO);
                }

                if (i < n - 1)
                {
                    dup2(pipes[i][1], STDOUT_FILENO);
                }

                for (int j = 0; j < n - 1; j++)
                {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                executeCommand(commands[i], stdout_index, stderr_index, stdout_append, stderr_append, stdout_file, stderr_file);  
                exit(0);
            }
            else if (pid > 0)
            {
                pids.push_back(pid);
            }
            else
            {
                perror("fork");
            }
        }

        for (int i = 0; i < n - 1; i++)
        {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        for (auto pid : pids)
        {
            waitpid(pid, nullptr, 0);
        }
    } 
    else 
    {
        if (!executeCommand(clean_tokens, stdout_index, stderr_index, stdout_append, stderr_append, stdout_file, stderr_file)) {
            break;
        }
    }

  }
}
