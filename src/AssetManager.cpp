#include <Canis/AssetManager.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    int AssetManager::LoadTexture(const std::string &_path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_path);

        // check if texture already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create texture
        Asset* texture = new TextureAsset();
        texture->Load(_path);
        int id = m_nextId;

        // cache texture
        m_assets[id] = texture;

        // cache id
        m_assetPath[_path] = id;

        // increment id
        m_nextId++;

        return id;
    }

    int AssetManager::LoadSkybox(const std::string &_path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_path);

        // check if skybox already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create skybox
        Asset* skybox = new SkyboxAsset();
        skybox->Load(_path);
        int id = m_nextId;

        // cache skybox
        m_assets[id] = skybox;

        // cache id
        m_assetPath[_path] = id;

        // increment id
        m_nextId++;

        return id;
    }

    int AssetManager::LoadModel(const std::string &_path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_path);

        // check if model already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create model
        Asset* model = new ModelAsset();
        model->Load(_path);
        int id = m_nextId;

        // cache model
        m_assets[id] = model;

        // cache id
        m_assetPath[_path] = id;

        // increment id
        m_nextId++;

        return id;
    }

    int AssetManager::LoadSound(const std::string &_path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_path);

        // check if model already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create sound
        Asset* sound = new SoundAsset();
        sound->Load(_path);
        int id = m_nextId;

        // cache model
        m_assets[id] = sound;

        // cache id
        m_assetPath[_path] = id;

        // increment id
        m_nextId++;

        return id;
    }

    int AssetManager::LoadMusic(const std::string &_path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_path);

        // check if model already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create sound
        Asset* music = new MusicAsset();
        music->Load(_path);
        int id = m_nextId;

        // cache music
        m_assets[id] = music;

        // cache id
        m_assetPath[_path] = id;

        // increment id
        m_nextId++;

        return id;
    }
    
    int AssetManager::LoadText(const std::string &_path, unsigned int fontSize)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_path + std::to_string(fontSize));

        // check if text already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create text
        Asset* text = new TextAsset(fontSize);
        text->Load(_path);
        int id = m_nextId;

        // cache text
        m_assets[id] = text;

        // cache id
        m_assetPath[_path + std::to_string(fontSize)] = id;

        // increment id
        m_nextId++;

        return id;
    }

    int AssetManager::LoadShader(const std::string &_pathWithOutExtension)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_pathWithOutExtension);

        // check if shader already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create sound
        Asset* shader = new ShaderAsset();
        shader->Load(_pathWithOutExtension);
        int id = m_nextId;

        // cache shader
        m_assets[id] = shader;

        // cache id
        m_assetPath[_pathWithOutExtension] = id;

        // increment id
        m_nextId++;

        return id;
    }

    int AssetManager::LoadSpriteAnimation(const std::string &_path)
    {
        std::map<std::string, int>::iterator it;
        it = m_assetPath.find(_path);

        // check if animation already exist
        if (it != m_assetPath.end()) // found
        {
            return it->second;
        }

        // create animation
        Asset* anim = new SpriteAnimationAsset();
        anim->Load(_path);

        YAML::Node root = YAML::LoadFile(_path);

        if (YAML::Node animation = root["Animation"])
        {
            for(auto animationFrame : animation)
            {
                SpriteFrame frame = {};
                frame.timeOnFrame = animationFrame["timeOnFrame"].as<float>();
                frame.textureId = LoadTexture(animationFrame["textureAssetId"].as<std::string>());
                glm::vec2 offset = animationFrame["offset"].as<glm::vec2>();
                frame.offsetX = offset.x;
                frame.offsetY = offset.y;
                glm::vec2 index = animationFrame["index"].as<glm::vec2>();
                frame.row = index.x;
                frame.col = index.y;
                glm::vec2 size = animationFrame["size"].as<glm::vec2>();
                frame.width = size.x;
                frame.height = size.y;
                ((SpriteAnimationAsset*)anim)->frames.push_back(frame);
            }
        }

        int id = m_nextId;

        // cache animation
        m_assets[id] = anim;

        // cache id
        m_assetPath[_path] = id;

        // increment id
        m_nextId++;

        return id;
    }
    

} // end of Canis namespace