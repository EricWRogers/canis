#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>
#include <Canis/IOManager.hpp>

#include <filesystem>
#include <algorithm>
#include <cctype>

#include <Canis/Yaml.hpp>

namespace Canis
{
    namespace AssetManager
    {
        namespace
        {
            std::string NormalizeAssetLibraryPath(const std::string& value)
            {
                if (value.empty())
                    return "";

                return std::filesystem::path(value).lexically_normal().generic_string();
            }

            int FindAssetIdForPath(const std::string &_path)
            {
                auto &assetLibrary = GetAssetLibrary();

                auto it = assetLibrary.assetPath.find(_path);
                if (it != assetLibrary.assetPath.end())
                    return it->second;

                const std::string normalizedPath = NormalizeAssetLibraryPath(_path);
                it = assetLibrary.assetPath.find(normalizedPath);
                if (it != assetLibrary.assetPath.end())
                    return it->second;

                return -1;
            }

            template <typename T>
            bool ReloadLoadedAssetInPlace(const std::string &_path)
            {
                auto &assetLibrary = GetAssetLibrary();
                const int id = FindAssetIdForPath(_path);
                if (id < 0)
                    return true;

                if (!assetLibrary.assets.contains(id))
                    return false;

                T *asset = static_cast<T *>(assetLibrary.assets[id]);
                if (asset == nullptr)
                    return false;

                return asset->Load(_path);
            }

            std::string Trim(const std::string &value)
            {
                const auto first = value.find_first_not_of(" \t\n\r");
                if (first == std::string::npos)
                    return "";
                const auto last = value.find_last_not_of(" \t\n\r");
                return value.substr(first, last - first + 1);
            }

            std::string ResolveAssetPath(const YAML::Node &node)
            {
                if (!node)
                    return "";

                // Supports either:
                // key: assets/path.ext
                // key: 1234567890
                // key: { uuid: 1234567890, path: assets/path.ext }
                if (node.IsMap())
                {
                    if (auto uuidNode = node["uuid"])
                    {
                        const UUID uuid = uuidNode.as<uint64_t>(0);
                        if ((uint64_t)uuid != 0)
                        {
                            std::string resolved = GetPath(uuid);
                            if (resolved != "Path was not found in AssetLibrary")
                                return resolved;
                        }
                    }

                    if (auto pathNode = node["path"])
                    {
                        return pathNode.as<std::string>("");
                    }

                    return "";
                }

                if (node.IsScalar())
                {
                    const std::string raw = Trim(node.as<std::string>(""));
                    if (raw.empty())
                        return "";

                    const bool isNumeric = std::all_of(raw.begin(), raw.end(), [](unsigned char c)
                        { return std::isdigit(c) != 0; });
                    if (isNumeric)
                    {
                        const UUID uuid = (UUID)std::stoull(raw);
                        std::string resolved = GetPath(uuid);
                        if (resolved != "Path was not found in AssetLibrary")
                            return resolved;
                    }

                    return raw;
                }

                return "";
            }

            std::string ResolveShaderPath(const YAML::Node &node)
            {
                std::string shaderPath = ResolveAssetPath(node);
                if (shaderPath.empty())
                    return "";

                // If UUID points at shader stage source meta (`.vs`/`.fs`), normalize to shader base path.
                if (shaderPath.size() > 3 && shaderPath.ends_with(".vs"))
                    shaderPath = shaderPath.substr(0, shaderPath.size() - 3);
                else if (shaderPath.size() > 3 && shaderPath.ends_with(".fs"))
                    shaderPath = shaderPath.substr(0, shaderPath.size() - 3);

                return shaderPath;
            }

            std::string ToLowerCopy(std::string value)
            {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
                return value;
            }

            bool IsReservedMaterialKey(const std::string &key)
            {
                return key == "shader" || key == "albedo" || key == "specular" || key == "roughness" || key == "metallic" ||
                       key == "emission" || key == "color" || key == "specularValue" || key == "roughnessValue" || key == "metallicValue" ||
                       key == "backFaceCulling" || key == "frontFaceCulling" || key == "uniforms";
            }

