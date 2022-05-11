#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>

#include "App.hpp"

int main(int argc, char* argv[])
{
    App app;

    app.Run();

    return 0;
}