#pragma once
#include <string>
#include <map>

#include <Canis/Asset.hpp>
#include <Canis/Debug.hpp>

namespace Canis
{
    namespace AssetManager
    {
        struct AssetLibrary {
            int nextId{ 0 };
            std::map<int, void*> assets{};
            std::map<std::string, int> assetPath{};
        };

        AssetLibrary& GetAssetLibrary();

        bool Has(std::string _name);

        template <typename T>
        T* Get(int id)
        {
            auto &assetLibrary = GetAssetLibrary();
            return (T*)assetLibrary.assets[id];
        }

        template <typename T>
        void Free(std::string _name)
        {
            auto &assetLibrary = GetAssetLibrary();
            if (!Has(_name))
                return;
            
            int assetId = assetLibrary.assetPath[_name];

            {
                std::map<std::string, int>::iterator it;
                it = assetLibrary.assetPath.find(_name);

                assetLibrary.assetPath.erase(it);
            }

            if (!assetLibrary.assets.contains(assetId))
                return;

            ((T*)assetLibrary.assets[assetId])->Free();
            delete ((T*)assetLibrary.assets[assetId]);

            {
                std::map<int, void*>::iterator it;
                it = assetLibrary.assets.find(assetId);

                assetLibrary.assets.erase(it);
            }
        }

        int LoadTexture(const std::string &_path);
        TextureAsset* GetTexture(const std::string &_path);
        TextureAsset* GetTexture(const int _textureID);

        int LoadSkybox(const std::string &_path);
        int LoadModel(const std::string &_path);
        int LoadModel(const std::string &_name, const std::vector<Canis::Vertex> &_vertices);
        int LoadSound(const std::string &_path);
        int LoadMusic(const std::string &_path);
        int LoadText(const std::string &_path, unsigned int fontSize);
        int LoadShader(const std::string &_pathWithOutExtension);
        int LoadMaterial(const std::string &_path);
        int LoadSpriteAnimation(const std::string &path);
        int LoadTiledMap(const std::string &_path);
        int LoadInstanceMeshAsset(const std::string &_name, unsigned int _modelID, const std::vector<glm::mat4> &_modelMatrices);
    } // end of AssetManager namespace
} // end of Canis namespace