            bool ApplyTypedMaterialUniform(MaterialFields &_fields, const std::string &_uniformName, const std::string &_type, const YAML::Node &_valueNode)
            {
                const std::string type = ToLowerCopy(_type);
                try
                {
                    if (type == "int" || type == "integer")
                    {
                        _fields.SetInt(_uniformName, _valueNode.as<int>(0));
                        return true;
                    }

                    if (type == "float")
                    {
                        _fields.SetFloat(_uniformName, _valueNode.as<float>(0.0f));
                        return true;
                    }

                    if (type == "vector2" || type == "vec2")
                    {
                        _fields.SetVec2(_uniformName, _valueNode.as<Vector2>(Vector2(0.0f)));
                        return true;
                    }

                    if (type == "vector3" || type == "vec3")
                    {
                        _fields.SetVec3(_uniformName, _valueNode.as<Vector3>(Vector3(0.0f)));
                        return true;
                    }

                    if (type == "vector4" || type == "vec4")
                    {
                        _fields.SetVec4(_uniformName, _valueNode.as<Vector4>(Vector4(0.0f)));
                        return true;
                    }

                    if (type == "color")
                    {
                        _fields.SetColor(_uniformName, _valueNode.as<Color>(Color(1.0f)));
                        return true;
                    }

                    if (type == "texture" || type == "sampler2d")
                    {
                        std::string texturePath = ResolveAssetPath(_valueNode);
                        i32 textureId = -1;
                        if (!texturePath.empty())
                            textureId = LoadTexture(texturePath);
                        _fields.SetTexture(_uniformName, textureId);
                        return true;
                    }
                }
                catch (const YAML::Exception &)
                {
                }

                return false;
            }

            bool ApplyInferredMaterialUniform(MaterialFields &_fields, const std::string &_uniformName, const YAML::Node &_valueNode)
            {
                if (!_valueNode)
                    return false;

                if (_valueNode.IsMap())
                {
                    std::string texturePath = ResolveAssetPath(_valueNode);
                    i32 textureId = -1;
                    if (!texturePath.empty())
                        textureId = LoadTexture(texturePath);
                    _fields.SetTexture(_uniformName, textureId);
                    return true;
                }

                if (_valueNode.IsSequence())
                {
                    try
                    {
                        if (_valueNode.size() == 2u)
                        {
                            _fields.SetVec2(_uniformName, _valueNode.as<Vector2>(Vector2(0.0f)));
                            return true;
                        }

                        if (_valueNode.size() == 3u)
                        {
                            _fields.SetVec3(_uniformName, _valueNode.as<Vector3>(Vector3(0.0f)));
                            return true;
                        }

                        if (_valueNode.size() == 4u)
                        {
                            _fields.SetVec4(_uniformName, _valueNode.as<Vector4>(Vector4(0.0f)));
                            return true;
                        }
                    }
                    catch (const YAML::Exception &)
                    {
                    }
                }

                if (_valueNode.IsScalar())
                {
                    try
                    {
                        _fields.SetFloat(_uniformName, _valueNode.as<float>());
                        return true;
                    }
                    catch (const YAML::Exception &)
                    {
                    }
                }

                return false;
            }

            void LoadMaterialUniforms(const YAML::Node &_root, MaterialFields &_fields)
            {
                _fields.Clear();

                if (YAML::Node uniformsNode = _root["uniforms"]; uniformsNode && uniformsNode.IsMap())
                {
                    for (const auto &uniformEntry : uniformsNode)
                    {
                        const std::string uniformName = uniformEntry.first.as<std::string>("");
                        if (uniformName.empty())
                            continue;

                        const YAML::Node uniformNode = uniformEntry.second;
                        if (!uniformNode)
                            continue;

                        bool loaded = false;
                        if (uniformNode.IsMap())
                        {
                            const std::string type = uniformNode["type"].as<std::string>("");
                            YAML::Node valueNode = uniformNode["value"];
                            if (!valueNode && uniformNode["texture"])
                                valueNode = uniformNode["texture"];
                            if (!valueNode && uniformNode["data"])
                                valueNode = uniformNode["data"];
                            if (!valueNode)
                                valueNode = uniformNode;

                            if (!type.empty())
                                loaded = ApplyTypedMaterialUniform(_fields, uniformName, type, valueNode);

                            if (!loaded)
                                loaded = ApplyInferredMaterialUniform(_fields, uniformName, valueNode);
                        }
                        else
                        {
                            loaded = ApplyInferredMaterialUniform(_fields, uniformName, uniformNode);
                        }

                        if (!loaded && uniformNode.IsScalar())
                        {
                            try
                            {
                                _fields.SetFloat(uniformName, uniformNode.as<float>());
                            }
                            catch (const YAML::Exception &)
                            {
                            }
                        }
                    }
                }

                // Backward compatibility for legacy root-level scalar uniforms.
                for (const auto &entry : _root)
                {
                    const std::string key = entry.first.as<std::string>("");
                    if (IsReservedMaterialKey(key) || !entry.second.IsScalar())
                        continue;

                    if (YAML::Node uniformsNode = _root["uniforms"]; uniformsNode && uniformsNode.IsMap() && uniformsNode[key])
                        continue;

                    try
                    {
                        _fields.SetFloat(key, entry.second.as<float>());
                    }
                    catch (const YAML::Exception &)
                    {
                    }
                }
            }

