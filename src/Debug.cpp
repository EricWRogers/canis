#include "Debug.h"

namespace canis
{

    void FatalError(std::string message)
    {
        std::cout << "\033[1;31mFatalError: \033[0m" + message << std::endl;
        std::cout << "Enter any key to quit";
        int tmp;
        std::cin >> tmp;
        SDL_Quit();
        exit(1);
    }

    void Error(std::string message)
    {
        std::cout << "\033[1;31mError: \033[0m" + message << std::endl;
    }

    void Warning(std::string message)
    {
        std::cout << "\033[1;33mWarning: \033[0m" + message << std::endl;
    }

    void Log(std::string message)
    {
        std::cout << "\033[1;32mLog: \033[0m" + message << std::endl;
    }

} // end of canis namespace