#include <iostream>

#include "App.hpp"

int main(int argc, char *argv[])
{
    unsigned int myBits = 0x00000010;
    std::cout << myBits << std::endl;
    App app;

    app.Run();

    return 0;
}