            void PopulateMaterialAssetFromRoot(MaterialAsset &_material, const YAML::Node &_root)
            {
                _material = MaterialAsset();

                if (YAML::Node shaderNode = _root["shader"])
                {
                    std::string shaderPath = ResolveShaderPath(shaderNode);
                    if (!shaderPath.empty())
                    {
                        _material.shaderId = LoadShader(shaderPath);
                        if (_material.shaderId >= 0)
                        {
                            if (ShaderAsset *shaderAsset = Get<ShaderAsset>(_material.shaderId))
                            {
                                shaderAsset->Load(shaderPath);
                                if (!shaderAsset->GetShader()->IsLinked())
                                    shaderAsset->GetShader()->Link();
                            }
                            _material.info |= MATERIAL_HAS_SHADER;
                        }
                    }
                }

                if (YAML::Node albedoNode = _root["albedo"])
                {
                    std::string albedoPath = ResolveAssetPath(albedoNode);
                    if (!albedoPath.empty())
                    {
                        _material.albedoId = LoadTexture(albedoPath);
                        if (_material.albedoId >= 0)
                            _material.info |= MATERIAL_HAS_ALBEDO;
                    }
                }

                if (YAML::Node specularNode = _root["specular"])
                {
                    std::string specularPath = ResolveAssetPath(specularNode);
                    if (!specularPath.empty())
                    {
                        _material.specularId = LoadTexture(specularPath);
                        if (_material.specularId >= 0)
                            _material.info |= MATERIAL_HAS_SPECULAR;
                    }
                }

                if (YAML::Node roughnessNode = _root["roughness"])
                {
                    std::string roughnessPath = ResolveAssetPath(roughnessNode);
                    if (!roughnessPath.empty())
                    {
                        _material.roughnessId = LoadTexture(roughnessPath);
                        if (_material.roughnessId >= 0)
                            _material.info |= MATERIAL_HAS_ROUGHNESS;
                    }
                }

                if (YAML::Node metallicNode = _root["metallic"])
                {
                    std::string metallicPath = ResolveAssetPath(metallicNode);
                    if (!metallicPath.empty())
                    {
                        _material.metallicId = LoadTexture(metallicPath);
                        if (_material.metallicId >= 0)
                            _material.info |= MATERIAL_HAS_METALLIC;
                    }
                }

                if (YAML::Node emissionNode = _root["emission"])
                {
                    std::string emissionPath = ResolveAssetPath(emissionNode);
                    if (!emissionPath.empty())
                    {
                        _material.emissionId = LoadTexture(emissionPath);
                        if (_material.emissionId >= 0)
                            _material.info |= MATERIAL_HAS_EMISSION;
                    }
                }

                if (YAML::Node colorNode = _root["color"])
                {
                    _material.color = colorNode.as<Color>(Color(1.0f));
                    _material.info |= MATERIAL_HAS_COLOR;
                }

                _material.specularValue = _root["specularValue"].as<float>(0.5f);
                _material.roughnessValue = _root["roughnessValue"].as<float>(0.5f);
                _material.metallicValue = _root["metallicValue"].as<float>(0.0f);

                if (YAML::Node cullNode = _root["backFaceCulling"])
                {
                    if (cullNode.as<bool>(false))
                        _material.info |= MATERIAL_BACK_FACE_CULLING;
                }

                if (YAML::Node cullNode = _root["frontFaceCulling"])
                {
                    if (cullNode.as<bool>(false))
                        _material.info |= MATERIAL_FRONT_FACE_CULLING;
                }

                LoadMaterialUniforms(_root, _material.materialFields);
            }
        } // namespace

        AssetLibrary &GetAssetLibrary()
        {
            static AssetLibrary assetLibrary = {};
            return assetLibrary;
        }

        bool Has(std::string _name)
        {
            _name = NormalizeAssetLibraryPath(_name);
            auto &assetLibrary = GetAssetLibrary();
            return assetLibrary.assetPath.contains(_name);
        }

