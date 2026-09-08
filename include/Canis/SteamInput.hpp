#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace Canis {
class InputActionSystem;
namespace SteamInputPlatform {
// Initialize before SDL so its Steam cooperation can identify emulated devices.
bool Initialize();
void Shutdown();
void Poll(InputActionSystem& actions, const std::vector<uint64_t>& sdlSteamHandles);
bool OwnsController(uint64_t steamHandle);
bool ShowBindingPanel();
const char* Status();
}
}
