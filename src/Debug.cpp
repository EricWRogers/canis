#include <Canis/Debug.hpp>
#include <Canis/Canis.hpp>

// https://www.codegrepper.com/code-examples/cpp/c%2B%2B+cout+with+color

namespace Canis
{
    #ifdef _WIN32
    const char* NULLLOGTARGET = "NUL";
    const char* DEFAULTLOGTARGET = "CON";
    #else
    const char* NULLLOGTARGET = "/dev/null";
    const char* DEFAULTLOGTARGET = "/dev/tty";
    #endif

    void FatalError(std::string _message)
    {
        if (GetProjectConfig().log == false)
            return;
        // \033[1;31m red \033[0m reset
        std::cout << "\033[1;31mFatalError: \033[0m" + _message << std::endl;
        std::cout << "Press enter to quit";
        int tmp;
        std::cin.get();
        exit(1);
    }

    void Error(std::string _message)
    {
        if (GetProjectConfig().log == false)
            return;
        // \033[1;31m red \033[0m reset
        std::cout << "\033[1;31mError: \033[0m" + _message << std::endl;
    }

    void Warning(std::string _message)
    {
        if (GetProjectConfig().log == false)
            return;
        // \033[1;33m yellow \033[0m reset
        std::cout << "\033[1;33mWarning: \033[0m" + _message << std::endl;
    }

    void Log(std::string _message)
    {
        if (GetProjectConfig().log == false)
            return;
        // \033[1;32m green \033[0m reset
        std::cout << "\033[1;32mLog: \033[0m" + _message << std::endl;
    }

    void TurnOffLog()
    {
        if (GetLoggingData().logFile) fclose((FILE*)GetLoggingData().logFile);
        if (GetLoggingData().logFileError) fclose((FILE*)GetLoggingData().logFileError);

        GetLoggingData().logFile = freopen(NULLLOGTARGET, "w", stdout);
        GetLoggingData().logFileError = freopen(NULLLOGTARGET, "w", stderr);
    }

    void TurnOnLog()
    {
        if (GetLoggingData().logFile) fclose((FILE*)GetLoggingData().logFile);
        if (GetLoggingData().logFileError) fclose((FILE*)GetLoggingData().logFileError);

        if (GetLoggingData().logTarget == "")
        {
            GetLoggingData().logFile = freopen(DEFAULTLOGTARGET, "w", stdout);
            GetLoggingData().logFileError = freopen(DEFAULTLOGTARGET, "w", stderr);
        }
        else
        {
            GetLoggingData().logFile = freopen(GetLoggingData().logTarget.c_str(), "a", stdout);
            GetLoggingData().logFileError = freopen(GetLoggingData().logTarget.c_str(), "a", stderr);
        }
    }

    void SetLogTarget(std::string _path)
    {
        GetLoggingData().logTarget = _path;
        TurnOnLog();
    }

    LoggingData& GetLoggingData()
    {
        static LoggingData loggingData = {};
        return loggingData;
    }
} // end of Canis namespace