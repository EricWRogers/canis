#include <Canis/PlayerPrefs.hpp>
#include <Canis/Debug.hpp>
#include <map>
#include <SDL.h>

namespace Canis
{
    namespace PlayerPrefs
    {
        static std::string PLAYERDATAPATH = "";

        std::map<std::string,std::string> &GetPlayerData()
        {
            static std::map<std::string,std::string> data = {};
            return data;
        }

        bool HasKey(const std::string &_key)
        {
            return GetPlayerData().contains(_key);
        }

        std::string GetString(const std::string &_key, const std::string &_defaultValue)
        {
            std::map<std::string,std::string>& playerData = GetPlayerData();
            
            if (playerData.contains(_key))
            {
                return playerData[_key];
            }

            SetString(_key, _defaultValue);
            return _defaultValue;
        }

        void SetString(const std::string &_key, const std::string &_value)
        {
            std::map<std::string,std::string>& playerData = GetPlayerData();

            playerData[_key] = _value;
        }

        int  GetInt(const std::string &_key, int _defaultValue)
        {
            std::map<std::string,std::string>& playerData = GetPlayerData();

            if (playerData.contains(_key))
            {
                char* endPtr;
                long longValue = std::strtol(playerData[_key].c_str(), &endPtr, 10);

                if (*endPtr != '\0') {
                    SetInt(_key, _defaultValue);
                    return _defaultValue;
                }

                if (longValue > INT_MAX || longValue < INT_MIN) {
                    SetInt(_key, _defaultValue);
                    return _defaultValue;
                }

                return static_cast<int>(longValue);
            }

            SetInt(_key, _defaultValue);
            return _defaultValue;
        }

        void SetInt(const std::string &_key, int _value)
        {
            std::map<std::string,std::string>& playerData = GetPlayerData();

            playerData[_key] = std::to_string(_value);
        }

        bool GetBool(const std::string &_key, bool _defaultValue)
        {
            std::map<std::string,std::string>& playerData = GetPlayerData();
            
            if (playerData.contains(_key))
            {
                std::string value = playerData[_key];

                if (value == "true")
                {
                    return true;
                }
                else if (value == "false")
                {
                    return false;
                }
                else
                {
                    SetBool(_key, false);
                    return false;
                }
            }

            SetBool(_key, _defaultValue);
            return _defaultValue;
        }

        void SetBool(const std::string &_key, bool _value)
        {
            std::map<std::string,std::string>& playerData = GetPlayerData();

            playerData[_key] = (_value) ? "true" : "false";
        }

        void LoadFromFile()
        {
            Canis::Log("Loading Player Data");

            std::map<std::string,std::string>& playerData = GetPlayerData();

            SDL_RWops* file = SDL_RWFromFile((PLAYERDATAPATH+"player.data").c_str(), "r");

            if (file == nullptr) {
                Canis::Log("Player Data Not Found.");
                return;
            }

            playerData.clear();
            char ch;
            std::string key, value;
            bool readingKey = true;
            while (SDL_RWread(file, &ch, 1, 1) > 0) {
                if (ch == '|') {
                    readingKey = false;
                } else if (ch == '\n') {
                    playerData[key] = value;
                    key.clear();
                    value.clear();
                    readingKey = true;
                } else {
                    if (readingKey) {
                        key += ch;
                    } else {
                        value += ch;
                    }
                }
            }

            for(const auto& pair : playerData)
            {
                Canis::Log("{ " + pair.first + " | " + pair.second + " }");
            }

            SDL_RWclose(file);
        }

        void SaveToFile()
        {
            Canis::Log("Saving Player Data");

            std::map<std::string,std::string>& playerData = GetPlayerData();

            SDL_RWops *file = SDL_RWFromFile((PLAYERDATAPATH+"player.data").c_str(), "w+");

            if (file == nullptr)
            {
                Canis::Warning("Fail to open player data for saving.");
                return;
            }

            for(const auto& pair : playerData)
            {
                std::string line = pair.first + "|" + pair.second + "\n";
                SDL_RWwrite(file, line.c_str(), 1, line.size());
            }

            SDL_RWclose(file);
        }

        void Init(const std::string &_organization, const std::string &_app)
        {
            PLAYERDATAPATH = std::string(SDL_GetPrefPath(_organization.c_str(), _app.c_str()));
        }
    }
}