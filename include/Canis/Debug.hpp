#pragma once

#include <cstdarg>
#include <cstdint>
#include <source_location>
#include <string>
#include <vector>

namespace Canis::Debug {
enum class LogLevel
{
    Log,
    Warning,
    Error,
    Fatal
};

struct LogFormat
{
    const char *format = "";
    const char *file = "";
    int line = 0;

    LogFormat(
        const char *_format,
        const std::source_location &_location = std::source_location::current()) :
        format(_format),
        file(_location.file_name()),
        line(static_cast<int>(_location.line()))
    {
    }
};

struct LogEntry
{
    uint64_t id = 0;
    LogLevel level = LogLevel::Log;
    std::string message = "";
    std::string file = "";
    int line = 0;
};

void FatalError(LogFormat fmt, ...);
void Error(LogFormat fmt, ...);
void Warning(LogFormat fmt, ...);
void Log(LogFormat fmt, ...);

std::vector<LogEntry> GetEntries();
void ClearEntries();
const char* LogLevelName(LogLevel level);
}
