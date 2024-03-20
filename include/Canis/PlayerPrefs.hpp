#pragma once
#include <string>

namespace Canis
{
    namespace PlayerPrefs
    {
        extern bool HasKey(const std::string &_key);

        extern std::string GetString(const std::string &_key, const std::string &_defaultValue);
        extern void SetString(const std::string &_key, const std::string &_value);

        extern int GetInt(const std::string &_key, int _defaultValue);
        extern void SetInt(const std::string &_key, int _value);

        extern bool GetBool(const std::string &_key, bool _defaultValue);
        extern void SetBool(const std::string &_key, bool _value);

        extern void LoadFromFile();
        extern void SaveToFile();
    }
}