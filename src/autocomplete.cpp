#include "autocomplete.hpp"
#include <readline/readline.h>
#include "builtin.hpp"
#include <cstring>
#include "pathfind.hpp"

char *generator(const char *text, int state) 
{
    static size_t index;
    static size_t len;
    bool phase; // 0 = builtins, 1 = PATH commands
    if (!state) 
    {
        index = 0;
        len = strlen(text);
        phase = 0;
    }

    while (true)
    {
        if (!phase) 
        {
            while (index < builtin_list.size())
            {
                const std::string &cmd = builtin_list[index++];
                if (!cmd.compare(0, len, text))
                    return strdup(cmd.c_str());
            }
            phase = 1;
            index = 0;
        }

        if (phase)
        {
            while (index < path_commands.size())
            {
                const std::string &cmd = path_commands[index++];
                if (!cmd.compare(0, len, text))
                    return strdup(cmd.c_str());
            }
            return nullptr;
        }
    }
}

char **completion(const char *text, int start, int end)
{
    if (start == 0) 
    {
        rl_attempted_completion_over = 1;
        rl_completion_append_character = ' ';
        return rl_completion_matches(text, generator);
    }
    return nullptr;
}
