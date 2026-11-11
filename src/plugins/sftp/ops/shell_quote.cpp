#include "shell_quote.h"

std::string QuoteForPosixShell(const std::string& text)
{
    std::string quoted = "'";
    for (const char c : text)
    {
        if (c == '\'')
            quoted += "'\\''";
        else
            quoted += c;
    }
    quoted += '\'';
    return quoted;
}

std::string BuildRemoteCommand(const std::string& currentDirRaw, const std::string& userCommand)
{
    if (currentDirRaw.empty())
        return userCommand;
    return "cd -- " + QuoteForPosixShell(currentDirRaw) + " && { " + userCommand + "\n}";
}
