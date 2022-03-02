#include "Debug.hpp"

// https://www.codegrepper.com/code-examples/cpp/c%2B%2B+cout+with+color

namespace Canis
{

  void FatalError(std::string message)
  {
    // \033[1;31m red \033[0m reset
    std::cout << "\033[1;31mFatalError: \033[0m" + message << std::endl;
    std::cout << "Press enter to quit";
    int tmp;
    std::cin.get();
    exit(1);
  }

  void Error(std::string message)
  {
    // \033[1;31m red \033[0m reset
    std::cout << "\033[1;31mError: \033[0m" + message << std::endl;
  }

  void Warning(std::string message)
  {
    // \033[1;33m yellow \033[0m reset
    std::cout << "\033[1;33mWarning: \033[0m" + message << std::endl;
  }

  void Log(std::string message)
  {
    // \033[1;32m green \033[0m reset
    std::cout << "\033[1;32mLog: \033[0m" + message << std::endl;
  }

} // end of Canis namespace