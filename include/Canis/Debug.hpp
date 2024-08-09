#pragma once
#include <iostream>
#include <string>

namespace Canis
{
  struct LoggingData {
    std::string logTarget = "";
    int count = 0;
    void *logFile = nullptr;
    void *logFileError = nullptr;
  };

  extern void FatalError(std::string _message);

  extern void Error(std::string _message);

  extern void Warning(std::string _message);

  extern void Log(std::string _message);

  extern void TurnOffLog();

  extern void TurnOnLog();

  extern void SetLogTarget(std::string _path);

  extern LoggingData& GetLoggingData();
} // end of Canis namespace