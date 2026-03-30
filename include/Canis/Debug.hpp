#pragma once
#include <cstdarg>

namespace Canis::Debug {
void FatalError(const char* fmt, ...);
void Error(const char* fmt, ...);
void Warning(const char* fmt, ...);
void Log(const char* fmt, ...);
}
