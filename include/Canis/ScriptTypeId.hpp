#pragma once

#include <Canis/Data/Types.hpp>

namespace Canis
{
    using ScriptTypeId = u64;

    constexpr ScriptTypeId ScriptTypeIdFromName(const char* _name)
    {
        if (_name == nullptr)
            return 0;

        ScriptTypeId hash = 14695981039346656037ull;
        while (*_name != '\0')
        {
            hash ^= static_cast<ScriptTypeId>(static_cast<unsigned char>(*_name));
            hash *= 1099511628211ull;
            ++_name;
        }
        return hash;
    }
}
