#include <Canis/Debug.hpp>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace Canis::Debug {
// helper function to print formatted messages
void PrintLog(const char* color, const char* prefix, const char* fmt, va_list args)
{
    //if (GetProjectConfig().log == false)
    //    return;

    printf("%s%s: \033[0m", color, prefix);
    vprintf(fmt, args);
    printf("\n");
}

void FatalError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintLog("\033[1;31m", "FatalError", fmt, args);
    va_end(args);

    printf("Press enter to quit");
    getchar();
    exit(1);
}

void Error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintLog("\033[1;31m", "Error", fmt, args);
    va_end(args);
}

void Warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintLog("\033[1;33m", "Warning", fmt, args);
    va_end(args);
}

void Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintLog("\033[1;32m", "Log", fmt, args);
    va_end(args);
}
}

