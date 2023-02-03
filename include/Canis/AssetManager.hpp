#pragma once
#include <string>
#include <map>

#include <Canis/Asset.hpp>

namespace Canis
{
    class AssetManager
    {
    public:
        AssetManager() {};

        int LoadTexture(const std::string &_path);
        int LoadSkybox(const std::string &_path);
        int LoadModel(const std::string &_path);
        int LoadSound(const std::string &_path);
        int LoadMusic(const std::string &_path);
        int LoadText(const std::string &_path, unsigned int fontSize);
        int LoadShader(const std::string &_pathWithOutExtension);
        int LoadSpriteAnimation(const std::string &path);
        int GetIdByPath(const std::string &_path);

        template <typename T>
        T* Get(int id)
        {
            return (T*)m_assets[id];
        }

    private:
        int m_nextId{ 0 };
        std::map<int, Asset*> m_assets{};
        std::map<std::string, int> m_assetPath{};
    };
} // end of Canis namespace