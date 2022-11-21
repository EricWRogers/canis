#pragma once
#include <string>
#include <map>

#include <Canis/Asset.hpp>

namespace Canis
{
    class AssetManager
    {
    public:
        // this forces refence instead of 
        AssetManager() {};

        int LoadTexture(std::string path);
        int LoadSkybox(std::string path);
        int LoadModel(std::string path);
        int LoadText(std::string path, unsigned int fontSize);
        int GetIdByPath(std::string path);

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