#pragma once
#include <string>
#include <string_view>

namespace Canis::Scripting
{
    // Separate wasm memories exchange copied, typed values, never pointers.
    std::string DispatchWebBinding(std::string_view request) noexcept;
}
