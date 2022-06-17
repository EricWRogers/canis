#pragma once
#include <iostream>
#include <string>

namespace Canis
{
  extern void FatalError(std::string message);

  extern void Error(std::string message);

  extern void Warning(std::string message);

  extern void Log(std::string message);
} // end of Canis namespace