#include <Canis/PlayerPrefs.hpp>
#include <Canis/FloatConversion.hpp>
#include <SDL3/SDL.h>
#include <yaml-cpp/yaml.h>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>

namespace Canis::PlayerPrefs
{
    namespace
    {
        std::filesystem::path path;
        std::map<std::string, std::string> values;
        bool dirty = false;
    }

    void Init(const std::string& organization, const std::string& app, const std::string& file)
    {
        path.clear();
        values.clear();
        dirty = false;
        if (!file.empty())
            path = file;
        else
        {
            char* directory = SDL_GetPrefPath(organization.c_str(), app.c_str());
            if (!directory)
                throw std::runtime_error(SDL_GetError());
            path = std::filesystem::path(directory) / "player-prefs.yaml";
            SDL_free(directory);
        }
        LoadFromFile();
    }

    bool HasKey(const std::string& key) { return values.contains(key); }
    std::string GetString(const std::string& key, const std::string& fallback)
    {
        auto found = values.find(key);
        return found == values.end() ? fallback : found->second;
    }
    int GetInt(const std::string& key, int fallback)
    {
        const auto text = GetString(key);
        int value;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size() ? value : fallback;
    }
    float GetFloat(const std::string& key, float fallback)
    {
        const auto text = GetString(key);
        float value;
        return ParseFiniteFloat(text, value) ? value : fallback;
    }
    bool GetBool(const std::string& key, bool fallback)
    {
        const auto text = GetString(key);
        return text == "true" ? true : text == "false" ? false : fallback;
    }
    void SetString(const std::string& key, const std::string& value)
    {
        if (!HasKey(key) || values[key] != value)
        {
            values[key] = value;
            dirty = true;
        }
    }
    void SetInt(const std::string& key, int value) { SetString(key, std::to_string(value)); }
    void SetFloat(const std::string& key, float value)
    {
        if (!std::isfinite(value))
            throw std::invalid_argument("PlayerPrefs float must be finite");
        SetString(key, FormatFiniteFloat(value));
    }
    void SetBool(const std::string& key, bool value) { SetString(key, value ? "true" : "false"); }
    void DeleteKey(const std::string& key) { dirty = values.erase(key) != 0 || dirty; }
    void DeleteAll()
    {
        dirty = !values.empty() || dirty;
        values.clear();
    }
    void LoadFromFile()
    {
        if (path.empty())
            throw std::runtime_error("PlayerPrefs is not initialized");
        std::map<std::string, std::string> loaded;
        if (std::filesystem::exists(path))
        {
            const auto document = YAML::LoadFile(path.string());
            if (!document.IsMap())
                throw std::runtime_error("Invalid PlayerPrefs file: expected a map");
            for (auto pair : document)
                loaded.emplace(pair.first.as<std::string>(), pair.second.as<std::string>());
        }
        values = std::move(loaded);
        dirty = false;
    }
    void SaveToFile()
    {
        if (!dirty)
            return;
        if (path.empty())
            throw std::runtime_error("PlayerPrefs is not initialized");
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        YAML::Emitter output;
        output << YAML::BeginMap;
        for (const auto& [key, value] : values)
            output << YAML::Key << YAML::DoubleQuoted << key << YAML::Value << YAML::DoubleQuoted << value;
        output << YAML::EndMap;
        const auto temporary = path.string() + ".tmp";
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << output.c_str();
        file.close();
        if (!file)
            throw std::runtime_error("Could not save PlayerPrefs: " + temporary);
        if (!SDL_RenamePath(temporary.c_str(), path.string().c_str()))
            throw std::runtime_error(SDL_GetError());
        dirty = false;
    }
}
