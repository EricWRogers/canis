#include <Canis/AssetManager.hpp>

namespace Canis
{
    int AssetManager::LoadTexture(std::string path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(path);

        // check if texture already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create texture
        Asset* texture = new Texture();
        texture->Load(path);
        int id = m_nextId;

        // cache texture
        m_assets[id] = texture;

        // cache id
        m_assetPath[path] = id;

        // increment id
        m_nextId++;

        return id;
    }

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

    int AssetManager::LoadModel(std::string path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(path);

        // check if model already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create model
        Asset* model = new Model();
        model->Load(path);
        int id = m_nextId;

        // cache model
        m_assets[id] = model;

        // cache id
        m_assetPath[path] = id;

        // increment id
        m_nextId++;

        return id;
    }
} // end of Canis namespace