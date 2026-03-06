#include "autocomplete.hpp"
#include <readline/readline.h>
#include "builtin.hpp"
#include <cstring>

char *generator(const char *text, int state) 
{
    static auto it = builtin.begin();
    static size_t len;
    if (!state) 
    {
        it = builtin.begin();
        len = strlen(text);
    }

    while (it != builtin.end()) 
    {
        const std:: string &cmd = *it;
        ++it;
        if (cmd.compare(0, len, text) == 0) 
        {
            return strdup(cmd.c_str());
        }
    }

    return nullptr;
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