        bool MoveAsset(std::string _source, std::string _target)
        {
            namespace fs = std::filesystem;
            _source = NormalizeAssetLibraryPath(_source);
            _target = NormalizeAssetLibraryPath(_target);
            fs::path src = _source;
            fs::path dst = _target;

            if (_source.empty() || _target.empty())
            {
                Debug::Warning("MoveAsset called with an empty source or target path.");
                return false;
            }

            if (src == dst)
                return true;

            if (!fs::exists(src))
            {
                Debug::Warning("MoveAsset source does not exist: %s", _source.c_str());
                return false;
            }

            if (fs::exists(dst))
            {
                Debug::Warning("MoveAsset target already exists: %s", _target.c_str());
                return false;
            }

            auto &assetLibrary = GetAssetLibrary();
            const std::string sourceMetaPath = _source + ".meta";
            const std::string targetMetaPath = _target + ".meta";

            MetaFileAsset* meta = nullptr;
            int metaId = -1;
            if (auto metaIt = assetLibrary.assetPath.find(sourceMetaPath); metaIt != assetLibrary.assetPath.end())
            {
                metaId = metaIt->second;
                if (assetLibrary.assets.contains(metaId))
                    meta = static_cast<MetaFileAsset*>(assetLibrary.assets[metaId]);
            }

            if (meta == nullptr)
            {
                meta = GetMetaFile(_source);
                if (auto metaIt = assetLibrary.assetPath.find(sourceMetaPath); metaIt != assetLibrary.assetPath.end())
                    metaId = metaIt->second;
            }

            if (meta == nullptr)
            {
                Debug::Warning("MoveAsset failed to load meta file for: %s", _source.c_str());
                return false;
            }

            // move asset
            std::error_code ec;
            fs::rename(src, dst, ec);
            if (ec)
            {
                Debug::Warning("Failed to move asset '%s' to '%s': %s", _source.c_str(), _target.c_str(), ec.message().c_str());
                return false;
            }

            // move asset meta
            std::error_code ec2;
            if (fs::exists(sourceMetaPath))
            {
                fs::rename(sourceMetaPath, targetMetaPath, ec2);
                if (ec2)
                    Debug::Warning("Failed to move meta file '%s' to '%s': %s", sourceMetaPath.c_str(), targetMetaPath.c_str(), ec2.message().c_str());
            }

            if (auto assetIt = assetLibrary.assetPath.find(_source); assetIt != assetLibrary.assetPath.end())
            {
                const int id = assetIt->second;
                assetLibrary.assetPath.erase(assetIt);
                assetLibrary.assetPath[_target] = id;
            }

            if (metaId >= 0)
            {
                assetLibrary.assetPath.erase(sourceMetaPath);
                assetLibrary.assetPath[targetMetaPath] = metaId;
            }

            meta->name = dst.filename().string();
            meta->path = _target;
            assetLibrary.uuidAssetPath[meta->uuid] = _target;

            // save updated meta
            meta->Save();
            return true;
        }

