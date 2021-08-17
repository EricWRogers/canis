#pragma once

#include <iostream>
#include <SDL2/SDL.h>
#include <string>

namespace canis
{

    void FatalError(std::string message);

    void Error(std::string message);

    void Warning(std::string message);

    void Log(std::string message);

} // end of canis namespace