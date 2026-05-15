#include <Canis/Debug.hpp>

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace Canis::Debug {
namespace
{
    constexpr std::size_t kMaxLogEntries = 2000u;

    std::mutex& LogMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::vector<LogEntry>& LogEntries()
    {
        static std::vector<LogEntry> entries;
        return entries;
    }

    uint64_t& NextLogEntryId()
    {
        static uint64_t id = 1u;
        return id;
    }

    std::string FormatMessage(const char *_format, va_list _args)
    {
        if (_format == nullptr || _format[0] == '\0')
            return "";

        va_list lengthArgs;
        va_copy(lengthArgs, _args);
        const int requiredLength = std::vsnprintf(nullptr, 0, _format, lengthArgs);
        va_end(lengthArgs);

        if (requiredLength < 0)
            return _format;

        std::vector<char> buffer(static_cast<std::size_t>(requiredLength) + 1u, '\0');
        va_list formatArgs;
        va_copy(formatArgs, _args);
        std::vsnprintf(buffer.data(), buffer.size(), _format, formatArgs);
        va_end(formatArgs);
        return std::string(buffer.data(), static_cast<std::size_t>(requiredLength));
    }

    const char* LogColor(LogLevel _level)
    {
        switch (_level)
        {
            case LogLevel::Warning: return "\033[1;33m";
            case LogLevel::Error:
            case LogLevel::Fatal: return "\033[1;31m";
            case LogLevel::Log:
            default: return "\033[1;32m";
        }
    }

    void PrintLog(LogLevel _level, const std::string &_message)
    {
        //if (GetProjectConfig().log == false)
        //    return;

        printf("%s%s: \033[0m%s\n", LogColor(_level), LogLevelName(_level), _message.c_str());
    }

    void RecordLog(LogLevel _level, const LogFormat &_format, va_list _args)
    {
        const std::string message = FormatMessage(_format.format, _args);
        PrintLog(_level, message);

        std::scoped_lock lock(LogMutex());
        std::vector<LogEntry> &entries = LogEntries();
        LogEntry entry = {};
        entry.id = NextLogEntryId()++;
        entry.level = _level;
        entry.message = message;
        entry.file = (_format.file != nullptr) ? _format.file : "";
        entry.line = _format.line;
        entries.push_back(entry);

        if (entries.size() > kMaxLogEntries)
            entries.erase(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(entries.size() - kMaxLogEntries));
    }
}

void FatalError(LogFormat fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    RecordLog(LogLevel::Fatal, fmt, args);
    va_end(args);

    printf("Press enter to quit");
    getchar();
    exit(1);
}

void Error(LogFormat fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    RecordLog(LogLevel::Error, fmt, args);
    va_end(args);
}

void Warning(LogFormat fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    RecordLog(LogLevel::Warning, fmt, args);
    va_end(args);
}

void Log(LogFormat fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    RecordLog(LogLevel::Log, fmt, args);
    va_end(args);
}

std::vector<LogEntry> GetEntries()
{
    std::scoped_lock lock(LogMutex());
    return LogEntries();
}

void ClearEntries()
{
    std::scoped_lock lock(LogMutex());
    LogEntries().clear();
}

const char* LogLevelName(LogLevel level)
{
    switch (level)
    {
        case LogLevel::Warning: return "Warning";
        case LogLevel::Error: return "Error";
        case LogLevel::Fatal: return "Fatal";
        case LogLevel::Log:
        default: return "Log";
    }
}
}
