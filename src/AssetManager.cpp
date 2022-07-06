#include <Canis/AssetManager.hpp>

namespace Canis
{
    int AssetManager::LoadSkybox(std::string path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(path);

        // check if skybox already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create skybox
        Asset* skybox = new Skybox();
        skybox->Load(path);
        int id = m_nextId;

        // cache skybox
        m_assets[id] = skybox;

        // cache id
        m_assetPath[path] = id;

        // increment id
        m_nextId++;

        return id;
    }
} // end of Canis namespace