#pragma once

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace Canis
{
    // Floating-point charconv is unavailable in the Catalina/Monterey libc++.
    // Classic-locale streams keep saves and network numbers independent of
    // the player's decimal separator without requiring a newer system library.
    template <typename T>
    bool ParseFiniteFloat(std::string_view text, T& value)
    {
        if (text.empty() || text.front() == '+') return false;
        std::istringstream stream{std::string(text)};
        stream.imbue(std::locale::classic());
        // Parse with extra precision so float max/denorm round trips do not
        // trigger libc++'s float stream overflow/underflow handling.
        long double parsed{};
        stream >> std::noskipws >> parsed;
        if (stream.fail() || !stream.eof() || !std::isfinite(parsed)) return false;
        const T converted = static_cast<T>(parsed);
        if (!std::isfinite(converted) || (parsed != 0 && converted == 0)) return false;
        value = converted;
        return true;
    }

    inline std::string FormatFiniteFloat(float value)
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
        return stream.str();
    }
}