        bool DeleteAsset(std::string _path)
        {
            namespace fs = std::filesystem;
            _path = NormalizeAssetLibraryPath(_path);

            if (_path.empty())
            {
                Debug::Warning("DeleteAsset called with an empty path.");
                return false;
            }

            fs::path assetPath = _path;
            if (!fs::exists(assetPath))
            {
                Debug::Warning("DeleteAsset path does not exist: %s", _path.c_str());
                return false;
            }

            auto &assetLibrary = GetAssetLibrary();
            const std::string metaPath = _path + ".meta";

            MetaFileAsset *meta = nullptr;
            int metaId = -1;
            if (auto metaIt = assetLibrary.assetPath.find(metaPath); metaIt != assetLibrary.assetPath.end())
            {
                metaId = metaIt->second;
                if (assetLibrary.assets.contains(metaId))
                    meta = static_cast<MetaFileAsset *>(assetLibrary.assets[metaId]);
            }

            if (meta == nullptr && FileExists(metaPath.c_str()))
            {
                meta = GetMetaFile(_path);
                if (auto metaIt = assetLibrary.assetPath.find(metaPath); metaIt != assetLibrary.assetPath.end())
                    metaId = metaIt->second;
            }

            const UUID metaUuid = meta != nullptr ? meta->uuid : UUID(0);

            std::error_code ec;
            fs::remove(assetPath, ec);
            if (ec)
            {
                Debug::Warning("Failed to delete asset '%s': %s", _path.c_str(), ec.message().c_str());
                return false;
            }

            std::error_code metaEc;
            if (fs::exists(metaPath))
            {
                fs::remove(metaPath, metaEc);
                if (metaEc)
                {
                    Debug::Warning("Failed to delete meta file '%s': %s", metaPath.c_str(), metaEc.message().c_str());
                    return false;
                }
            }

            assetLibrary.assetPath.erase(_path);
            assetLibrary.assetPath.erase(metaPath);

            if ((uint64_t)metaUuid != 0)
                assetLibrary.uuidAssetPath.erase(metaUuid);

            if (metaId >= 0)
            {
                if (assetLibrary.assets.contains(metaId))
                {
                    delete static_cast<MetaFileAsset *>(assetLibrary.assets[metaId]);
                    assetLibrary.assets.erase(metaId);
                }
            }

            return true;
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

        bool ReloadTexture(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            const int id = FindAssetIdForPath(_path);
            if (id < 0)
                return true;

            if (!assetLibrary.assets.contains(id))
                return false;

            TextureAsset *texture = static_cast<TextureAsset *>(assetLibrary.assets[id]);
            if (texture == nullptr)
                return false;

            return texture->Reload(_path);
        }

        TextureAsset *GetTexture(const int _textureID)
        {
            return (TextureAsset *)GetAssetLibrary().assets[_textureID];
        }

        TextureAsset *GetTexture(const std::string &_path)
        {
            return GetTexture(LoadTexture(_path));
        }

        TextureHandle GetTextureHandle(const std::string &_path)
        {
            TextureHandle handle;
            handle.id = LoadTexture(_path);
            handle.texture = GetTexture(handle.id)->GetGLTexture();
            return handle;
        }

        TextureHandle GetTextureHandle(const int _textureID)
        {
            TextureHandle handle;
            handle.id = _textureID;
            handle.texture = GetTexture(_textureID)->GetGLTexture();
            return handle;
        }

        int LoadText(const std::string &_path, unsigned int _fontSize)
        {
            auto &assetLibrary = GetAssetLibrary();
            const std::string key = _path + std::to_string(_fontSize);

            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(key);

            if (it != assetLibrary.assetPath.end())
            {
                return it->second;
            }

            Asset *text = new TextAsset(_fontSize);
            text->Load(_path);
            int id = assetLibrary.nextId;

            assetLibrary.assets[id] = text;
            assetLibrary.assetPath[key] = id;
            assetLibrary.nextId++;

            return id;
        }

        TextAsset *GetText(const std::string &_path, unsigned int _fontSize)
        {
            return GetText(LoadText(_path, _fontSize));
        }

        TextAsset *GetText(i32 _textID)
        {
            if (GetAssetLibrary().assets.contains(_textID))
                return (TextAsset *)GetAssetLibrary().assets[_textID];

            return nullptr;
        }

        int LoadAudioClip(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();

            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            if (it != assetLibrary.assetPath.end())
            {
                return it->second;
            }

            Asset *audioClip = new AudioClipAsset();
            if (!audioClip->Load(_path))
            {
                delete audioClip;
                return -1;
            }

            const int id = assetLibrary.nextId;
            assetLibrary.assets[id] = audioClip;
            assetLibrary.assetPath[_path] = id;
            assetLibrary.nextId++;

            return id;
        }

        AudioClipAsset *GetAudioClip(const std::string &_path)
        {
            const int id = LoadAudioClip(_path);
            return id >= 0 ? GetAudioClip(id) : nullptr;
        }

        AudioClipAsset *GetAudioClip(i32 _audioID)
        {
            if (GetAssetLibrary().assets.contains(_audioID))
                return (AudioClipAsset *)GetAssetLibrary().assets[_audioID];

            return nullptr;
        }

        int LoadSound(const std::string &_path)
        {
            return LoadAudioClip(_path);
        }

        SoundAsset *GetSound(const std::string &_path)
        {
            return static_cast<SoundAsset *>(GetAudioClip(_path));
        }

        SoundAsset *GetSound(i32 _soundID)
        {
            return static_cast<SoundAsset *>(GetAudioClip(_soundID));
        }

        int LoadMusic(const std::string &_path)
        {
            return LoadAudioClip(_path);
        }

        MusicAsset *GetMusic(const std::string &_path)
        {
            return static_cast<MusicAsset *>(GetAudioClip(_path));
        }

        MusicAsset *GetMusic(i32 _musicID)
        {
            return static_cast<MusicAsset *>(GetAudioClip(_musicID));
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

            // create shader
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

        void ReloadLoadedShaders()
        {
            auto &assetLibrary = GetAssetLibrary();
            std::vector<std::pair<int, std::string>> shaderAssets = {};

            for (const auto &[path, id] : assetLibrary.assetPath)
            {
                if (!assetLibrary.assets.contains(id))
                    continue;

                if (!FileExists((path + ".vs").c_str()) || !FileExists((path + ".fs").c_str()))
                    continue;

                shaderAssets.emplace_back(id, path);
            }

            for (const auto &[id, path] : shaderAssets)
            {
                ShaderAsset *shaderAsset = static_cast<ShaderAsset *>(assetLibrary.assets[id]);
                if (shaderAsset == nullptr)
                    continue;

                shaderAsset->Load(path);
                if (!shaderAsset->GetShader()->IsLinked())
                    shaderAsset->GetShader()->Link();
            }
        }

        int LoadMetaFile(const std::string &_path)
        {
            const std::string normalizedPath = NormalizeAssetLibraryPath(_path);
            auto &assetLibrary = GetAssetLibrary();

            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(normalizedPath + ".meta");

            // check if meta file already exist
            if (it != assetLibrary.assetPath.end()) // found
            {
                return it->second;
            }

            // create texture
            Asset *metaFile = new MetaFileAsset();
            metaFile->Load(normalizedPath);
            int id = assetLibrary.nextId;

            // cache texture
            assetLibrary.assets[id] = metaFile;

            // cache id
            assetLibrary.assetPath[normalizedPath + ".meta"] = id;

            // uuid (repair collisions from duplicated/copied .meta files)
            MetaFileAsset* loadedMeta = (MetaFileAsset*)metaFile;
            while (assetLibrary.uuidAssetPath.contains(loadedMeta->uuid) &&
                   assetLibrary.uuidAssetPath[loadedMeta->uuid] != normalizedPath)
            {
                Debug::Warning(
                    "Duplicate asset UUID detected (%llu) for '%s' and '%s'. Regenerating UUID for '%s'.",
                    static_cast<unsigned long long>(loadedMeta->uuid),
                    assetLibrary.uuidAssetPath[loadedMeta->uuid].c_str(),
                    normalizedPath.c_str(),
                    normalizedPath.c_str());
                loadedMeta->uuid = UUID();
                loadedMeta->Save();
            }
            loadedMeta->path = normalizedPath;
            assetLibrary.uuidAssetPath[loadedMeta->uuid] = normalizedPath;

            // increment id
            assetLibrary.nextId++;

            return id;
        }

        MetaFileAsset* GetMetaFile(const std::string &_path)
        {
            return GetMetaFile(LoadMetaFile(NormalizeAssetLibraryPath(_path)));
        }

        MetaFileAsset* GetMetaFile(const int _metaID)
        {
            if (GetAssetLibrary().assets.contains(_metaID))
                return (MetaFileAsset *)GetAssetLibrary().assets[_metaID];
            else
                return nullptr;
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
                    Vector2 offset = animationFrame["offset"].as<Vector2>();
                    frame.offsetX = offset.x;
                    frame.offsetY = offset.y;
                    Vector2 index = animationFrame["index"].as<Vector2>();
                    frame.row = index.x;
                    frame.col = index.y;
                    Vector2 size = animationFrame["size"].as<Vector2>();
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
    
        SpriteAnimationAsset* GetSpriteAnimation(const std::string &_path)
        {
            return GetSpriteAnimation(LoadSpriteAnimation(_path));
        }

        SpriteAnimationAsset* GetSpriteAnimation(i32 _animationID)
        {
            return (SpriteAnimationAsset *)GetAssetLibrary().assets[_animationID];
        }

        int LoadAnimationClip(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            if (std::map<std::string, int>::iterator it = assetLibrary.assetPath.find(_path); it != assetLibrary.assetPath.end())
                return it->second;

            Asset *clip = new AnimationClipAsset();
            if (!clip->Load(_path))
            {
                delete clip;
                return -1;
            }

            const int id = assetLibrary.nextId++;
            assetLibrary.assets[id] = clip;
            assetLibrary.assetPath[_path] = id;
            return id;
        }

        AnimationClipAsset* GetAnimationClip(const std::string &_path)
        {
            const int id = LoadAnimationClip(_path);
            return id >= 0 ? GetAnimationClip(id) : nullptr;
        }

        AnimationClipAsset* GetAnimationClip(i32 _animationID)
        {
            if (!GetAssetLibrary().assets.contains(_animationID))
                return nullptr;

            return static_cast<AnimationClipAsset *>(GetAssetLibrary().assets[_animationID]);
        }

        int LoadAnimatorController(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            if (std::map<std::string, int>::iterator it = assetLibrary.assetPath.find(_path); it != assetLibrary.assetPath.end())
                return it->second;

            Asset *controller = new AnimatorControllerAsset();
            if (!controller->Load(_path))
            {
                delete controller;
                return -1;
            }

            const int id = assetLibrary.nextId++;
            assetLibrary.assets[id] = controller;
            assetLibrary.assetPath[_path] = id;
            return id;
        }

        AnimatorControllerAsset* GetAnimatorController(const std::string &_path)
        {
            const int id = LoadAnimatorController(_path);
            return id >= 0 ? GetAnimatorController(id) : nullptr;
        }

        AnimatorControllerAsset* GetAnimatorController(i32 _animatorControllerID)
        {
            if (!GetAssetLibrary().assets.contains(_animatorControllerID))
                return nullptr;

            return static_cast<AnimatorControllerAsset *>(GetAssetLibrary().assets[_animatorControllerID]);
        }

        int LoadModel(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            std::map<std::string, int>::iterator it;
            it = assetLibrary.assetPath.find(_path);

            if (it != assetLibrary.assetPath.end())
            {
                return it->second;
            }

            Asset *model = new ModelAsset();
            if (!model->Load(_path))
            {
                delete model;
                return -1;
            }

            const int id = assetLibrary.nextId++;
            assetLibrary.assets[id] = model;
            assetLibrary.assetPath[_path] = id;

            return id;
        }

        bool ReloadModel(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            const int id = FindAssetIdForPath(_path);
            if (id < 0)
                return true;

            if (!assetLibrary.assets.contains(id))
                return false;

            ModelAsset *model = static_cast<ModelAsset *>(assetLibrary.assets[id]);
            if (model == nullptr)
                return false;

            return model->Load(_path);
        }

        int CreateModel()
        {
            auto &assetLibrary = GetAssetLibrary();

            const int id = assetLibrary.nextId++;
            assetLibrary.assets[id] = new ModelAsset();
            return id;
        }

        void FreeModel(i32 _modelID)
        {
            auto &assetLibrary = GetAssetLibrary();
            auto assetIt = assetLibrary.assets.find(_modelID);
            if (assetIt == assetLibrary.assets.end())
                return;

            for (auto pathIt = assetLibrary.assetPath.begin(); pathIt != assetLibrary.assetPath.end();)
            {
                if (pathIt->second == _modelID)
                    pathIt = assetLibrary.assetPath.erase(pathIt);
                else
                    ++pathIt;
            }

            if (Asset *asset = static_cast<Asset *>(assetIt->second))
            {
                asset->Free();
                delete asset;
            }

            assetLibrary.assets.erase(assetIt);
        }

        ModelAsset* GetModel(const std::string &_path)
        {
            int id = LoadModel(_path);
            if (id < 0)
                return nullptr;

            return GetModel(id);
        }

        ModelAsset* GetModel(i32 _modelID)
        {
            if (GetAssetLibrary().assets.contains(_modelID))
                return (ModelAsset *)GetAssetLibrary().assets[_modelID];

            return nullptr;
        }

        int LoadMaterial(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            auto it = assetLibrary.assetPath.find(_path);
            if (it != assetLibrary.assetPath.end())
                return it->second;

            if (!FileExists(_path.c_str()))
            {
                Debug::Warning("Material file not found: %s", _path.c_str());
                return -1;
            }

            MaterialAsset *material = new MaterialAsset();
            YAML::Node root = YAML::LoadFile(_path);
            PopulateMaterialAssetFromRoot(*material, root);

            const int id = assetLibrary.nextId;
            assetLibrary.assets[id] = material;
            assetLibrary.assetPath[_path] = id;
            assetLibrary.nextId++;

            return id;
        }

        MaterialAsset* GetMaterial(const std::string &_path)
        {
            const int id = LoadMaterial(_path);
            if (id < 0)
                return nullptr;

            return GetMaterial(id);
        }

        MaterialAsset* GetMaterial(i32 _materialID)
        {
            if (GetAssetLibrary().assets.contains(_materialID))
                return (MaterialAsset *)GetAssetLibrary().assets[_materialID];

            return nullptr;
        }

        bool ReloadMaterial(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            auto it = assetLibrary.assetPath.find(_path);
            if (it == assetLibrary.assetPath.end())
                return LoadMaterial(_path) >= 0;

            if (!assetLibrary.assets.contains(it->second))
                return false;

            if (!FileExists(_path.c_str()))
            {
                Debug::Warning("Material file not found: %s", _path.c_str());
                return false;
            }

            MaterialAsset *material = static_cast<MaterialAsset *>(assetLibrary.assets[it->second]);
            if (material == nullptr)
                return false;

            YAML::Node root = YAML::LoadFile(_path);
            PopulateMaterialAssetFromRoot(*material, root);
            return true;
        }

        bool ReloadAsset(const std::string &_path)
        {
            const std::string path = NormalizeAssetLibraryPath(_path);
            if (path.empty())
                return false;

            MetaFileAsset *meta = GetMetaFile(path);
            if (meta == nullptr)
                return false;

            switch (meta->type)
            {
                case MetaFileAsset::FileType::TEXTURE:
                    return ReloadTexture(path);
                case MetaFileAsset::FileType::MODEL:
                    return ReloadModel(path);
                case MetaFileAsset::FileType::MATERIAL:
                    return FindAssetIdForPath(path) < 0 ? true : ReloadMaterial(path);
                case MetaFileAsset::FileType::AUDIO:
                    return ReloadLoadedAssetInPlace<AudioClipAsset>(path);
                case MetaFileAsset::FileType::ANIMATIONCLIP:
                    return ReloadLoadedAssetInPlace<AnimationClipAsset>(path);
                case MetaFileAsset::FileType::ANIMATORCONTROLLER:
                    return ReloadLoadedAssetInPlace<AnimatorControllerAsset>(path);
                case MetaFileAsset::FileType::SKYBOX:
                    return ReloadLoadedAssetInPlace<SkyboxAsset>(path);
                case MetaFileAsset::FileType::POSTPROCESS:
                    return ReloadLoadedAssetInPlace<PostProcessAsset>(path);
                case MetaFileAsset::FileType::VERTEX:
                case MetaFileAsset::FileType::FRAGMENT:
                {
                    const std::string shaderBasePath = std::filesystem::path(path).replace_extension("").generic_string();
                    if (FindAssetIdForPath(shaderBasePath) >= 0)
                        ReloadLoadedShaders();
                    return true;
                }
                default:
                    return true;
            }
        }

        int LoadSkybox(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            auto it = assetLibrary.assetPath.find(_path);
            if (it != assetLibrary.assetPath.end())
                return it->second;

            SkyboxAsset *skybox = new SkyboxAsset();
            if (!skybox->Load(_path))
            {
                delete skybox;
                return -1;
            }

            const int id = assetLibrary.nextId;
            assetLibrary.assets[id] = skybox;
            assetLibrary.assetPath[_path] = id;
            assetLibrary.nextId++;

            return id;
        }

        SkyboxAsset* GetSkybox(const std::string &_path)
        {
            const int id = LoadSkybox(_path);
            if (id < 0)
                return nullptr;

            return GetSkybox(id);
        }

        SkyboxAsset* GetSkybox(i32 _skyboxID)
        {
            if (GetAssetLibrary().assets.contains(_skyboxID))
                return (SkyboxAsset *)GetAssetLibrary().assets[_skyboxID];

            return nullptr;
        }

        int LoadPostProcess(const std::string &_path)
        {
            auto &assetLibrary = GetAssetLibrary();
            auto it = assetLibrary.assetPath.find(_path);
            if (it != assetLibrary.assetPath.end())
                return it->second;

            if (!FileExists(_path.c_str()))
            {
                Debug::Warning("PostProcess file not found: %s", _path.c_str());
                return -1;
            }

            PostProcessAsset *postProcess = new PostProcessAsset();
            if (!postProcess->Load(_path))
            {
                delete postProcess;
                return -1;
            }

            const int id = assetLibrary.nextId;
            assetLibrary.assets[id] = postProcess;
            assetLibrary.assetPath[_path] = id;
            assetLibrary.nextId++;

            return id;
        }

        PostProcessAsset* GetPostProcess(const std::string &_path)
        {
            const int id = LoadPostProcess(_path);
            if (id < 0)
                return nullptr;

            return GetPostProcess(id);
        }

        PostProcessAsset* GetPostProcess(i32 _postProcessID)
        {
            if (GetAssetLibrary().assets.contains(_postProcessID))
                return (PostProcessAsset *)GetAssetLibrary().assets[_postProcessID];

            return nullptr;
        }
    } // end of AssetManager namespace

} // end of Canis namespace
