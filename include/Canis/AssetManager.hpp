#pragma once
#include <string>
#include <map>

#include <Canis/Asset.hpp>
#include <Canis/Debug.hpp>

namespace Canis
{
    class AssetManager
    {
    public:
        AssetManager() {};

        int LoadTexture(const std::string &_path);
        int LoadSkybox(const std::string &_path);
        int LoadModel(const std::string &_path);
        int LoadModel(const std::string &_name, const std::vector<Canis::Vertex> &_vertices);
        int LoadSound(const std::string &_path);
        int LoadMusic(const std::string &_path);
        int LoadText(const std::string &_path, unsigned int fontSize);
        int LoadShader(const std::string &_pathWithOutExtension);
        int LoadSpriteAnimation(const std::string &path);
        int LoadMaterial(const std::string &_path);
        int GetIdByPath(const std::string &_path);

        template <typename T>
        T* Get(int id)
        {
            return (T*)m_assets[id];
        }

        bool Has(std::string _name)
        {
            return m_assetPath.contains(_name);
        }

        void Free(std::string _name)
        {
            if (!Has(_name))
                return;
            
            int assetId = m_assetPath[_name];

            {
                std::map<std::string, int>::iterator it;
                it = m_assetPath.find(_name);

                m_assetPath.erase(it);
            }

            if (!m_assets.contains(assetId))
                return;

            ((Asset*)m_assets[assetId])->Free();
            delete ((Asset*)m_assets[assetId]);

            {
                std::map<int, void*>::iterator it;
                it = m_assets.find(assetId);

                m_assets.erase(it);
            }
        }

    private:
        int m_nextId{ 0 };
        std::map<int, void*> m_assets{};
        std::map<std::string, int> m_assetPath{};
    };
} // end of Canis namespace