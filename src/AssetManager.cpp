#include <Canis/AssetManager.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/Debug.hpp>

namespace Canis
{
    namespace AssetManager
    {
        AssetLibrary &GetAssetLibrary()
        {
            static AssetLibrary assetLibrary = {};
            return assetLibrary;
        }

        bool Has(std::string _name)
        {
            auto &assetLibrary = GetAssetLibrary();
            return assetLibrary.assetPath.contains(_name);
        }

        int LoadTexture(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();

            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            // check if texture already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create texture
            Asset *texture = new TextureAsset();
            texture->Load(_path);
            int id = assetLibrary.nextId;

            // cache texture
            assetLibrary.assets[id] = texture;

            // cache id
            assetLibrary.assetPath[_path] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        TextureAsset *GetTexture(const int _textureID)
        {
            return (TextureAsset *)GetAssetLibrary().assets[_textureID];
        }

        TextureAsset *GetTexture(const std::string &_path)
        {
            return GetTexture(LoadTexture(_path));
        }

        int LoadSkybox(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            // check if skybox already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create skybox
            Asset *skybox = new SkyboxAsset();
            skybox->Load(_path);
            int id = assetLibrary.nextId;

            // cache skybox
            assetLibrary.assets[id] = skybox;

            // cache id
            assetLibrary.assetPath[_path] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadModel(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            // check if model already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create model
            Asset *model = new ModelAsset();
            model->Load(_path);
            int id = assetLibrary.nextId;

            // cache model
            assetLibrary.assets[id] = model;

            // cache id
            assetLibrary.assetPath[_path] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadModel(const std::string &_name, const std::vector<Canis::Vertex> &_vertices)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_name);

            // check if model already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create model
            Asset *model = new ModelAsset();
            ((ModelAsset *)model)->LoadWithVertex(_vertices);
            int id = assetLibrary.nextId;

            // cache model
            assetLibrary.assets[id] = model;

            // cache id
            assetLibrary.assetPath[_name] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadSound(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            // check if model already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create sound
            Asset *sound = new SoundAsset();
            sound->Load(_path);
            int id = assetLibrary.nextId;

            // cache model
            assetLibrary.assets[id] = sound;

            // cache id
            assetLibrary.assetPath[_path] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadMusic(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            // check if model already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create sound
            Asset *music = new MusicAsset();
            music->Load(_path);
            int id = assetLibrary.nextId;

            // cache music
            assetLibrary.assets[id] = music;

            // cache id
            assetLibrary.assetPath[_path] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadText(const std::string &_path, unsigned int fontSize)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path + std::to_string(fontSize));

            // check if text already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create text
            Asset *text = new TextAsset(fontSize);
            text->Load(_path);
            int id = assetLibrary.nextId;

            // cache text
            assetLibrary.assets[id] = text;

            // cache id
            assetLibrary.assetPath[_path + std::to_string(fontSize)] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadShader(const std::string &_pathWithOutExtension)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_pathWithOutExtension);

            // check if shader already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create sound
            Asset *shader = new ShaderAsset();
            shader->Load(_pathWithOutExtension);
            int id = assetLibrary.nextId;

            // cache shader
            assetLibrary.assets[id] = shader;

            // cache id
            assetLibrary.assetPath[_pathWithOutExtension] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadMaterial(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            // check if animation already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create animation
            MaterialAsset *material = new MaterialAsset();

            YAML::Node root = YAML::LoadFile(_path);

            if (YAML::Node shaderNode = root["shader"])
            {
                std::string shaderPath = shaderNode.as<std::string>();

                material->shaderId = LoadShader(shaderPath);

                ShaderAsset *shaderAsset = Get<ShaderAsset>(material->shaderId);

                if (shaderAsset->GetShader()->IsLinked() == false)
                {
                    shaderAsset->GetShader()->Link();
                }

                material->info |= MaterialInfo::HASSHADER;
            }

            if (YAML::Node albedoNode = root["albedo"])
            {
                std::string albedoPath = albedoNode.as<std::string>();

                material->albedoId = LoadTexture(albedoPath);

                material->info |= MaterialInfo::HASALBEDO;
            }

            if (YAML::Node specularNode = root["specular"])
            {
                std::string specularPath = specularNode.as<std::string>();

                material->specularId = LoadTexture(specularPath);

                material->info |= MaterialInfo::HASSPECULAR;
            }

            if (YAML::Node emissionNode = root["emission"])
            {
                std::string emissionPath = emissionNode.as<std::string>();

                material->emissionId = LoadTexture(emissionPath);

                material->info |= MaterialInfo::HASEMISSION;
            }

            if (YAML::Node noiseNode = root["noise"])
            {
                std::string noisePath = noiseNode.as<std::string>();

                material->noiseId = LoadTexture(noisePath);

                material->info |= MaterialInfo::HASNOISE;
            }

            if (YAML::Node screenNode = root["screen"])
            {
                if (screenNode.as<std::string>() == "true")
                    material->info |= MaterialInfo::HASSCREENTEXTURE;
            }

            int id = assetLibrary.nextId;

            // cache material
            assetLibrary.assets[id] = material;

            // cache id
            assetLibrary.assetPath[_path] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        int LoadSpriteAnimation(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            // check if animation already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create animation
            Asset *anim = new SpriteAnimationAsset();
            anim->Load(_path);

            YAML::Node root = YAML::LoadFile(_path);

            if (YAML::Node animation = root["Animation"])
            {
                for (auto animationFrame : animation)
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
                    ((SpriteAnimationAsset *)anim)->frames.push_back(frame);
                }
            }

            int id = assetLibrary.nextId;

            // cache animation
            assetLibrary.assets[id] = anim;

            // cache id
            assetLibrary.assetPath[_path] = id;

            // increment id
            assetLibrary.nextId++;

            return id;
        }
    } // end of AssetManager namespace

} // end of Canis namespace