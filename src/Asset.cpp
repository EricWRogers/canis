#include <Canis/Asset.hpp>
#include <Canis/Audio.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/AssetManager.hpp>
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <Canis/External/tinygltf/tiny_gltf.h>
#include <memory>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <cmath>
#include <cstddef>
#include <functional>
#include <sstream>
#include <string.h>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <unordered_map>

#include <glm/gtc/quaternion.hpp>
#include <stb_image.h>
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#if defined(CANIS_HAS_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace Canis
{
    namespace
    {
        bool PrimitiveMatchesNodeFilter(const ModelAsset::Primitive3D &_primitive, i32 _nodeIndex)
        {
            return _nodeIndex < 0 || _primitive.nodeIndex == _nodeIndex;
        }

        std::string ResolveAssetRefPath(const YAML::Node &_node)
        {
            if (!_node)
                return "";

            if (_node.IsMap())
            {
                if (YAML::Node uuidNode = _node["uuid"])
                {
                    UUID uuid = uuidNode.as<uint64_t>(0);
                    if ((uint64_t)uuid != 0)
                    {
                        std::string path = AssetManager::GetPath(uuid);
                        if (path != "Path was not found in AssetLibrary")
                            return path;
                    }
                }

                if (YAML::Node pathNode = _node["path"])
                    return pathNode.as<std::string>("");

                return "";
            }

            if (_node.IsScalar())
            {
                const std::string value = _node.as<std::string>("");
                if (value.empty())
                    return "";

                const bool isNumeric = std::all_of(value.begin(), value.end(), [](unsigned char c)
                    { return std::isdigit(c) != 0; });
                if (isNumeric)
                {
                    UUID uuid = (UUID)std::stoull(value);
                    std::string path = AssetManager::GetPath(uuid);
                    if (path != "Path was not found in AssetLibrary")
                        return path;
                }

                return value;
            }

            return "";
        }

        std::string ToLower(std::string _value)
        {
            std::transform(_value.begin(), _value.end(), _value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            return _value;
        }

        bool ConvertAudioBufferToMixSamples(const SDL_AudioSpec &_sourceSpec, const Uint8 *_sourceData, int _sourceLength, std::vector<float> &_outSamples)
        {
            Uint8 *convertedData = nullptr;
            int convertedLength = 0;
            const SDL_AudioSpec &mixSpec = Audio::GetMixSpec();

            if (!SDL_ConvertAudioSamples(&_sourceSpec, _sourceData, _sourceLength, &mixSpec, &convertedData, &convertedLength))
            {
                Debug::Warning("Failed to convert audio samples: %s", SDL_GetError());
                return false;
            }

            if (convertedData == nullptr || convertedLength <= 0)
            {
                if (convertedData != nullptr)
                    SDL_free(convertedData);

                Debug::Warning("Audio conversion produced no data.");
                return false;
            }

            _outSamples.resize(static_cast<size_t>(convertedLength) / sizeof(float));
            std::memcpy(_outSamples.data(), convertedData, static_cast<size_t>(convertedLength));
            SDL_free(convertedData);
            return true;
        }

        bool LoadWaveAudioSamples(const std::string &_path, std::vector<float> &_outSamples)
        {
            SDL_AudioSpec sourceSpec = {};
            Uint8 *sourceBuffer = nullptr;
            Uint32 sourceLength = 0;

            if (!SDL_LoadWAV(_path.c_str(), &sourceSpec, &sourceBuffer, &sourceLength))
            {
                Debug::Warning("Failed to load WAV audio clip '%s': %s", _path.c_str(), SDL_GetError());
                return false;
            }

            const bool converted = ConvertAudioBufferToMixSamples(
                sourceSpec,
                sourceBuffer,
                static_cast<int>(sourceLength),
                _outSamples);

            SDL_free(sourceBuffer);
            return converted;
        }

        bool LoadOggAudioSamples(const std::string &_path, std::vector<float> &_outSamples)
        {
            int channels = 0;
            int sampleRate = 0;
            short *decodedSamples = nullptr;

            const int sampleFrames = stb_vorbis_decode_filename(_path.c_str(), &channels, &sampleRate, &decodedSamples);
            if (sampleFrames <= 0 || decodedSamples == nullptr)
            {
                if (decodedSamples != nullptr)
                    std::free(decodedSamples);

                Debug::Warning("Failed to load OGG audio clip '%s'.", _path.c_str());
                return false;
            }

            SDL_AudioSpec sourceSpec = { SDL_AUDIO_S16, channels, sampleRate };
            const int sourceLength = sampleFrames * channels * static_cast<int>(sizeof(short));
            const bool converted = ConvertAudioBufferToMixSamples(
                sourceSpec,
                reinterpret_cast<const Uint8 *>(decodedSamples),
                sourceLength,
                _outSamples);

            std::free(decodedSamples);
            return converted;
        }

        struct OBJVertexKey
        {
            int position = -1;
            int texcoord = -1;
            int normal = -1;

            bool operator==(const OBJVertexKey &_other) const
            {
                return position == _other.position &&
                    texcoord == _other.texcoord &&
                    normal == _other.normal;
            }
        };

        struct OBJVertexKeyHash
        {
            std::size_t operator()(const OBJVertexKey &_key) const
            {
                std::size_t hash = std::hash<int>{}(_key.position);
                hash ^= std::hash<int>{}(_key.texcoord) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
                hash ^= std::hash<int>{}(_key.normal) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
                return hash;
            }
        };

        struct OBJPrimitiveAccumulator
        {
            ModelAsset::PrimitiveBuild3D primitive = {};
            std::unordered_map<OBJVertexKey, unsigned int, OBJVertexKeyHash> vertexMap = {};
            std::vector<bool> hasExplicitNormal = {};
            std::vector<Vector3> generatedNormalSums = {};
        };

        int ResolveOBJIndex(const int _rawIndex, const std::size_t _count)
        {
            if (_rawIndex > 0)
            {
                const int resolved = _rawIndex - 1;
                return (resolved >= 0 && static_cast<std::size_t>(resolved) < _count) ? resolved : -1;
            }

            if (_rawIndex < 0)
            {
                const int resolved = static_cast<int>(_count) + _rawIndex;
                return (resolved >= 0 && static_cast<std::size_t>(resolved) < _count) ? resolved : -1;
            }

            return -1;
        }

        bool ParseOBJInteger(const std::string &_text, int &_value)
        {
            if (_text.empty())
                return false;

            try
            {
                std::size_t parsed = 0;
                const long long raw = std::stoll(_text, &parsed);
                if (parsed != _text.size())
                    return false;

                _value = static_cast<int>(raw);
                return true;
            }
            catch (const std::exception &)
            {
                return false;
            }
        }

        bool ParseOBJVertexToken(
            const std::string &_token,
            const std::size_t _positionCount,
            const std::size_t _texcoordCount,
            const std::size_t _normalCount,
            OBJVertexKey &_outKey)
        {
            _outKey = {};

            const std::size_t firstSlash = _token.find('/');
            const std::size_t secondSlash = (firstSlash == std::string::npos)
                ? std::string::npos
                : _token.find('/', firstSlash + 1u);

            const std::string positionToken = (firstSlash == std::string::npos)
                ? _token
                : _token.substr(0u, firstSlash);
            const std::string texcoordToken = (firstSlash == std::string::npos || secondSlash == std::string::npos)
                ? std::string()
                : _token.substr(firstSlash + 1u, secondSlash - firstSlash - 1u);
            const std::string normalToken = (secondSlash == std::string::npos)
                ? ((firstSlash == std::string::npos) ? std::string() : _token.substr(firstSlash + 1u))
                : _token.substr(secondSlash + 1u);

            int rawIndex = 0;
            if (!ParseOBJInteger(positionToken, rawIndex))
                return false;

            _outKey.position = ResolveOBJIndex(rawIndex, _positionCount);
            if (_outKey.position < 0)
                return false;

            if (firstSlash != std::string::npos && secondSlash == std::string::npos)
            {
                if (!normalToken.empty())
                {
                    if (!ParseOBJInteger(normalToken, rawIndex))
                        return false;

                    _outKey.texcoord = ResolveOBJIndex(rawIndex, _texcoordCount);
                    if (_outKey.texcoord < 0)
                        return false;
                }

                return true;
            }

            if (!texcoordToken.empty())
            {
                if (!ParseOBJInteger(texcoordToken, rawIndex))
                    return false;

                _outKey.texcoord = ResolveOBJIndex(rawIndex, _texcoordCount);
                if (_outKey.texcoord < 0)
                    return false;
            }

            if (!normalToken.empty())
            {
                if (!ParseOBJInteger(normalToken, rawIndex))
                    return false;

                _outKey.normal = ResolveOBJIndex(rawIndex, _normalCount);
                if (_outKey.normal < 0)
                    return false;
            }

            return true;
        }

        void FinalizeOBJPrimitive(OBJPrimitiveAccumulator &_accumulator)
        {
            if (_accumulator.primitive.vertices.empty() || _accumulator.primitive.indices.empty())
                return;

            bool needsGeneratedNormals = false;
            for (bool hasExplicitNormal : _accumulator.hasExplicitNormal)
            {
                if (!hasExplicitNormal)
                {
                    needsGeneratedNormals = true;
                    break;
                }
            }

            if (!needsGeneratedNormals)
                return;

            for (std::size_t index = 0; index + 2u < _accumulator.primitive.indices.size(); index += 3u)
            {
                const unsigned int index0 = _accumulator.primitive.indices[index + 0u];
                const unsigned int index1 = _accumulator.primitive.indices[index + 1u];
                const unsigned int index2 = _accumulator.primitive.indices[index + 2u];

                if (index0 >= _accumulator.primitive.vertices.size() ||
                    index1 >= _accumulator.primitive.vertices.size() ||
                    index2 >= _accumulator.primitive.vertices.size())
                    continue;

                const Vector3 &position0 = _accumulator.primitive.vertices[index0].position;
                const Vector3 &position1 = _accumulator.primitive.vertices[index1].position;
                const Vector3 &position2 = _accumulator.primitive.vertices[index2].position;

                Vector3 faceNormal = glm::cross(position1 - position0, position2 - position0);
                const float faceNormalLength = glm::length(faceNormal);
                if (faceNormalLength <= 1e-6f)
                    continue;

                faceNormal /= faceNormalLength;

                if (!_accumulator.hasExplicitNormal[index0])
                    _accumulator.generatedNormalSums[index0] += faceNormal;
                if (!_accumulator.hasExplicitNormal[index1])
                    _accumulator.generatedNormalSums[index1] += faceNormal;
                if (!_accumulator.hasExplicitNormal[index2])
                    _accumulator.generatedNormalSums[index2] += faceNormal;
            }

            for (std::size_t vertexIndex = 0; vertexIndex < _accumulator.primitive.vertices.size(); ++vertexIndex)
            {
                if (_accumulator.hasExplicitNormal[vertexIndex])
                    continue;

                Vector3 normal = _accumulator.generatedNormalSums[vertexIndex];
                const float normalLength = glm::length(normal);
                if (normalLength > 1e-6f)
                    normal /= normalLength;
                else
                    normal = Vector3(0.0f, 1.0f, 0.0f);

                _accumulator.primitive.vertices[vertexIndex].normal = normal;
            }
        }

        bool LoadOBJPrimitives(
            const std::string &_path,
            std::vector<ModelAsset::PrimitiveBuild3D> &_outPrimitives,
            std::vector<std::string> &_outMaterialSlotNames)
        {
            std::ifstream file(_path);
            if (!file.is_open())
            {
                Debug::Warning("Failed to open OBJ mesh '%s'.", _path.c_str());
                return false;
            }

            std::vector<Vector3> positions = {};
            std::vector<Vector2> texcoords = {};
            std::vector<Vector3> normals = {};
            std::vector<OBJPrimitiveAccumulator> accumulators = { OBJPrimitiveAccumulator{} };
            std::unordered_map<std::string, int> materialToAccumulator = {};
            int currentAccumulatorIndex = 0;

            auto getAccumulatorIndex = [&](const std::string &_materialName) -> int
            {
                if (_materialName.empty())
                    return 0;

                if (const auto existing = materialToAccumulator.find(_materialName); existing != materialToAccumulator.end())
                    return existing->second;

                const int slotIndex = static_cast<int>(_outMaterialSlotNames.size());
                _outMaterialSlotNames.push_back(_materialName);

                OBJPrimitiveAccumulator accumulator = {};
                accumulator.primitive.materialSlot = slotIndex;
                accumulators.push_back(std::move(accumulator));

                const int accumulatorIndex = static_cast<int>(accumulators.size()) - 1;
                materialToAccumulator[_materialName] = accumulatorIndex;
                return accumulatorIndex;
            };

            auto getOrCreateVertexIndex = [&](OBJPrimitiveAccumulator &_accumulator, const OBJVertexKey &_key) -> unsigned int
            {
                if (const auto existing = _accumulator.vertexMap.find(_key); existing != _accumulator.vertexMap.end())
                    return existing->second;

                ModelAsset::RenderVertex3D vertex = {};
                vertex.position = positions[_key.position];
                if (_key.texcoord >= 0)
                    vertex.uv = texcoords[_key.texcoord];
                if (_key.normal >= 0)
                    vertex.normal = normals[_key.normal];

                const unsigned int index = static_cast<unsigned int>(_accumulator.primitive.vertices.size());
                _accumulator.primitive.vertices.push_back(vertex);
                _accumulator.vertexMap.emplace(_key, index);
                _accumulator.hasExplicitNormal.push_back(_key.normal >= 0);
                _accumulator.generatedNormalSums.push_back(Vector3(0.0f));
                return index;
            };

            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                std::istringstream lineStream(line);
                std::string prefix;
                lineStream >> prefix;
                if (prefix.empty() || prefix[0] == '#')
                    continue;

                if (prefix == "v")
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    if (lineStream >> x >> y >> z)
                        positions.emplace_back(x, y, z);
                }
                else if (prefix == "vt")
                {
                    float u = 0.0f;
                    float v = 0.0f;
                    if (lineStream >> u >> v)
                        texcoords.emplace_back(u, v);
                }
                else if (prefix == "vn")
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    if (lineStream >> x >> y >> z)
                        normals.emplace_back(x, y, z);
                }
                else if (prefix == "usemtl")
                {
                    std::string materialName;
                    lineStream >> materialName;
                    currentAccumulatorIndex = getAccumulatorIndex(materialName);
                }
                else if (prefix == "f")
                {
                    std::vector<OBJVertexKey> faceVertices = {};
                    std::string token;
                    while (lineStream >> token)
                    {
                        OBJVertexKey key;
                        if (!ParseOBJVertexToken(token, positions.size(), texcoords.size(), normals.size(), key))
                        {
                            Debug::Warning("Failed to parse OBJ face token '%s' in '%s'.", token.c_str(), _path.c_str());
                            faceVertices.clear();
                            break;
                        }

                        faceVertices.push_back(key);
                    }

                    if (faceVertices.size() < 3u)
                        continue;

                    OBJPrimitiveAccumulator &accumulator = accumulators[currentAccumulatorIndex];
                    const unsigned int rootIndex = getOrCreateVertexIndex(accumulator, faceVertices[0]);
                    for (std::size_t vertexIndex = 1u; vertexIndex + 1u < faceVertices.size(); ++vertexIndex)
                    {
                        accumulator.primitive.indices.push_back(rootIndex);
                        accumulator.primitive.indices.push_back(getOrCreateVertexIndex(accumulator, faceVertices[vertexIndex]));
                        accumulator.primitive.indices.push_back(getOrCreateVertexIndex(accumulator, faceVertices[vertexIndex + 1u]));
                    }
                }
            }

            for (OBJPrimitiveAccumulator &accumulator : accumulators)
            {
                if (accumulator.primitive.vertices.empty() || accumulator.primitive.indices.empty())
                    continue;

                FinalizeOBJPrimitive(accumulator);
                _outPrimitives.push_back(std::move(accumulator.primitive));
            }

            if (_outPrimitives.empty())
            {
                Debug::Warning("OBJ mesh '%s' did not contain any triangle geometry.", _path.c_str());
                return false;
            }

            return true;
        }
    } // namespace

    Asset::Asset() {}

    Asset::~Asset() {}

    bool TextureAsset::Load(std::string _path)
    {
        m_path = _path;
        m_texture = LoadImageToGLTexture(_path, GL_RGBA, GL_RGBA);
        return true;
    }

    bool TextureAsset::Reload(std::string _path)
    {
        GLTexture replacement = m_texture;
        if (replacement.id == 0)
            glGenTextures(1, &replacement.id);

        int width = 0;
        int height = 0;
        int channels = 0;
        bool loaded = false;

        stbi_set_flip_vertically_on_load(false);
        SDL_IOStream *io = SDL_IOFromFile(_path.c_str(), "rb");
        if (io != nullptr)
        {
            size_t imageDataLength = 0;
            void *imageData = SDL_LoadFile_IO(io, &imageDataLength, true);
            if (imageData != nullptr && imageDataLength > 0)
            {
                stbi_uc *data = stbi_load_from_memory(static_cast<stbi_uc *>(imageData), imageDataLength, &width, &height, &channels, 4);
                if (data != nullptr)
                {
                    glBindTexture(GL_TEXTURE_2D, replacement.id);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glGenerateMipmap(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    replacement.width = width;
                    replacement.height = height;
                    loaded = true;
                }
                else
                {
                    const char *failureReason = stbi_failure_reason();
                    Debug::Warning(
                        "Failed to reload texture %s%s%s",
                        _path.c_str(),
                        failureReason != nullptr ? ": " : "",
                        failureReason != nullptr ? failureReason : "");
                }

                stbi_image_free(data);
                SDL_free(imageData);
            }
            else
            {
                Debug::Warning("Failed to read texture for reload %s: %s", _path.c_str(), SDL_GetError());
            }
        }
        else
        {
            Debug::Warning("Failed to open texture for reload: %s", _path.c_str());
        }

        stbi_set_flip_vertically_on_load(true);

        if (!loaded)
        {
            if (m_texture.id == 0 && replacement.id != 0)
                glDeleteTextures(1, &replacement.id);
            return false;
        }

        m_path = _path;
        m_texture = replacement;
        return true;
    }

    bool TextureAsset::Free()
    {
        if (m_texture.id != 0)
            glDeleteTextures(1, &m_texture.id);
        m_texture = {};
        return true;
    }

    bool SkyboxAsset::Load(std::string _path)
    {
        Free();

        if (!FileExists(_path.c_str()))
        {
            Debug::Warning("Skybox file not found: %s", _path.c_str());
            return false;
        }

        YAML::Node root = YAML::LoadFile(_path);

        static const char *faceNames[] = { "right", "left", "top", "bottom", "front", "back" };
        std::string facePaths[6];
        for (int i = 0; i < 6; i++)
        {
            facePaths[i] = ResolveAssetRefPath(root[faceNames[i]]);
            if (facePaths[i].empty())
            {
                Debug::Warning("Skybox '%s' is missing '%s' texture.", _path.c_str(), faceNames[i]);
                return false;
            }
        }

        glGenTextures(1, &m_cubemapTexture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);

        stbi_set_flip_vertically_on_load(false);
        bool loadedAllFaces = true;

        for (int i = 0; i < 6; i++)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc *data = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 4);
            if (!data)
            {
                Debug::Warning("Failed to load skybox face texture: %s", facePaths[i].c_str());
                loadedAllFaces = false;
                break;
            }

            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                GL_RGBA,
                width,
                height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                data);
            stbi_image_free(data);
        }

        stbi_set_flip_vertically_on_load(true);

        if (!loadedAllFaces)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            Free();
            return false;
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        m_loaded = true;
        return true;
    }

    bool SkyboxAsset::Free()
    {
        if (m_cubemapTexture != 0)
            glDeleteTextures(1, &m_cubemapTexture);

        m_cubemapTexture = 0;
        m_loaded = false;
        return true;
    }

    bool ShaderAsset::Load(std::string _path)
    {
        m_shader->Compile(
            _path + ".vs",
            _path + ".fs");

        return true;
    }

    bool ShaderAsset::Free()
    {
        delete m_shader;
        return true;
    }

    bool PostProcessAsset::Load(std::string _path)
    {
        Free();

        if (!FileExists(_path.c_str()))
        {
            Debug::Warning("PostProcess file not found: %s", _path.c_str());
            return false;
        }

        m_path = _path;
        YAML::Node root = YAML::LoadFile(_path);
        YAML::Node passesNode = root["passes"];
        if (!passesNode || !passesNode.IsSequence())
            passesNode = root;

        if (!passesNode || !passesNode.IsSequence())
            return true;

        m_passes.reserve(passesNode.size());
        for (const auto &passEntry : passesNode)
        {
            YAML::Node passNode = passEntry;
            if (!passNode || !passNode.IsMap())
                continue;

            PostProcessPass pass = {};
            pass.name = passNode["name"].as<std::string>("");
            pass.enabled = passNode["enabled"].as<bool>(true);

            YAML::Node settingsNode = passNode["settings"];
            if (!settingsNode || !settingsNode.IsMap())
                settingsNode = passNode;

            pass.exposure = settingsNode["exposure"].as<float>(pass.exposure);
            pass.contrast = settingsNode["contrast"].as<float>(pass.contrast);
            pass.saturation = settingsNode["saturation"].as<float>(pass.saturation);
            pass.bloomThreshold = settingsNode["bloomThreshold"].as<float>(pass.bloomThreshold);
            pass.bloomIntensity = settingsNode["bloomIntensity"].as<float>(pass.bloomIntensity);
            pass.ssaoRadius = settingsNode["ssaoRadius"].as<float>(pass.ssaoRadius);
            pass.ssaoBias = settingsNode["ssaoBias"].as<float>(pass.ssaoBias);
            pass.ssaoStrength = settingsNode["ssaoStrength"].as<float>(pass.ssaoStrength);

            const std::string rawShaderPath = ResolveAssetRefPath(passNode["shader"]);
            if (rawShaderPath.empty())
            {
                m_passes.push_back(pass);
                continue;
            }

            std::string shaderPath = rawShaderPath;
            if (shaderPath.size() > 3 && shaderPath.ends_with(".vs"))
                shaderPath = shaderPath.substr(0, shaderPath.size() - 3);
            else if (shaderPath.size() > 3 && shaderPath.ends_with(".fs"))
                shaderPath = shaderPath.substr(0, shaderPath.size() - 3);

            if (pass.name.empty())
                pass.name = GetFileName(shaderPath);
            pass.shaderPath = shaderPath;
            pass.shaderId = AssetManager::LoadShader(shaderPath);

            if (pass.shaderId >= 0)
            {
                if (ShaderAsset *shaderAsset = AssetManager::Get<ShaderAsset>(pass.shaderId))
                {
                    Shader *shader = shaderAsset->GetShader();
                    if (!shader->IsLinked())
                    {
                        shader->AddAttribute("vertexPosition");
                        shader->AddAttribute("vertexUV");
                        shader->Link();
                    }
                }
            }

            m_passes.push_back(pass);
        }

        return true;
    }

    bool PostProcessAsset::Free()
    {
        m_path.clear();
        m_passes.clear();
        m_passes.shrink_to_fit();
        return true;
    }

    bool TextAsset::Load(std::string _path)
    {
        m_path = _path;

#if defined(CANIS_HAS_FREETYPE)
        static constexpr int kGlyphPadding = 2;
        static constexpr float kGlyphAlphaSharpen = 1.25f;

        FT_Library fontLibrary;
        if (FT_Init_FreeType(&fontLibrary))
        {
            Debug::Error("ERROR::FREETYPE: Could not init FreeType Library");
            return false;
        }

        FT_Face fontFace;
        if (FT_New_Face(fontLibrary, _path.c_str(), 0, &fontFace))
        {
            Debug::Error("ERROR::FREETYPE: Failed to load font: %s", _path.c_str());
            FT_Done_FreeType(fontLibrary);
            return false;
        }

        FT_Set_Pixel_Sizes(fontFace, 0, m_fontSize);

        int currentX = 0;
        int currentY = 0;
        int maxHeightInRow = 0;
        memset(m_atlasData, 0, sizeof(m_atlasData));

        for (unsigned char c = 32; c < 127; c++)
        {
            if (FT_Load_Char(fontFace, c, FT_LOAD_RENDER))
            {
                Debug::Warning("Failed to load glyph: %d", c);
                continue;
            }

            const int glyphWidth = static_cast<int>(fontFace->glyph->bitmap.width);
            const int glyphHeight = static_cast<int>(fontFace->glyph->bitmap.rows);
            const int paddedWidth = std::max(glyphWidth, 1) + (kGlyphPadding * 2);
            const int paddedHeight = std::max(glyphHeight, 1) + (kGlyphPadding * 2);

            if (currentX + paddedWidth > atlasWidth)
            {
                currentX = 0;
                currentY += maxHeightInRow;
                maxHeightInRow = 0;
            }

            if (currentY + paddedHeight > atlasHeight)
            {
                Debug::Error("Text atlas is too small for font '%s' at size %u", _path.c_str(), m_fontSize);
                break;
            }

            for (int y = -kGlyphPadding; y < glyphHeight + kGlyphPadding; y++)
            {
                for (int x = -kGlyphPadding; x < glyphWidth + kGlyphPadding; x++)
                {
                    if (glyphWidth <= 0 || glyphHeight <= 0)
                        continue;

                    const int sourceX = std::clamp(x, 0, glyphWidth - 1);
                    const int sourceY = std::clamp(y, 0, glyphHeight - 1);
                    const unsigned char alpha =
                        fontFace->glyph->bitmap.buffer[(sourceY * glyphWidth) + sourceX];
                    const float alpha01 = static_cast<float>(alpha) / 255.0f;
                    const unsigned char sharpenedAlpha =
                        static_cast<unsigned char>(std::round(std::pow(alpha01, kGlyphAlphaSharpen) * 255.0f));
                    const int atlasX = currentX + kGlyphPadding + x;
                    const int atlasY = currentY + kGlyphPadding + y;
                    const int atlasIndex = ((atlasY * atlasWidth) + atlasX) * 4;

                    m_atlasData[atlasIndex + 0] = 255;
                    m_atlasData[atlasIndex + 1] = 255;
                    m_atlasData[atlasIndex + 2] = 255;
                    m_atlasData[atlasIndex + 3] = sharpenedAlpha;
                }
            }

            Character character = {};
            character.sizeX = fontFace->glyph->bitmap.width;
            character.sizeY = fontFace->glyph->bitmap.rows;
            character.bearingX = fontFace->glyph->bitmap_left;
            character.bearingY = fontFace->glyph->bitmap_top;
            character.advance = static_cast<unsigned int>(fontFace->glyph->advance.x);
            character.atlasPos = Vector2(
                static_cast<float>(currentX + kGlyphPadding) / atlasWidth,
                static_cast<float>(currentY + kGlyphPadding) / atlasHeight);
            character.atlasSize = Vector2(
                static_cast<float>(glyphWidth) / atlasWidth,
                static_cast<float>(glyphHeight) / atlasHeight);

            characters[c] = character;

            currentX += paddedWidth;
            maxHeightInRow = std::max(maxHeightInRow, paddedHeight);
        }

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_atlasData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        FT_Done_Face(fontFace);
        FT_Done_FreeType(fontLibrary);

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
#else
        Debug::Error("TextAsset::Load called but engine was built without FreeType support.");
        return false;
#endif
    }

    bool TextAsset::Free()
    {
        if (m_vbo != 0)
            glDeleteBuffers(1, &m_vbo);

        if (m_vao != 0)
            glDeleteVertexArrays(1, &m_vao);

        if (m_texture != 0)
            glDeleteTextures(1, &m_texture);

        return true;
    }

    bool AudioClipAsset::Load(std::string _path)
    {
        Free();

        const std::string extension = ToLower(GetFileExtension(_path));
        bool loaded = false;

        if (extension == "wav")
        {
            loaded = LoadWaveAudioSamples(_path, m_samples);
        }
        else if (extension == "ogg")
        {
            loaded = LoadOggAudioSamples(_path, m_samples);
        }
        else
        {
            Debug::Warning("Unsupported audio format '%s' for clip '%s'. Supported formats: .wav, .ogg", extension.c_str(), _path.c_str());
        }

        if (!loaded)
        {
            Free();
            return false;
        }

        m_path = _path;
        m_sampleRate = Audio::GetMixSpec().freq;
        m_channels = Audio::GetMixSpec().channels;
        return true;
    }

    bool AudioClipAsset::Free()
    {
        m_path.clear();
        m_samples.clear();
        m_samples.shrink_to_fit();
        m_sampleRate = 0;
        m_channels = 0;
        return true;
    }

    std::string FileTypeToString(MetaFileAsset::FileType _type)
    {
        switch (_type)
        {
            case MetaFileAsset::FileType::FRAGMENT:
                return "FRAGMENT";
            case MetaFileAsset::FileType::VERTEX:
                return "VERTEX";
            case MetaFileAsset::FileType::TEXTURE:
                return "TEXTURE";
            case MetaFileAsset::FileType::AUDIO:
                return "AUDIO";
            case MetaFileAsset::FileType::SCENE:
                return "SCENE";
            case MetaFileAsset::FileType::ANIMATIONCLIP2D:
                return "ANIMATIONCLIP2D";
            case MetaFileAsset::FileType::ANIMATIONCLIP:
                return "ANIMATIONCLIP";
            case MetaFileAsset::FileType::ANIMATORCONTROLLER:
                return "ANIMATORCONTROLLER";
            case MetaFileAsset::FileType::MODEL:
                return "MODEL";
            case MetaFileAsset::FileType::MATERIAL:
                return "MATERIAL";
            case MetaFileAsset::FileType::SKYBOX:
                return "SKYBOX";
            case MetaFileAsset::FileType::POSTPROCESS:
                return "POSTPROCESS";
            case MetaFileAsset::FileType::SHADERGRAPH:
                return "SHADERGRAPH";
            default:
                return "FILE_UNKNOWN";
        }
    }

    MetaFileAsset::FileType StringToFileType(std::string _type)
    {
        if (_type == "FRAGMENT")
            return MetaFileAsset::FileType::FRAGMENT;
        else if (_type == "VERTEX")
            return MetaFileAsset::FileType::VERTEX;
        else if (_type == "TEXTURE")
            return MetaFileAsset::FileType::TEXTURE;
        else if (_type == "AUDIO")
            return MetaFileAsset::FileType::AUDIO;
        else if (_type == "SCENE")
            return MetaFileAsset::FileType::SCENE;
        else if (_type == "ANIMATIONCLIP2D")
            return MetaFileAsset::FileType::ANIMATIONCLIP2D;
        else if (_type == "ANIMATIONCLIP")
            return MetaFileAsset::FileType::ANIMATIONCLIP;
        else if (_type == "ANIMATORCONTROLLER")
            return MetaFileAsset::FileType::ANIMATORCONTROLLER;
        else if (_type == "MODEL")
            return MetaFileAsset::FileType::MODEL;
        else if (_type == "MATERIAL")
            return MetaFileAsset::FileType::MATERIAL;
        else if (_type == "SKYBOX")
            return MetaFileAsset::FileType::SKYBOX;
        else if (_type == "POSTPROCESS")
            return MetaFileAsset::FileType::POSTPROCESS;
        else if (_type == "SHADERGRAPH")
            return MetaFileAsset::FileType::SHADERGRAPH;
        else
            return MetaFileAsset::FileType::FILE_UNKNOWN;
    }
    
    void MetaFileAsset::CreateMetaFile(std::string _path)
    {
        SDL_PathInfo info;

        if (SDL_GetPathInfo(_path.c_str(), &info))
        {
            path = _path;
            name = GetFileName(_path);
            extension = ToLower(GetFileExtension(_path));

            if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "bmp" || extension == "tga")
                type = FileType::TEXTURE;
            else if (extension == "wav" || extension == "ogg")
                type = FileType::AUDIO;
            else if (extension == "scene")
                type = FileType::SCENE;
            else if (extension == "fs")
                type = FileType::FRAGMENT;
            else if (extension == "vs")
                type = FileType::VERTEX;
            else if (extension == "ac2d")
                type = FileType::ANIMATIONCLIP2D;
            else if (extension == "animclip")
                type = FileType::ANIMATIONCLIP;
            else if (extension == "animator")
                type = FileType::ANIMATORCONTROLLER;
            else if (extension == "gltf" || extension == "glb" || extension == "obj")
                type = FileType::MODEL;
            else if (extension == "material")
                type = FileType::MATERIAL;
            else if (extension == "skybox")
                type = FileType::SKYBOX;
            else if (extension == "postprocess")
                type = FileType::POSTPROCESS;
            else if (extension == "shadergraph")
                type = FileType::SHADERGRAPH;
            else
                type = FileType::FILE_UNKNOWN;

            size = info.size;
            modified = info.modify_time;

            Save();
        }
    }

    void MetaFileAsset::Save()
    {
        YAML::Node node;
        node["FileType"] = FileTypeToString(type);
        node["UUID"] = std::to_string(uuid);
        node["name"] = name;
        node["extension"] = extension;
        node["size"] = size;
        node["modified"] = modified;

        std::ofstream fout(path + ".meta");
        fout << node;
    }

    bool MetaFileAsset::Load(std::string _path)
    {
        if (FileExists((_path + ".meta").c_str()))
        {
            YAML::Node root = YAML::LoadFile(_path+".meta");
            type = StringToFileType(root["FileType"].as<std::string>());
            uuid = root["UUID"].as<uint64_t>(0);
            path = _path;
            name = root["name"].as<std::string>();
            extension = ToLower(root["extension"].as<std::string>(GetFileExtension(_path)));
            size = root["size"].as<u64>();
            modified = root["modified"].as<i64>();

            if (type == FileType::FILE_UNKNOWN && (extension == "gltf" || extension == "glb" || extension == "obj"))
                type = FileType::MODEL;
        }
        else
        {
            CreateMetaFile(_path);
        }

        return false;
    }

    namespace
    {
        bool LoadTinyGLTFImageData(
            tinygltf::Image *_image,
            const int /*_imageIndex*/,
            std::string *_error,
            std::string* /*_warning*/,
            int /*_reqWidth*/,
            int /*_reqHeight*/,
            const unsigned char *_bytes,
            int _size,
            void* /*_userPointer*/)
        {
            if (_image == nullptr || _bytes == nullptr || _size <= 0)
            {
                if (_error != nullptr)
                    *_error += "Invalid glTF image payload.\n";
                return false;
            }

            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* decoded = stbi_load_from_memory(_bytes, _size, &width, &height, &channels, 4);
            if (decoded == nullptr)
            {
                if (_error != nullptr)
                {
                    *_error += "Failed to decode glTF image data";
                    if (const char* reason = stbi_failure_reason())
                    {
                        *_error += ": ";
                        *_error += reason;
                    }
                    *_error += "\n";
                }
                return false;
            }

            _image->width = width;
            _image->height = height;
            _image->component = 4;
            _image->bits = 8;
            _image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
            _image->image.assign(decoded, decoded + (width * height * 4));

            stbi_image_free(decoded);
            return true;
        }

        float ReadScalarAsFloat(const unsigned char *_data, int _componentType, bool _normalized)
        {
            switch (_componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return *reinterpret_cast<const float*>(_data);
                case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                    return static_cast<float>(*reinterpret_cast<const double*>(_data));
                case TINYGLTF_COMPONENT_TYPE_BYTE:
                {
                    const int8_t value = *reinterpret_cast<const int8_t*>(_data);
                    if (_normalized)
                        return std::fmax(-1.0f, value / 127.0f);
                    return static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                {
                    const uint8_t value = *reinterpret_cast<const uint8_t*>(_data);
                    if (_normalized)
                        return value / 255.0f;
                    return static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_SHORT:
                {
                    const int16_t value = *reinterpret_cast<const int16_t*>(_data);
                    if (_normalized)
                        return std::fmax(-1.0f, value / 32767.0f);
                    return static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                {
                    const uint16_t value = *reinterpret_cast<const uint16_t*>(_data);
                    if (_normalized)
                        return value / 65535.0f;
                    return static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_INT:
                    return static_cast<float>(*reinterpret_cast<const int32_t*>(_data));
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    return static_cast<float>(*reinterpret_cast<const uint32_t*>(_data));
                default:
                    return 0.0f;
            }
        }

        bool ReadAccessorFloats(const tinygltf::Model &_model, const tinygltf::Accessor &_accessor, std::vector<float> &_values)
        {
            if (_accessor.bufferView < 0 || _accessor.bufferView >= (int)_model.bufferViews.size())
                return false;

            if (_accessor.sparse.isSparse)
            {
                Debug::Warning("Sparse accessors are not supported yet.");
                return false;
            }

            const tinygltf::BufferView &bufferView = _model.bufferViews[_accessor.bufferView];
            if (bufferView.buffer < 0 || bufferView.buffer >= (int)_model.buffers.size())
                return false;

            const tinygltf::Buffer &buffer = _model.buffers[bufferView.buffer];
            const int componentsPerElement = tinygltf::GetNumComponentsInType(_accessor.type);
            const int componentSize = tinygltf::GetComponentSizeInBytes(_accessor.componentType);
            if (componentsPerElement <= 0 || componentSize <= 0)
                return false;

            int stride = _accessor.ByteStride(bufferView);
            if (stride <= 0)
                stride = componentsPerElement * componentSize;

            const size_t byteOffset = static_cast<size_t>(bufferView.byteOffset) + static_cast<size_t>(_accessor.byteOffset);
            if (byteOffset >= buffer.data.size())
                return false;

            const unsigned char *base = buffer.data.data() + byteOffset;
            _values.resize(static_cast<size_t>(_accessor.count) * static_cast<size_t>(componentsPerElement));

            for (size_t i = 0; i < _accessor.count; ++i)
            {
                const unsigned char *element = base + i * static_cast<size_t>(stride);
                for (int c = 0; c < componentsPerElement; ++c)
                {
                    const unsigned char *component = element + c * static_cast<size_t>(componentSize);
                    _values[i * static_cast<size_t>(componentsPerElement) + static_cast<size_t>(c)] =
                        ReadScalarAsFloat(component, _accessor.componentType, _accessor.normalized);
                }
            }

            return true;
        }

        bool ReadAccessorIndices(const tinygltf::Model &_model, const tinygltf::Accessor &_accessor, std::vector<unsigned int> &_indices)
        {
            std::vector<float> values;
            if (!ReadAccessorFloats(_model, _accessor, values))
                return false;

            _indices.resize(values.size());
            for (size_t i = 0; i < values.size(); ++i)
            {
                const float value = values[i];
                _indices[i] = (value < 0.0f) ? 0u : static_cast<unsigned int>(value);
            }

            return true;
        }

        std::vector<Vector4> ConvertToVec4(const std::vector<float> &_values, int _components)
        {
            std::vector<Vector4> out;
            if (_components <= 0)
                return out;

            out.resize(_values.size() / static_cast<size_t>(_components));
            for (size_t i = 0; i < out.size(); ++i)
            {
                const size_t base = i * static_cast<size_t>(_components);
                Vector4 value = Vector4(0.0f);
                if (_components > 0) value.x = _values[base + 0];
                if (_components > 1) value.y = _values[base + 1];
                if (_components > 2) value.z = _values[base + 2];
                if (_components > 3) value.w = _values[base + 3];
                out[i] = value;
            }

            return out;
        }

        Matrix4 MatrixFromNode(const tinygltf::Node &_node)
        {
            Matrix4 matrix = Matrix4(1.0f);

            if (_node.matrix.size() == 16)
            {
                for (size_t i = 0; i < 16; ++i)
                    matrix[i / 4][i % 4] = static_cast<float>(_node.matrix[i]);
                return matrix;
            }

            Vector3 translation = Vector3(0.0f);
            Vector4 rotation = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            Vector3 scale = Vector3(1.0f);

            if (_node.translation.size() == 3)
                translation = Vector3(
                    static_cast<float>(_node.translation[0]),
                    static_cast<float>(_node.translation[1]),
                    static_cast<float>(_node.translation[2]));

            if (_node.rotation.size() == 4)
                rotation = Vector4(
                    static_cast<float>(_node.rotation[0]),
                    static_cast<float>(_node.rotation[1]),
                    static_cast<float>(_node.rotation[2]),
                    static_cast<float>(_node.rotation[3]));

            if (_node.scale.size() == 3)
                scale = Vector3(
                    static_cast<float>(_node.scale[0]),
                    static_cast<float>(_node.scale[1]),
                    static_cast<float>(_node.scale[2]));

            const glm::quat q = glm::normalize(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
            return glm::translate(Matrix4(1.0f), translation) * glm::mat4_cast(q) * glm::scale(Matrix4(1.0f), scale);
        }

        std::string SanitizeEmbeddedTextureName(std::string _name)
        {
            if (_name.empty())
                return "embedded_texture";

            for (char &c : _name)
            {
                const bool valid =
                    (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '_' ||
                    c == '-';

                if (!valid)
                    c = '_';
            }

            return _name;
        }

        bool WriteRgbaTga(
            const std::filesystem::path &_path,
            int _width,
            int _height,
            const std::vector<unsigned char> &_rgbaPixels)
        {
            if (_width <= 0 || _height <= 0 || _rgbaPixels.size() < static_cast<std::size_t>(_width * _height * 4))
                return false;

            std::ofstream out(_path, std::ios::binary);
            if (!out.is_open())
                return false;

            unsigned char header[18] = {};
            header[2] = 2;
            header[12] = static_cast<unsigned char>(_width & 0xFF);
            header[13] = static_cast<unsigned char>((_width >> 8) & 0xFF);
            header[14] = static_cast<unsigned char>(_height & 0xFF);
            header[15] = static_cast<unsigned char>((_height >> 8) & 0xFF);
            header[16] = 32;
            header[17] = 0x20;
            out.write(reinterpret_cast<const char *>(header), sizeof(header));

            for (int y = 0; y < _height; ++y)
            {
                for (int x = 0; x < _width; ++x)
                {
                    const std::size_t offset = static_cast<std::size_t>((y * _width + x) * 4);
                    const unsigned char bgra[4] = {
                        _rgbaPixels[offset + 2],
                        _rgbaPixels[offset + 1],
                        _rgbaPixels[offset + 0],
                        _rgbaPixels[offset + 3]
                    };
                    out.write(reinterpret_cast<const char *>(bgra), sizeof(bgra));
                }
            }

            return out.good();
        }

        std::string ExtractEmbeddedTexture(
            const tinygltf::Image &_image,
            int _imageIndex,
            const std::filesystem::path &_modelPath)
        {
            if (_image.image.empty() || _image.width <= 0 || _image.height <= 0)
                return "";

            if (_image.component != 4)
            {
                Debug::Warning("Embedded texture has %d channels. Only RGBA embedded textures are supported for extraction.", _image.component);
                return "";
            }

            const std::filesystem::path modelDirectory = _modelPath.parent_path();
            const std::string modelStem = _modelPath.stem().string().empty() ? "model" : _modelPath.stem().string();
            const std::filesystem::path textureDirectory = modelDirectory / (modelStem + "_textures");

            std::string textureName = _image.name;
            if (textureName.empty())
                textureName = "embedded_texture_" + std::to_string(_imageIndex);
            textureName = SanitizeEmbeddedTextureName(textureName);

            const std::filesystem::path texturePath = textureDirectory / (textureName + ".tga");

            std::error_code ec;
            std::filesystem::create_directories(textureDirectory, ec);
            if (ec)
            {
                Debug::Warning("Failed to create embedded texture directory '%s': %s", textureDirectory.generic_string().c_str(), ec.message().c_str());
                return "";
            }

            if (!std::filesystem::exists(texturePath))
            {
                if (!WriteRgbaTga(texturePath, _image.width, _image.height, _image.image))
                {
                    Debug::Warning("Failed to write embedded texture '%s'.", texturePath.generic_string().c_str());
                    return "";
                }
            }

            const std::string outputPath = texturePath.generic_string();
            (void)AssetManager::GetMetaFile(outputPath);
            return outputPath;
        }

        std::string ResolveTexturePath(
            const tinygltf::Model &_gltfModel,
            const tinygltf::Primitive &_primitive,
            const std::filesystem::path &_modelPath)
        {
            if (_primitive.material < 0 || _primitive.material >= (int)_gltfModel.materials.size())
                return "";

            const tinygltf::Material &material = _gltfModel.materials[_primitive.material];
            const int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            if (textureIndex < 0 || textureIndex >= (int)_gltfModel.textures.size())
                return "";

            const tinygltf::Texture &texture = _gltfModel.textures[textureIndex];
            if (texture.source < 0 || texture.source >= (int)_gltfModel.images.size())
                return "";

            const tinygltf::Image &image = _gltfModel.images[texture.source];
            if (image.uri.empty() || image.uri.rfind("data:", 0) == 0)
            {
                return ExtractEmbeddedTexture(image, texture.source, _modelPath);
            }

            const std::filesystem::path modelDirectory = _modelPath.parent_path();
            std::filesystem::path texturePath = modelDirectory / image.uri;
            return texturePath.generic_string();
        }

        Vector4 EvaluateAnimationSampler(const ModelAsset::AnimationSampler3D &_sampler, float _time, bool _rotation)
        {
            if (_sampler.inputs.empty() || _sampler.outputs.empty())
                return _rotation ? Vector4(0.0f, 0.0f, 0.0f, 1.0f) : Vector4(0.0f);

            const size_t keyCount = _sampler.inputs.size();
            const size_t step = _sampler.cubicSpline ? 3u : 1u;

            auto valueAt = [&](size_t _index) -> Vector4
            {
                const size_t outputIndex = _sampler.cubicSpline ? (_index * 3u + 1u) : _index;
                if (outputIndex < _sampler.outputs.size())
                    return _sampler.outputs[outputIndex];
                return _rotation ? Vector4(0.0f, 0.0f, 0.0f, 1.0f) : Vector4(0.0f);
            };

            if (keyCount == 1 || _time <= _sampler.inputs.front())
            {
                if (!_rotation)
                    return valueAt(0u);

                const Vector4 value = valueAt(0u);
                const glm::quat q = glm::normalize(glm::quat(value.w, value.x, value.y, value.z));
                return Vector4(q.x, q.y, q.z, q.w);
            }

            if (_time >= _sampler.inputs.back())
            {
                if (!_rotation)
                    return valueAt(keyCount - 1u);

                const Vector4 value = valueAt(keyCount - 1u);
                const glm::quat q = glm::normalize(glm::quat(value.w, value.x, value.y, value.z));
                return Vector4(q.x, q.y, q.z, q.w);
            }

            size_t rightKey = 1u;
            while (rightKey < keyCount && _time > _sampler.inputs[rightKey])
                ++rightKey;

            const size_t leftKey = rightKey - 1u;
            const float startTime = _sampler.inputs[leftKey];
            const float endTime = _sampler.inputs[rightKey];
            const float segmentDuration = endTime - startTime;
            const float t = (segmentDuration > 0.0f) ? ((_time - startTime) / segmentDuration) : 0.0f;

            const Vector4 v0 = valueAt(leftKey);
            const Vector4 v1 = valueAt(rightKey);

            if (_sampler.interpolation == "STEP")
            {
                if (!_rotation)
                    return v0;

                const glm::quat q = glm::normalize(glm::quat(v0.w, v0.x, v0.y, v0.z));
                return Vector4(q.x, q.y, q.z, q.w);
            }

            if (_sampler.cubicSpline)
            {
                const size_t leftBase = leftKey * step;
                const size_t rightBase = rightKey * step;

                const Vector4 outTangent = _sampler.outputs[leftBase + 2u] * segmentDuration;
                const Vector4 inTangent = _sampler.outputs[rightBase + 0u] * segmentDuration;

                const float t2 = t * t;
                const float t3 = t2 * t;
                const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
                const float h10 = t3 - 2.0f * t2 + t;
                const float h01 = -2.0f * t3 + 3.0f * t2;
                const float h11 = t3 - t2;

                const Vector4 value = (v0 * h00) + (outTangent * h10) + (v1 * h01) + (inTangent * h11);
                if (!_rotation)
                    return value;

                const glm::quat q = glm::normalize(glm::quat(value.w, value.x, value.y, value.z));
                return Vector4(q.x, q.y, q.z, q.w);
            }

            if (_rotation)
            {
                const glm::quat q0 = glm::normalize(glm::quat(v0.w, v0.x, v0.y, v0.z));
                const glm::quat q1 = glm::normalize(glm::quat(v1.w, v1.x, v1.y, v1.z));
                const glm::quat q = glm::normalize(glm::slerp(q0, q1, t));
                return Vector4(q.x, q.y, q.z, q.w);
            }

            return v0 + ((v1 - v0) * t);
        }
    } // namespace

    bool ModelAsset::Load(std::string _path)
    {
        m_path = _path;
        FreePrimitives();
        m_nodes.clear();
        m_sceneRoots.clear();
        m_skins.clear();
        m_animations.clear();
        m_materialSlotNames.clear();
        m_bindTranslations.clear();
        m_bindRotations.clear();
        m_bindScales.clear();
        m_bindLocalMatrices.clear();

        const std::string extension = ToLower(GetFileExtension(_path));
        if (extension == "obj")
        {
            std::vector<PrimitiveBuild3D> primitives = {};
            std::vector<std::string> materialSlotNames = {};
            if (!LoadOBJPrimitives(_path, primitives, materialSlotNames))
                return false;

            if (!SetRuntimePrimitives(primitives, materialSlotNames))
                return false;

            m_path = _path;
            return true;
        }

        if (extension != "gltf" && extension != "glb")
        {
            Debug::Warning("Unsupported model format '%s' for '%s'.", extension.c_str(), _path.c_str());
            return false;
        }

        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(LoadTinyGLTFImageData, nullptr);
        tinygltf::Model gltfModel;
        std::string error;
        std::string warning;

        bool loaded = false;
        if (extension == "glb")
            loaded = loader.LoadBinaryFromFile(&gltfModel, &error, &warning, _path);
        else
            loaded = loader.LoadASCIIFromFile(&gltfModel, &error, &warning, _path);

        if (!warning.empty())
            Debug::Warning("tinygltf: %s", warning.c_str());

        if (!loaded)
        {
            Debug::Warning("Failed to load glTF mesh %s. Error: %s", _path.c_str(), error.c_str());
            return false;
        }

        m_nodes.resize(gltfModel.nodes.size());
        m_bindTranslations.resize(gltfModel.nodes.size(), Vector3(0.0f));
        m_bindRotations.resize(gltfModel.nodes.size(), Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        m_bindScales.resize(gltfModel.nodes.size(), Vector3(1.0f));
        m_bindLocalMatrices.resize(gltfModel.nodes.size(), Matrix4(1.0f));

        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node &gltfNode = gltfModel.nodes[i];
            Node3D node;
            node.name = gltfNode.name;
            node.mesh = gltfNode.mesh;
            node.skin = gltfNode.skin;
            node.hasMatrix = (gltfNode.matrix.size() == 16);

            if (gltfNode.translation.size() == 3)
            {
                node.translation = Vector3(
                    static_cast<float>(gltfNode.translation[0]),
                    static_cast<float>(gltfNode.translation[1]),
                    static_cast<float>(gltfNode.translation[2]));
            }

            if (gltfNode.rotation.size() == 4)
            {
                node.rotation = Vector4(
                    static_cast<float>(gltfNode.rotation[0]),
                    static_cast<float>(gltfNode.rotation[1]),
                    static_cast<float>(gltfNode.rotation[2]),
                    static_cast<float>(gltfNode.rotation[3]));
            }

            if (gltfNode.scale.size() == 3)
            {
                node.scale = Vector3(
                    static_cast<float>(gltfNode.scale[0]),
                    static_cast<float>(gltfNode.scale[1]),
                    static_cast<float>(gltfNode.scale[2]));
            }

            for (int child : gltfNode.children)
                node.children.push_back(child);

            node.localMatrix = MatrixFromNode(gltfNode);
            node.globalMatrix = node.localMatrix;

            m_bindTranslations[i] = node.translation;
            m_bindRotations[i] = node.rotation;
            m_bindScales[i] = node.scale;
            m_bindLocalMatrices[i] = node.localMatrix;
            m_nodes[i] = node;
        }

        for (size_t i = 0; i < m_nodes.size(); ++i)
        {
            for (int child : m_nodes[i].children)
            {
                if (child >= 0 && child < (int)m_nodes.size())
                    m_nodes[child].parent = static_cast<i32>(i);
            }
        }

        if (gltfModel.defaultScene >= 0 && gltfModel.defaultScene < (int)gltfModel.scenes.size())
        {
            for (int rootNode : gltfModel.scenes[gltfModel.defaultScene].nodes)
                m_sceneRoots.push_back(rootNode);
        }
        else if (!gltfModel.scenes.empty())
        {
            for (int rootNode : gltfModel.scenes[0].nodes)
                m_sceneRoots.push_back(rootNode);
        }

        if (m_sceneRoots.empty())
        {
            for (size_t i = 0; i < m_nodes.size(); ++i)
            {
                if (m_nodes[i].parent < 0)
                    m_sceneRoots.push_back(static_cast<i32>(i));
            }
        }

        m_skins.resize(gltfModel.skins.size());
        for (size_t i = 0; i < gltfModel.skins.size(); ++i)
        {
            const tinygltf::Skin &skin = gltfModel.skins[i];
            Skin3D localSkin;
            for (int joint : skin.joints)
                localSkin.joints.push_back(joint);

            localSkin.inverseBindMatrices.resize(localSkin.joints.size(), Matrix4(1.0f));
            if (skin.inverseBindMatrices >= 0 && skin.inverseBindMatrices < (int)gltfModel.accessors.size())
            {
                const tinygltf::Accessor &accessor = gltfModel.accessors[skin.inverseBindMatrices];
                std::vector<float> matrixData;
                if (ReadAccessorFloats(gltfModel, accessor, matrixData) && matrixData.size() >= accessor.count * 16u)
                {
                    const size_t matrixCount = std::min<size_t>(accessor.count, localSkin.inverseBindMatrices.size());
                    for (size_t matrixIndex = 0; matrixIndex < matrixCount; ++matrixIndex)
                    {
                        Matrix4 matrix = Matrix4(1.0f);
                        for (size_t c = 0; c < 16; ++c)
                            matrix[c / 4][c % 4] = matrixData[matrixIndex * 16u + c];
                        localSkin.inverseBindMatrices[matrixIndex] = matrix;
                    }
                }
            }

            m_skins[i] = localSkin;
        }

        m_animations.resize(gltfModel.animations.size());
        for (size_t i = 0; i < gltfModel.animations.size(); ++i)
        {
            const tinygltf::Animation &gltfAnimation = gltfModel.animations[i];
            AnimationClip3D clip;
            clip.name = gltfAnimation.name;
            clip.duration = 0.0f;
            clip.samplers.resize(gltfAnimation.samplers.size());

            for (size_t samplerIndex = 0; samplerIndex < gltfAnimation.samplers.size(); ++samplerIndex)
            {
                const tinygltf::AnimationSampler &gltfSampler = gltfAnimation.samplers[samplerIndex];
                AnimationSampler3D sampler;
                sampler.interpolation = gltfSampler.interpolation.empty() ? "LINEAR" : gltfSampler.interpolation;
                sampler.cubicSpline = (sampler.interpolation == "CUBICSPLINE");

                if (gltfSampler.input >= 0 && gltfSampler.input < (int)gltfModel.accessors.size())
                {
                    const tinygltf::Accessor &inputAccessor = gltfModel.accessors[gltfSampler.input];
                    ReadAccessorFloats(gltfModel, inputAccessor, sampler.inputs);
                    if (!sampler.inputs.empty())
                        clip.duration = std::max(clip.duration, sampler.inputs.back());
                }

                if (gltfSampler.output >= 0 && gltfSampler.output < (int)gltfModel.accessors.size())
                {
                    const tinygltf::Accessor &outputAccessor = gltfModel.accessors[gltfSampler.output];
                    std::vector<float> outputFloats;
                    if (ReadAccessorFloats(gltfModel, outputAccessor, outputFloats))
                    {
                        const int componentCount = tinygltf::GetNumComponentsInType(outputAccessor.type);
                        sampler.outputs = ConvertToVec4(outputFloats, componentCount);
                    }
                }

                clip.samplers[samplerIndex] = sampler;
            }

            for (const tinygltf::AnimationChannel &gltfChannel : gltfAnimation.channels)
            {
                AnimationChannel3D channel;
                channel.sampler = gltfChannel.sampler;
                channel.targetNode = gltfChannel.target_node;

                if (gltfChannel.target_path == "translation")
                    channel.path = AnimationPath3D::TRANSLATION;
                else if (gltfChannel.target_path == "rotation")
                    channel.path = AnimationPath3D::ROTATION;
                else if (gltfChannel.target_path == "scale")
                    channel.path = AnimationPath3D::SCALE;
                else
                    continue;

                clip.channels.push_back(channel);
            }

            m_animations[i] = clip;
        }

        std::unordered_map<int, int> gltfMaterialToSlot = {};
        auto getMaterialSlot = [&](int _gltfMaterialIndex) -> int
        {
            if (_gltfMaterialIndex < 0 || _gltfMaterialIndex >= (int)gltfModel.materials.size())
                return -1;

            auto it = gltfMaterialToSlot.find(_gltfMaterialIndex);
            if (it != gltfMaterialToSlot.end())
                return it->second;

            const int slot = static_cast<int>(m_materialSlotNames.size());
            gltfMaterialToSlot[_gltfMaterialIndex] = slot;

            std::string slotName = gltfModel.materials[_gltfMaterialIndex].name;
            if (slotName.empty())
                slotName = "Material " + std::to_string(slot);
            m_materialSlotNames.push_back(slotName);
            return slot;
        };

        bool skinningWarningPrinted = false;
        for (size_t nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex)
        {
            const Node3D &node = m_nodes[nodeIndex];
            if (node.mesh < 0 || node.mesh >= (int)gltfModel.meshes.size())
                continue;

            const tinygltf::Mesh &mesh = gltfModel.meshes[node.mesh];
            for (const tinygltf::Primitive &primitive : mesh.primitives)
            {
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
                    continue;

                auto positionIt = primitive.attributes.find("POSITION");
                if (positionIt == primitive.attributes.end())
                    continue;

                if (positionIt->second < 0 || positionIt->second >= (int)gltfModel.accessors.size())
                    continue;

                const tinygltf::Accessor &positionAccessor = gltfModel.accessors[positionIt->second];
                std::vector<float> positions;
                if (!ReadAccessorFloats(gltfModel, positionAccessor, positions))
                    continue;

                Primitive3D outputPrimitive;
                outputPrimitive.nodeIndex = static_cast<i32>(nodeIndex);
                outputPrimitive.skinIndex = node.skin;
                outputPrimitive.materialSlot = getMaterialSlot(primitive.material);

                const size_t vertexCount = positionAccessor.count;
                outputPrimitive.bindVertices.resize(vertexCount);
                outputPrimitive.skinnedVertices.resize(vertexCount);

                for (size_t v = 0; v < vertexCount; ++v)
                {
                    outputPrimitive.bindVertices[v].position = Vector3(
                        positions[v * 3u + 0u],
                        positions[v * 3u + 1u],
                        positions[v * 3u + 2u]);
                }

                auto normalIt = primitive.attributes.find("NORMAL");
                if (normalIt != primitive.attributes.end() && normalIt->second >= 0 && normalIt->second < (int)gltfModel.accessors.size())
                {
                    const tinygltf::Accessor &normalAccessor = gltfModel.accessors[normalIt->second];
                    std::vector<float> normals;
                    if (ReadAccessorFloats(gltfModel, normalAccessor, normals) && normalAccessor.count == vertexCount)
                    {
                        for (size_t v = 0; v < vertexCount; ++v)
                        {
                            outputPrimitive.bindVertices[v].normal = Vector3(
                                normals[v * 3u + 0u],
                                normals[v * 3u + 1u],
                                normals[v * 3u + 2u]);
                        }
                    }
                }

                auto uvIt = primitive.attributes.find("TEXCOORD_0");
                if (uvIt != primitive.attributes.end() && uvIt->second >= 0 && uvIt->second < (int)gltfModel.accessors.size())
                {
                    const tinygltf::Accessor &uvAccessor = gltfModel.accessors[uvIt->second];
                    std::vector<float> uvs;
                    if (ReadAccessorFloats(gltfModel, uvAccessor, uvs) && uvAccessor.count == vertexCount)
                    {
                        for (size_t v = 0; v < vertexCount; ++v)
                        {
                            outputPrimitive.bindVertices[v].uv = Vector2(
                                uvs[v * 2u + 0u],
                                uvs[v * 2u + 1u]);
                        }
                    }
                }

                auto jointsIt = primitive.attributes.find("JOINTS_0");
                auto weightsIt = primitive.attributes.find("WEIGHTS_0");
                if (jointsIt != primitive.attributes.end() && weightsIt != primitive.attributes.end() && node.skin >= 0)
                {
                    if (jointsIt->second >= 0 && jointsIt->second < (int)gltfModel.accessors.size() &&
                        weightsIt->second >= 0 && weightsIt->second < (int)gltfModel.accessors.size())
                    {
                        const tinygltf::Accessor &jointsAccessor = gltfModel.accessors[jointsIt->second];
                        const tinygltf::Accessor &weightsAccessor = gltfModel.accessors[weightsIt->second];

                        std::vector<float> joints;
                        std::vector<float> weights;
                        if (ReadAccessorFloats(gltfModel, jointsAccessor, joints) &&
                            ReadAccessorFloats(gltfModel, weightsAccessor, weights) &&
                            jointsAccessor.count == vertexCount &&
                            weightsAccessor.count == vertexCount)
                        {
                            outputPrimitive.hasSkinning = true;
                            outputPrimitive.dynamicVertices = true;
                            outputPrimitive.skinVertices.resize(vertexCount);
                            for (size_t v = 0; v < vertexCount; ++v)
                            {
                                outputPrimitive.skinVertices[v].joints = Vector4(
                                    joints[v * 4u + 0u],
                                    joints[v * 4u + 1u],
                                    joints[v * 4u + 2u],
                                    joints[v * 4u + 3u]);
                                outputPrimitive.skinVertices[v].weights = Vector4(
                                    weights[v * 4u + 0u],
                                    weights[v * 4u + 1u],
                                    weights[v * 4u + 2u],
                                    weights[v * 4u + 3u]);
                            }
                        }
                    }
                }

                if (!outputPrimitive.hasSkinning &&
                    jointsIt != primitive.attributes.end() &&
                    weightsIt != primitive.attributes.end() &&
                    !skinningWarningPrinted)
                {
                    Debug::Warning("glTF mesh %s contains JOINTS_0/WEIGHTS_0. Rendering as static mesh (skinning ignored).", _path.c_str());
                    skinningWarningPrinted = true;
                }

                if (primitive.indices >= 0 && primitive.indices < (int)gltfModel.accessors.size())
                {
                    const tinygltf::Accessor &indexAccessor = gltfModel.accessors[primitive.indices];
                    ReadAccessorIndices(gltfModel, indexAccessor, outputPrimitive.indices);
                }
                else
                {
                    outputPrimitive.indices.resize(vertexCount);
                    for (size_t i = 0; i < vertexCount; ++i)
                        outputPrimitive.indices[i] = static_cast<unsigned int>(i);
                }

                outputPrimitive.skinnedVertices = outputPrimitive.bindVertices;
                std::string texturePath = ResolveTexturePath(gltfModel, primitive, std::filesystem::path(_path));
                if (!texturePath.empty())
                    outputPrimitive.textureId = AssetManager::LoadTexture(texturePath);

                if (!UploadPrimitive(outputPrimitive))
                    continue;

                m_primitives.push_back(outputPrimitive);
            }
        }

        ResetPose();
        ++m_geometryRevision;
        return true;
    }

    bool ModelAsset::SetRuntimePrimitives(
        const std::vector<PrimitiveBuild3D> &_primitives,
        const std::vector<std::string> &_materialSlotNames)
    {
        FreePrimitives();

        m_path.clear();
        m_nodes.clear();
        m_sceneRoots.clear();
        m_skins.clear();
        m_animations.clear();
        m_materialSlotNames = _materialSlotNames;
        m_bindTranslations.clear();
        m_bindRotations.clear();
        m_bindScales.clear();
        m_bindLocalMatrices.clear();
        m_sharedPose = {};

        m_primitives.reserve(_primitives.size());
        for (const PrimitiveBuild3D &buildPrimitive : _primitives)
        {
            Primitive3D primitive;
            primitive.bindVertices = buildPrimitive.vertices;
            primitive.skinnedVertices = buildPrimitive.vertices;
            primitive.skinVertices = buildPrimitive.skinVertices;
            primitive.indices = buildPrimitive.indices;
            primitive.nodeIndex = buildPrimitive.nodeIndex;
            primitive.skinIndex = buildPrimitive.skinIndex;
            primitive.materialSlot = buildPrimitive.materialSlot;
            primitive.textureId = buildPrimitive.textureId;
            primitive.hasSkinning = !primitive.skinVertices.empty() &&
                primitive.skinVertices.size() == primitive.bindVertices.size();
            primitive.dynamicVertices = buildPrimitive.dynamicVertices || primitive.hasSkinning;

            if (!UploadPrimitive(primitive))
            {
                FreePrimitives();
                return false;
            }

            m_primitives.push_back(std::move(primitive));
        }

        ++m_geometryRevision;
        return true;
    }

    void ModelAsset::FreePrimitives()
    {
        for (Primitive3D &primitive : m_primitives)
        {
            if (primitive.instanceVbo != 0)
                glDeleteBuffers(1, &primitive.instanceVbo);
            if (primitive.ebo != 0)
                glDeleteBuffers(1, &primitive.ebo);
            if (primitive.vbo != 0)
                glDeleteBuffers(1, &primitive.vbo);
            if (primitive.vao != 0)
                glDeleteVertexArrays(1, &primitive.vao);

            primitive.instanceVbo = 0;
            primitive.ebo = 0;
            primitive.vbo = 0;
            primitive.vao = 0;
        }

        m_primitives.clear();
    }

    bool ModelAsset::UploadPrimitive(Primitive3D &_primitive) const
    {
        if (_primitive.bindVertices.empty() || _primitive.indices.empty())
            return false;

        if (_primitive.skinnedVertices.size() != _primitive.bindVertices.size())
            _primitive.skinnedVertices = _primitive.bindVertices;

        if (_primitive.vao == 0)
            glGenVertexArrays(1, &_primitive.vao);
        if (_primitive.vbo == 0)
            glGenBuffers(1, &_primitive.vbo);
        if (_primitive.ebo == 0)
            glGenBuffers(1, &_primitive.ebo);

        glBindVertexArray(_primitive.vao);

        glBindBuffer(GL_ARRAY_BUFFER, _primitive.vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            _primitive.skinnedVertices.size() * sizeof(RenderVertex3D),
            _primitive.skinnedVertices.data(),
            _primitive.dynamicVertices ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _primitive.ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            _primitive.indices.size() * sizeof(unsigned int),
            _primitive.indices.data(),
            GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex3D), (void*)offsetof(RenderVertex3D, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex3D), (void*)offsetof(RenderVertex3D, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertex3D), (void*)offsetof(RenderVertex3D, uv));

        glBindVertexArray(0);
        return true;
    }

    bool ModelAsset::Free()
    {
        FreePrimitives();
        m_nodes.clear();
        m_sceneRoots.clear();
        m_skins.clear();
        m_animations.clear();
        m_materialSlotNames.clear();
        m_bindTranslations.clear();
        m_bindRotations.clear();
        m_bindScales.clear();
        m_bindLocalMatrices.clear();
        m_sharedPose.localNodeMatrices.clear();
        m_sharedPose.globalNodeMatrices.clear();
        m_sharedPose.skinnedVertices.clear();
        m_sharedPose.skinMatricesScratch.clear();
        m_sharedPose.translationsScratch.clear();
        m_sharedPose.rotationsScratch.clear();
        m_sharedPose.scalesScratch.clear();
        m_sharedPose.trsChangedScratch.clear();
        m_sharedPose.visitedScratch.clear();
        m_geometryRevision = 0u;

        return true;
    }

    bool ModelAsset::UpdateAnimation(i32 _clipIndex, float _timeSeconds)
    {
        return UpdateAnimation(m_sharedPose, _clipIndex, _timeSeconds);
    }

    bool ModelAsset::UpdateAnimation(Pose3D &_pose, i32 _clipIndex, float _timeSeconds) const
    {
        if (_clipIndex < 0 || _clipIndex >= (i32)m_animations.size())
            return false;

        if (m_nodes.empty())
            return false;

        EnsurePose(_pose);

        const AnimationClip3D &clip = m_animations[_clipIndex];
        std::vector<Vector3> &translations = _pose.translationsScratch;
        std::vector<Vector4> &rotations = _pose.rotationsScratch;
        std::vector<Vector3> &scales = _pose.scalesScratch;
        std::vector<bool> &trsChanged = _pose.trsChangedScratch;

        if (translations.size() != m_bindTranslations.size())
            translations.resize(m_bindTranslations.size());
        if (rotations.size() != m_bindRotations.size())
            rotations.resize(m_bindRotations.size());
        if (scales.size() != m_bindScales.size())
            scales.resize(m_bindScales.size());
        if (trsChanged.size() != m_nodes.size())
            trsChanged.resize(m_nodes.size(), false);
        else
            std::fill(trsChanged.begin(), trsChanged.end(), false);

        std::copy(m_bindTranslations.begin(), m_bindTranslations.end(), translations.begin());
        std::copy(m_bindRotations.begin(), m_bindRotations.end(), rotations.begin());
        std::copy(m_bindScales.begin(), m_bindScales.end(), scales.begin());

        for (const AnimationChannel3D &channel : clip.channels)
        {
            if (channel.targetNode < 0 || channel.targetNode >= (i32)m_nodes.size())
                continue;

            if (channel.sampler < 0 || channel.sampler >= (i32)clip.samplers.size())
                continue;

            const AnimationSampler3D &sampler = clip.samplers[channel.sampler];
            const bool isRotation = (channel.path == AnimationPath3D::ROTATION);
            const Vector4 sampled = EvaluateAnimationSampler(sampler, _timeSeconds, isRotation);

            switch (channel.path)
            {
                case AnimationPath3D::TRANSLATION:
                    translations[channel.targetNode] = Vector3(sampled.x, sampled.y, sampled.z);
                    trsChanged[channel.targetNode] = true;
                    break;
                case AnimationPath3D::ROTATION:
                {
                    const glm::quat q = glm::normalize(glm::quat(sampled.w, sampled.x, sampled.y, sampled.z));
                    rotations[channel.targetNode] = Vector4(q.x, q.y, q.z, q.w);
                    trsChanged[channel.targetNode] = true;
                    break;
                }
                case AnimationPath3D::SCALE:
                    scales[channel.targetNode] = Vector3(sampled.x, sampled.y, sampled.z);
                    trsChanged[channel.targetNode] = true;
                    break;
            }
        }

        for (size_t nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex)
        {
            if (m_nodes[nodeIndex].hasMatrix && !trsChanged[nodeIndex])
            {
                _pose.localNodeMatrices[nodeIndex] = m_bindLocalMatrices[nodeIndex];
            }
            else
            {
                const glm::quat q = glm::normalize(glm::quat(
                    rotations[nodeIndex].w,
                    rotations[nodeIndex].x,
                    rotations[nodeIndex].y,
                    rotations[nodeIndex].z));
                _pose.localNodeMatrices[nodeIndex] = glm::translate(Matrix4(1.0f), translations[nodeIndex]) *
                    glm::mat4_cast(q) *
                    glm::scale(Matrix4(1.0f), scales[nodeIndex]);
            }
        }

        UpdateGlobalMatrices(_pose);
        UpdateSkinning(_pose);
        return true;
    }

    void ModelAsset::ResetPose()
    {
        ResetPose(m_sharedPose);
    }

    void ModelAsset::ResetPose(Pose3D &_pose) const
    {
        if (m_nodes.empty())
            return;

        EnsurePose(_pose);

        for (size_t nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex)
        {
            if (m_nodes[nodeIndex].hasMatrix)
                _pose.localNodeMatrices[nodeIndex] = m_bindLocalMatrices[nodeIndex];
            else
            {
                const glm::quat q = glm::normalize(glm::quat(
                    m_bindRotations[nodeIndex].w,
                    m_bindRotations[nodeIndex].x,
                    m_bindRotations[nodeIndex].y,
                    m_bindRotations[nodeIndex].z));
                _pose.localNodeMatrices[nodeIndex] = glm::translate(Matrix4(1.0f), m_bindTranslations[nodeIndex]) *
                    glm::mat4_cast(q) *
                    glm::scale(Matrix4(1.0f), m_bindScales[nodeIndex]);
            }
        }

        UpdateGlobalMatrices(_pose);
        UpdateSkinning(_pose);
    }

    void ModelAsset::Draw(
        Shader &_shader,
        const Matrix4 &_modelMatrix,
        const Pose3D *_pose,
        i32 _overrideTextureId,
        const Color &_baseColor,
        const std::vector<MaterialAsset*> *_slotMaterialOverrides,
        i32 _nodeIndex,
        bool _applyNodeTransform)
    {
        const Pose3D *pose = (_pose == nullptr) ? &m_sharedPose : _pose;
        const bool applyPrimitiveNodeTransform = (_nodeIndex < 0) ? true : _applyNodeTransform;

        for (size_t primitiveIndex = 0; primitiveIndex < m_primitives.size(); ++primitiveIndex)
        {
            Primitive3D &primitive = m_primitives[primitiveIndex];
            if (!PrimitiveMatchesNodeFilter(primitive, _nodeIndex))
                continue;

            Matrix4 model = _modelMatrix;
            if (applyPrimitiveNodeTransform && primitive.nodeIndex >= 0 && primitive.nodeIndex < (i32)m_nodes.size())
            {
                if (pose != nullptr && pose->globalNodeMatrices.size() == m_nodes.size())
                    model = _modelMatrix * pose->globalNodeMatrices[primitive.nodeIndex];
                else
                    model = _modelMatrix * m_nodes[primitive.nodeIndex].globalMatrix;
            }

            MaterialAsset *slotMaterial = nullptr;
            if (_slotMaterialOverrides != nullptr && primitive.materialSlot >= 0 &&
                primitive.materialSlot < static_cast<i32>(_slotMaterialOverrides->size()))
            {
                slotMaterial = (*_slotMaterialOverrides)[primitive.materialSlot];
            }

            Color drawColor = _baseColor;
            i32 textureId = primitive.textureId;
            if (slotMaterial != nullptr)
            {
                if ((slotMaterial->info & MATERIAL_HAS_COLOR) != 0u)
                    drawColor *= slotMaterial->color;

                if (slotMaterial->albedoId >= 0)
                    textureId = slotMaterial->albedoId;
                else if (_overrideTextureId >= 0)
                    textureId = _overrideTextureId;
            }
            else if (_overrideTextureId >= 0)
            {
                textureId = _overrideTextureId;
            }

            _shader.SetBool("useInstanceMatrix", false);
            _shader.SetMat4("M", model);
            _shader.SetBool("useAlbedoMap", textureId >= 0);
            _shader.SetInt("albedoMap", 0);
            _shader.SetVec4("albedoValue", drawColor);

            glActiveTexture(GL_TEXTURE0);
            if (textureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(textureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            if (primitive.hasSkinning && !primitive.bindVertices.empty())
            {
                const std::vector<RenderVertex3D> *vertices = &primitive.bindVertices;
                if (pose != nullptr &&
                    primitiveIndex < pose->skinnedVertices.size() &&
                    pose->skinnedVertices[primitiveIndex].size() == primitive.bindVertices.size())
                {
                    vertices = &pose->skinnedVertices[primitiveIndex];
                }

                glBindBuffer(GL_ARRAY_BUFFER, primitive.vbo);
                glBufferSubData(
                    GL_ARRAY_BUFFER,
                    0,
                    vertices->size() * sizeof(RenderVertex3D),
                    vertices->data());
            }

            glBindVertexArray(primitive.vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(primitive.indices.size()), GL_UNSIGNED_INT, nullptr);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void ModelAsset::DrawInstanced(
        Shader &_shader,
        const std::vector<Matrix4> &_modelMatrices,
        i32 _overrideTextureId,
        const Color &_baseColor,
        i32 _nodeIndex,
        bool _applyNodeTransform)
    {
        if (_modelMatrices.empty())
            return;

        const bool applyPrimitiveNodeTransform = (_nodeIndex < 0) ? true : _applyNodeTransform;
        std::vector<Matrix4> primitiveMatrices = {};
        primitiveMatrices.reserve(_modelMatrices.size());

        for (Primitive3D &primitive : m_primitives)
        {
            if (!PrimitiveMatchesNodeFilter(primitive, _nodeIndex) || primitive.hasSkinning)
                continue;

            primitiveMatrices.clear();
            for (const Matrix4 &modelMatrix : _modelMatrices)
            {
                Matrix4 model = modelMatrix;
                if (applyPrimitiveNodeTransform && primitive.nodeIndex >= 0 && primitive.nodeIndex < (i32)m_nodes.size())
                    model = modelMatrix * m_nodes[primitive.nodeIndex].globalMatrix;

                primitiveMatrices.push_back(model);
            }

            i32 textureId = (_overrideTextureId >= 0) ? _overrideTextureId : primitive.textureId;
            _shader.SetBool("useInstanceMatrix", true);
            _shader.SetBool("useAlbedoMap", textureId >= 0);
            _shader.SetInt("albedoMap", 0);
            _shader.SetVec4("albedoValue", _baseColor);

            glActiveTexture(GL_TEXTURE0);
            if (textureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(textureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            if (primitive.instanceVbo == 0)
                glGenBuffers(1, &primitive.instanceVbo);

            glBindVertexArray(primitive.vao);
            glBindBuffer(GL_ARRAY_BUFFER, primitive.instanceVbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                primitiveMatrices.size() * sizeof(Matrix4),
                primitiveMatrices.data(),
                GL_DYNAMIC_DRAW);

            const GLsizei matrixStride = static_cast<GLsizei>(sizeof(Matrix4));
            for (unsigned int column = 0; column < 4; ++column)
            {
                const unsigned int attribute = 3u + column;
                glEnableVertexAttribArray(attribute);
                glVertexAttribPointer(
                    attribute,
                    4,
                    GL_FLOAT,
                    GL_FALSE,
                    matrixStride,
                    reinterpret_cast<void*>(sizeof(float) * 4u * column));
                glVertexAttribDivisor(attribute, 1);
            }

            glDrawElementsInstanced(
                GL_TRIANGLES,
                static_cast<GLsizei>(primitive.indices.size()),
                GL_UNSIGNED_INT,
                nullptr,
                static_cast<GLsizei>(primitiveMatrices.size()));
        }

        _shader.SetBool("useInstanceMatrix", false);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    const ModelAsset::Node3D* ModelAsset::GetNode(i32 _index) const
    {
        if (_index < 0 || _index >= static_cast<i32>(m_nodes.size()))
            return nullptr;

        return &m_nodes[_index];
    }

    std::string ModelAsset::GetNodeName(i32 _index) const
    {
        if (const Node3D *node = GetNode(_index))
            return node->name;

        return "";
    }

    bool ModelAsset::NodeHasPrimitives(i32 _index) const
    {
        if (_index < 0)
            return !m_primitives.empty();

        for (const Primitive3D &primitive : m_primitives)
        {
            if (primitive.nodeIndex == _index)
                return true;
        }

        return false;
    }

    std::string ModelAsset::GetMaterialSlotName(i32 _index) const
    {
        if (_index < 0 || _index >= static_cast<i32>(m_materialSlotNames.size()))
            return "";

        return m_materialSlotNames[_index];
    }

    std::string ModelAsset::GetAnimationName(i32 _index) const
    {
        if (_index < 0 || _index >= (i32)m_animations.size())
            return "";

        if (m_animations[_index].name.empty())
            return "Animation " + std::to_string(_index);

        return m_animations[_index].name;
    }

    float ModelAsset::GetAnimationDuration(i32 _index) const
    {
        if (_index < 0 || _index >= (i32)m_animations.size())
            return 0.0f;

        return m_animations[_index].duration;
    }

    void ModelAsset::EnsurePose(Pose3D &_pose) const
    {
        if (_pose.localNodeMatrices.size() != m_nodes.size())
            _pose.localNodeMatrices.resize(m_nodes.size(), Matrix4(1.0f));

        if (_pose.globalNodeMatrices.size() != m_nodes.size())
            _pose.globalNodeMatrices.resize(m_nodes.size(), Matrix4(1.0f));

        if (_pose.skinnedVertices.size() != m_primitives.size())
            _pose.skinnedVertices.resize(m_primitives.size());

        if (_pose.skinMatricesScratch.size() != m_primitives.size())
            _pose.skinMatricesScratch.resize(m_primitives.size());

        for (size_t primitiveIndex = 0; primitiveIndex < m_primitives.size(); ++primitiveIndex)
        {
            const Primitive3D &primitive = m_primitives[primitiveIndex];
            if (!primitive.hasSkinning)
            {
                _pose.skinnedVertices[primitiveIndex].clear();
                _pose.skinMatricesScratch[primitiveIndex].clear();
                continue;
            }

            std::vector<RenderVertex3D> &poseVertices = _pose.skinnedVertices[primitiveIndex];
            if (poseVertices.size() != primitive.bindVertices.size())
                poseVertices = primitive.bindVertices;
        }
    }

    void ModelAsset::VisitNodeRecursive(i32 _nodeIndex, const Matrix4 &_parentMatrix, Pose3D &_pose, std::vector<bool> &_visited) const
    {
        if (_nodeIndex < 0 || _nodeIndex >= (i32)m_nodes.size())
            return;

        const Node3D &node = m_nodes[_nodeIndex];
        _pose.globalNodeMatrices[_nodeIndex] = _parentMatrix * _pose.localNodeMatrices[_nodeIndex];
        _visited[_nodeIndex] = true;

        for (i32 child : node.children)
            VisitNodeRecursive(child, _pose.globalNodeMatrices[_nodeIndex], _pose, _visited);
    }

    void ModelAsset::UpdateGlobalMatrices(Pose3D &_pose) const
    {
        if (m_nodes.empty())
            return;

        EnsurePose(_pose);

        std::vector<bool> &visited = _pose.visitedScratch;
        if (visited.size() != m_nodes.size())
            visited.resize(m_nodes.size(), false);
        else
            std::fill(visited.begin(), visited.end(), false);

        Matrix4 identity = Matrix4(1.0f);

        for (i32 root : m_sceneRoots)
            VisitNodeRecursive(root, identity, _pose, visited);

        for (size_t nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex)
        {
            if (!visited[nodeIndex])
                VisitNodeRecursive(static_cast<i32>(nodeIndex), identity, _pose, visited);
        }
    }

    void ModelAsset::UpdateSkinning(Pose3D &_pose) const
    {
        EnsurePose(_pose);

        for (size_t primitiveIndex = 0; primitiveIndex < m_primitives.size(); ++primitiveIndex)
        {
            const Primitive3D &primitive = m_primitives[primitiveIndex];
            if (!primitive.hasSkinning ||
                primitive.skinVertices.size() != primitive.bindVertices.size() ||
                primitive.skinIndex < 0 ||
                primitive.skinIndex >= (i32)m_skins.size())
                continue;

            const Skin3D &skin = m_skins[primitive.skinIndex];
            if (skin.joints.empty())
                continue;

            std::vector<RenderVertex3D> &poseVertices = _pose.skinnedVertices[primitiveIndex];
            if (poseVertices.size() != primitive.bindVertices.size())
                poseVertices = primitive.bindVertices;

            std::vector<Matrix4> &skinMatrices = _pose.skinMatricesScratch[primitiveIndex];
            if (skinMatrices.size() != skin.joints.size())
                skinMatrices.resize(skin.joints.size(), Matrix4(1.0f));
            else
                std::fill(skinMatrices.begin(), skinMatrices.end(), Matrix4(1.0f));

            for (size_t jointIndex = 0; jointIndex < skin.joints.size(); ++jointIndex)
            {
                const i32 nodeIndex = skin.joints[jointIndex];
                if (nodeIndex < 0 || nodeIndex >= (i32)m_nodes.size())
                    continue;

                if (jointIndex >= skin.inverseBindMatrices.size())
                    continue;

                skinMatrices[jointIndex] = _pose.globalNodeMatrices[nodeIndex] * skin.inverseBindMatrices[jointIndex];
            }

            for (size_t vertexIndex = 0; vertexIndex < primitive.bindVertices.size(); ++vertexIndex)
            {
                const RenderVertex3D &bindVertex = primitive.bindVertices[vertexIndex];
                const SkinVertex3D &skinVertex = primitive.skinVertices[vertexIndex];
                RenderVertex3D &skinnedVertex = poseVertices[vertexIndex];
                skinnedVertex = bindVertex;

                const float weights[4] = {skinVertex.weights.x, skinVertex.weights.y, skinVertex.weights.z, skinVertex.weights.w};
                const int joints[4] = {
                    static_cast<int>(skinVertex.joints.x),
                    static_cast<int>(skinVertex.joints.y),
                    static_cast<int>(skinVertex.joints.z),
                    static_cast<int>(skinVertex.joints.w),
                };

                float totalWeight = weights[0] + weights[1] + weights[2] + weights[3];
                if (totalWeight <= 1e-6f)
                    continue;

                const float weightScale = 1.0f / totalWeight;
                Vector4 position(0.0f);
                Vector4 normal(0.0f);
                for (size_t i = 0; i < 4; ++i)
                {
                    const int jointIndex = joints[i];
                    if (jointIndex < 0 || jointIndex >= (int)skinMatrices.size())
                        continue;

                    const float weight = weights[i] * weightScale;
                    if (weight <= 0.0f)
                        continue;

                    const Matrix4 &jointMatrix = skinMatrices[jointIndex];
                    const Vector4 weightedPosition = jointMatrix * Vector4(
                        bindVertex.position.x,
                        bindVertex.position.y,
                        bindVertex.position.z,
                        1.0f);
                    const Vector4 weightedNormal = jointMatrix * Vector4(
                        bindVertex.normal.x,
                        bindVertex.normal.y,
                        bindVertex.normal.z,
                        0.0f);

                    position += weightedPosition * weight;
                    normal += weightedNormal * weight;
                }

                skinnedVertex.position = Vector3(position.x, position.y, position.z);

                const Vector3 normal3(normal.x, normal.y, normal.z);
                const float normalLength = glm::length(normal3);
                if (normalLength > 0.0f)
                    skinnedVertex.normal = normal3 / normalLength;
                else
                    skinnedVertex.normal = Vector3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    bool ModelAsset::BuildTriangleMesh(std::vector<Vector3> &_vertices, std::vector<u32> &_indices, i32 _nodeIndex, bool _applyNodeTransform) const
    {
        _vertices.clear();
        _indices.clear();

        size_t totalVertexCount = 0;
        size_t totalIndexCount = 0;
        for (const Primitive3D &primitive : m_primitives)
        {
            if (!PrimitiveMatchesNodeFilter(primitive, _nodeIndex))
                continue;

            totalVertexCount += primitive.bindVertices.size();
            totalIndexCount += primitive.indices.size();
        }

        if (totalVertexCount == 0 || totalIndexCount == 0)
            return false;

        _vertices.reserve(totalVertexCount);
        _indices.reserve(totalIndexCount);

        for (const Primitive3D &primitive : m_primitives)
        {
            if (!PrimitiveMatchesNodeFilter(primitive, _nodeIndex))
                continue;

            if (primitive.bindVertices.empty() || primitive.indices.empty())
                continue;

            const u32 vertexOffset = static_cast<u32>(_vertices.size());
            Matrix4 nodeMatrix = Matrix4(1.0f);
            if ((_nodeIndex < 0 || _applyNodeTransform) &&
                primitive.nodeIndex >= 0 &&
                primitive.nodeIndex < static_cast<i32>(m_nodes.size()))
            {
                nodeMatrix = m_nodes[primitive.nodeIndex].globalMatrix;
            }

            for (const RenderVertex3D &vertex : primitive.bindVertices)
            {
                const Vector4 transformed = nodeMatrix * Vector4(vertex.position, 1.0f);
                _vertices.push_back(Vector3(transformed.x, transformed.y, transformed.z));
            }

            for (unsigned int index : primitive.indices)
                _indices.push_back(vertexOffset + static_cast<u32>(index));
        }

        return !_vertices.empty() && !_indices.empty();
    }

    bool ModelAsset::GetLocalBounds(Vector3 &_min, Vector3 &_max, i32 _nodeIndex, bool _applyNodeTransform) const
    {
        bool hasBounds = false;
        Vector3 minBounds(0.0f);
        Vector3 maxBounds(0.0f);

        for (const Primitive3D &primitive : m_primitives)
        {
            if (!PrimitiveMatchesNodeFilter(primitive, _nodeIndex))
                continue;

            Matrix4 nodeMatrix = Matrix4(1.0f);
            if ((_nodeIndex < 0 || _applyNodeTransform) &&
                primitive.nodeIndex >= 0 &&
                primitive.nodeIndex < static_cast<i32>(m_nodes.size()))
            {
                nodeMatrix = m_nodes[primitive.nodeIndex].globalMatrix;
            }

            for (const RenderVertex3D &vertex : primitive.bindVertices)
            {
                const Vector4 transformed = nodeMatrix * Vector4(vertex.position, 1.0f);
                const Vector3 point(transformed.x, transformed.y, transformed.z);

                if (!hasBounds)
                {
                    minBounds = point;
                    maxBounds = point;
                    hasBounds = true;
                    continue;
                }

                minBounds = glm::min(minBounds, point);
                maxBounds = glm::max(maxBounds, point);
            }
        }

        if (!hasBounds)
            return false;

        _min = minBounds;
        _max = maxBounds;
        return true;
    }

    bool SpriteAnimationAsset::Load(std::string _path)
    {
        return true;
    }

    bool SpriteAnimationAsset::Free()
    {
        return true;
    }

    namespace
    {
        const char *AnimationValueTypeToString(AnimationValueType _type)
        {
            switch (_type)
            {
                case AnimationValueType::FLOAT: return "float";
                case AnimationValueType::INT: return "int";
                case AnimationValueType::BOOL: return "bool";
                case AnimationValueType::VEC2: return "vec2";
                case AnimationValueType::VEC3: return "vec3";
                case AnimationValueType::VEC4: return "vec4";
                default: return "none";
            }
        }

        AnimationValueType AnimationValueTypeFromString(const std::string &_type)
        {
            if (_type == "float") return AnimationValueType::FLOAT;
            if (_type == "int" || _type == "integer") return AnimationValueType::INT;
            if (_type == "bool" || _type == "boolean") return AnimationValueType::BOOL;
            if (_type == "vec2" || _type == "vector2") return AnimationValueType::VEC2;
            if (_type == "vec3" || _type == "vector3") return AnimationValueType::VEC3;
            if (_type == "vec4" || _type == "vector4" || _type == "color") return AnimationValueType::VEC4;
            return AnimationValueType::NONE;
        }

        const char *AnimationInterpolationToString(AnimationInterpolation _interpolation)
        {
            switch (_interpolation)
            {
                case AnimationInterpolation::STEP: return "step";
                case AnimationInterpolation::LINEAR:
                default:
                    return "linear";
            }
        }

        AnimationInterpolation AnimationInterpolationFromString(const std::string &_value)
        {
            return _value == "step" ? AnimationInterpolation::STEP : AnimationInterpolation::LINEAR;
        }

        const char *AnimatorParameterTypeToString(AnimatorParameterType _type)
        {
            switch (_type)
            {
                case AnimatorParameterType::INT: return "int";
                case AnimatorParameterType::BOOL: return "bool";
                case AnimatorParameterType::TRIGGER: return "trigger";
                case AnimatorParameterType::FLOAT:
                default:
                    return "float";
            }
        }

        AnimatorParameterType AnimatorParameterTypeFromString(const std::string &_type)
        {
            if (_type == "int" || _type == "integer") return AnimatorParameterType::INT;
            if (_type == "bool" || _type == "boolean") return AnimatorParameterType::BOOL;
            if (_type == "trigger") return AnimatorParameterType::TRIGGER;
            return AnimatorParameterType::FLOAT;
        }

        AnimationValueType AnimatorParameterValueType(AnimatorParameterType _type)
        {
            switch (_type)
            {
                case AnimatorParameterType::INT: return AnimationValueType::INT;
                case AnimatorParameterType::BOOL:
                case AnimatorParameterType::TRIGGER:
                    return AnimationValueType::BOOL;
                case AnimatorParameterType::FLOAT:
                default:
                    return AnimationValueType::FLOAT;
            }
        }

        const char *AnimatorConditionModeToString(AnimatorConditionMode _mode)
        {
            switch (_mode)
            {
                case AnimatorConditionMode::LESS: return "less";
                case AnimatorConditionMode::EQUAL: return "equal";
                case AnimatorConditionMode::NOT_EQUAL: return "not_equal";
                case AnimatorConditionMode::IF_TRUE: return "if_true";
                case AnimatorConditionMode::IF_FALSE: return "if_false";
                case AnimatorConditionMode::TRIGGERED: return "triggered";
                case AnimatorConditionMode::GREATER:
                default:
                    return "greater";
            }
        }

        AnimatorConditionMode AnimatorConditionModeFromString(const std::string &_mode)
        {
            if (_mode == "less") return AnimatorConditionMode::LESS;
            if (_mode == "equal") return AnimatorConditionMode::EQUAL;
            if (_mode == "not_equal") return AnimatorConditionMode::NOT_EQUAL;
            if (_mode == "if_true") return AnimatorConditionMode::IF_TRUE;
            if (_mode == "if_false") return AnimatorConditionMode::IF_FALSE;
            if (_mode == "triggered") return AnimatorConditionMode::TRIGGERED;
            return AnimatorConditionMode::GREATER;
        }

        YAML::Node EncodeAnimationValue(const AnimationValue &_value)
        {
            switch (_value.type)
            {
                case AnimationValueType::FLOAT: return YAML::Node(_value.AsFloat());
                case AnimationValueType::INT: return YAML::Node(_value.AsInt());
                case AnimationValueType::BOOL: return YAML::Node(_value.AsBool());
                case AnimationValueType::VEC2: return YAML::Node(_value.AsVec2());
                case AnimationValueType::VEC3: return YAML::Node(_value.AsVec3());
                case AnimationValueType::VEC4: return YAML::Node(_value.AsVec4());
                case AnimationValueType::NONE:
                default:
                    return YAML::Node();
            }
        }

        AnimationValue DecodeAnimationValue(AnimationValueType _type, const YAML::Node &_node)
        {
            switch (_type)
            {
                case AnimationValueType::FLOAT: return AnimationValue::Float(_node.as<float>(0.0f));
                case AnimationValueType::INT: return AnimationValue::Int(_node.as<int>(0));
                case AnimationValueType::BOOL: return AnimationValue::Bool(_node.as<bool>(false));
                case AnimationValueType::VEC2: return AnimationValue::Vec2(_node.as<Vector2>(Vector2(0.0f)));
                case AnimationValueType::VEC3: return AnimationValue::Vec3(_node.as<Vector3>(Vector3(0.0f)));
                case AnimationValueType::VEC4: return AnimationValue::Vec4(_node.as<Vector4>(Vector4(0.0f)));
                case AnimationValueType::NONE:
                default:
                    return {};
            }
        }
    }

    bool AnimationClipAsset::Load(std::string _path)
    {
        Free();

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(_path);
        }
        catch (const YAML::Exception &)
        {
            return false;
        }

        m_path = _path;
        length = root["length"].as<float>(1.0f);
        tracks.clear();
        events.clear();

        if (YAML::Node tracksNode = root["tracks"]; tracksNode && tracksNode.IsSequence())
        {
            for (const YAML::Node &trackNode : tracksNode)
            {
                AnimationTrack track = {};
                track.path = trackNode["path"].as<std::string>("");
                track.component = trackNode["component"].as<std::string>("");
                track.property = trackNode["property"].as<std::string>("");
                track.type = AnimationValueTypeFromString(trackNode["type"].as<std::string>("none"));
                track.interpolation = AnimationInterpolationFromString(trackNode["interpolation"].as<std::string>("linear"));

                if (YAML::Node keysNode = trackNode["keys"]; keysNode && keysNode.IsSequence())
                {
                    for (const YAML::Node &keyNode : keysNode)
                    {
                        AnimationKeyframe keyframe = {};
                        keyframe.time = keyNode["time"].as<float>(0.0f);
                        keyframe.value = DecodeAnimationValue(track.type, keyNode["value"]);
                        track.keys.push_back(keyframe);
                    }
                }

                std::sort(track.keys.begin(), track.keys.end(), [](const AnimationKeyframe &_left, const AnimationKeyframe &_right)
                {
                    return _left.time < _right.time;
                });

                tracks.push_back(track);
            }
        }

        if (YAML::Node eventsNode = root["events"]; eventsNode && eventsNode.IsSequence())
        {
            for (const YAML::Node &eventNode : eventsNode)
            {
                AnimationEvent event = {};
                event.time = eventNode["time"].as<float>(0.0f);
                event.path = eventNode["path"].as<std::string>("");
                event.script = eventNode["script"].as<std::string>("");
                event.name = eventNode["name"].as<std::string>("");
                event.stringPayload = eventNode["stringPayload"].as<std::string>("");
                event.floatPayload = eventNode["floatPayload"].as<float>(0.0f);
                event.intPayload = eventNode["intPayload"].as<int>(0);
                events.push_back(event);
            }
        }

        std::sort(events.begin(), events.end(), [](const AnimationEvent &_left, const AnimationEvent &_right)
        {
            return _left.time < _right.time;
        });

        return true;
    }

    bool AnimationClipAsset::Free()
    {
        m_path.clear();
        length = 1.0f;
        tracks.clear();
        events.clear();
        return true;
    }

    bool AnimationClipAsset::Save(const std::string &_path) const
    {
        const std::string targetPath = _path.empty() ? m_path : _path;
        if (targetPath.empty())
            return false;

        YAML::Node root(YAML::NodeType::Map);
        root["length"] = length;

        YAML::Node tracksNode(YAML::NodeType::Sequence);
        for (const AnimationTrack &track : tracks)
        {
            YAML::Node trackNode(YAML::NodeType::Map);
            trackNode["path"] = track.path;
            trackNode["component"] = track.component;
            trackNode["property"] = track.property;
            trackNode["type"] = AnimationValueTypeToString(track.type);
            trackNode["interpolation"] = AnimationInterpolationToString(track.interpolation);

            YAML::Node keysNode(YAML::NodeType::Sequence);
            for (const AnimationKeyframe &key : track.keys)
            {
                YAML::Node keyNode(YAML::NodeType::Map);
                keyNode["time"] = key.time;
                keyNode["value"] = EncodeAnimationValue(key.value);
                keysNode.push_back(keyNode);
            }

            trackNode["keys"] = keysNode;
            tracksNode.push_back(trackNode);
        }

        root["tracks"] = tracksNode;

        YAML::Node eventsNode(YAML::NodeType::Sequence);
        for (const AnimationEvent &event : events)
        {
            YAML::Node eventNode(YAML::NodeType::Map);
            eventNode["time"] = event.time;
            eventNode["path"] = event.path;
            eventNode["script"] = event.script;
            eventNode["name"] = event.name;
            eventNode["stringPayload"] = event.stringPayload;
            eventNode["floatPayload"] = event.floatPayload;
            eventNode["intPayload"] = event.intPayload;
            eventsNode.push_back(eventNode);
        }

        root["events"] = eventsNode;

        std::ofstream out(targetPath);
        if (!out.is_open())
            return false;

        out << root;
        return true;
    }

    bool AnimatorControllerAsset::Load(std::string _path)
    {
        Free();

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(_path);
        }
        catch (const YAML::Exception &)
        {
            return false;
        }

        m_path = _path;
        entryState = root["entryState"].as<std::string>("");
        parameters.clear();
        states.clear();

        if (YAML::Node parametersNode = root["parameters"]; parametersNode && parametersNode.IsSequence())
        {
            for (const YAML::Node &parameterNode : parametersNode)
            {
                AnimatorParameterDefinition parameter = {};
                parameter.name = parameterNode["name"].as<std::string>("");
                parameter.type = AnimatorParameterTypeFromString(parameterNode["type"].as<std::string>("float"));
                parameter.defaultValue = DecodeAnimationValue(
                    AnimatorParameterValueType(parameter.type),
                    parameterNode["defaultValue"]);
                parameters.push_back(parameter);
            }
        }

        if (YAML::Node statesNode = root["states"]; statesNode && statesNode.IsSequence())
        {
            for (const YAML::Node &stateNode : statesNode)
            {
                AnimatorState state = {};
                state.name = stateNode["name"].as<std::string>(state.name);
                state.clip = stateNode["clip"].as<AnimationClipAssetHandle>(state.clip);
                state.loop = stateNode["loop"].as<bool>(true);
                state.speed = stateNode["speed"].as<float>(1.0f);
                if (YAML::Node editorPositionNode = stateNode["editorPosition"])
                    state.editorPosition = editorPositionNode.as<Vector2>(state.editorPosition);

                if (YAML::Node transitionsNode = stateNode["transitions"]; transitionsNode && transitionsNode.IsSequence())
                {
                    for (const YAML::Node &transitionNode : transitionsNode)
                    {
                        AnimatorTransition transition = {};
                        transition.toState = transitionNode["toState"].as<std::string>("");
                        transition.hasExitTime = transitionNode["hasExitTime"].as<bool>(false);
                        transition.exitTimeNormalized = transitionNode["exitTimeNormalized"].as<float>(1.0f);

                        if (YAML::Node conditionsNode = transitionNode["conditions"]; conditionsNode && conditionsNode.IsSequence())
                        {
                            for (const YAML::Node &conditionNode : conditionsNode)
                            {
                                AnimatorTransitionCondition condition = {};
                                condition.parameter = conditionNode["parameter"].as<std::string>("");
                                condition.mode = AnimatorConditionModeFromString(conditionNode["mode"].as<std::string>("greater"));

                                AnimatorParameterType parameterType = AnimatorParameterType::FLOAT;
                                for (const AnimatorParameterDefinition &parameter : parameters)
                                {
                                    if (parameter.name == condition.parameter)
                                    {
                                        parameterType = parameter.type;
                                        break;
                                    }
                                }

                                condition.value = DecodeAnimationValue(
                                    AnimatorParameterValueType(parameterType),
                                    conditionNode["value"]);
                                transition.conditions.push_back(condition);
                            }
                        }

                        state.transitions.push_back(transition);
                    }
                }

                states.push_back(state);
            }
        }

        if (entryState.empty() && !states.empty())
            entryState = states.front().name;

        return true;
    }

    bool AnimatorControllerAsset::Free()
    {
        m_path.clear();
        entryState.clear();
        parameters.clear();
        states.clear();
        return true;
    }

    bool AnimatorControllerAsset::Save(const std::string &_path) const
    {
        const std::string targetPath = _path.empty() ? m_path : _path;
        if (targetPath.empty())
            return false;

        YAML::Node root(YAML::NodeType::Map);
        root["entryState"] = entryState;

        YAML::Node parametersNode(YAML::NodeType::Sequence);
        for (const AnimatorParameterDefinition &parameter : parameters)
        {
            YAML::Node parameterNode(YAML::NodeType::Map);
            parameterNode["name"] = parameter.name;
            parameterNode["type"] = AnimatorParameterTypeToString(parameter.type);
            parameterNode["defaultValue"] = EncodeAnimationValue(parameter.defaultValue);
            parametersNode.push_back(parameterNode);
        }
        root["parameters"] = parametersNode;

        YAML::Node statesNode(YAML::NodeType::Sequence);
        for (const AnimatorState &state : states)
        {
            YAML::Node stateNode(YAML::NodeType::Map);
            stateNode["name"] = state.name;
            stateNode["clip"] = state.clip;
            stateNode["loop"] = state.loop;
            stateNode["speed"] = state.speed;
            stateNode["editorPosition"] = state.editorPosition;

            YAML::Node transitionsNode(YAML::NodeType::Sequence);
            for (const AnimatorTransition &transition : state.transitions)
            {
                YAML::Node transitionNode(YAML::NodeType::Map);
                transitionNode["toState"] = transition.toState;
                transitionNode["hasExitTime"] = transition.hasExitTime;
                transitionNode["exitTimeNormalized"] = transition.exitTimeNormalized;

                YAML::Node conditionsNode(YAML::NodeType::Sequence);
                for (const AnimatorTransitionCondition &condition : transition.conditions)
                {
                    YAML::Node conditionNode(YAML::NodeType::Map);
                    conditionNode["parameter"] = condition.parameter;
                    conditionNode["mode"] = AnimatorConditionModeToString(condition.mode);
                    conditionNode["value"] = EncodeAnimationValue(condition.value);
                    conditionsNode.push_back(conditionNode);
                }

                transitionNode["conditions"] = conditionsNode;
                transitionsNode.push_back(transitionNode);
            }

            stateNode["transitions"] = transitionsNode;
            statesNode.push_back(stateNode);
        }

        root["states"] = statesNode;

        std::ofstream out(targetPath);
        if (!out.is_open())
            return false;

        out << root;
        return true;
    }

    int MaterialFields::Use(Shader &_shader, int _firstTextureUnit) const
    {
        for (const IntUniformData &uniform : m_intUniformData)
            _shader.SetInt(uniform.name, uniform.value);

        for (const FloatUniformData &uniform : m_floatUniformData)
            _shader.SetFloat(uniform.name, uniform.value);

        for (const Vec2UniformData &uniform : m_vec2UniformData)
            _shader.SetVec2(uniform.name, uniform.value);

        for (const Vec3UniformData &uniform : m_vec3UniformData)
            _shader.SetVec3(uniform.name, uniform.value);

        for (const Vec4UniformData &uniform : m_vec4UniformData)
            _shader.SetVec4(uniform.name, uniform.value);

        for (const ColorUniformData &uniform : m_colorUniformData)
            _shader.SetVec4(uniform.name, uniform.value);

        int textureUnit = std::max(_firstTextureUnit, 0);
        for (const TextureUniformData &uniform : m_textureUniformData)
        {
            _shader.SetInt(uniform.name, textureUnit);

            GLuint glTexture = 0;
            if (uniform.textureId >= 0)
            {
                if (TextureAsset *textureAsset = AssetManager::GetTexture(uniform.textureId))
                    glTexture = textureAsset->GetGLTexture().id;
            }

            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D, glTexture);
            textureUnit++;
        }

        glActiveTexture(GL_TEXTURE0);
        return textureUnit;
    }

    void MaterialFields::SetInt(const std::string &_name, int _value)
    {
        for (IntUniformData &uniform : m_intUniformData)
        {
            if (uniform.name == _name)
            {
                uniform.value = _value;
                RemoveFloat(_name);
                RemoveVec2(_name);
                RemoveVec3(_name);
                RemoveVec4(_name);
                RemoveColor(_name);
                RemoveTexture(_name);
                return;
            }
        }

        RemoveFloat(_name);
        RemoveVec2(_name);
        RemoveVec3(_name);
        RemoveVec4(_name);
        RemoveColor(_name);
        RemoveTexture(_name);
        m_intUniformData.push_back(IntUniformData{.name = _name, .value = _value});
    }

    bool MaterialFields::TryGetInt(const std::string &_name, int &_outValue) const
    {
        for (const IntUniformData &uniform : m_intUniformData)
        {
            if (uniform.name == _name)
            {
                _outValue = uniform.value;
                return true;
            }
        }

        return false;
    }

    void MaterialFields::RemoveInt(const std::string &_name)
    {
        m_intUniformData.erase(
            std::remove_if(
                m_intUniformData.begin(),
                m_intUniformData.end(),
                [&](const IntUniformData &_uniform)
                {
                    return _uniform.name == _name;
                }),
            m_intUniformData.end());
    }

    void MaterialFields::SetFloat(const std::string &_name, float _value)
    {
        for (FloatUniformData &uniform : m_floatUniformData)
        {
            if (uniform.name == _name)
            {
                uniform.value = _value;
                RemoveInt(_name);
                RemoveVec2(_name);
                RemoveVec3(_name);
                RemoveVec4(_name);
                RemoveColor(_name);
                RemoveTexture(_name);
                return;
            }
        }

        RemoveInt(_name);
        RemoveVec2(_name);
        RemoveVec3(_name);
        RemoveVec4(_name);
        RemoveColor(_name);
        RemoveTexture(_name);
        m_floatUniformData.push_back(FloatUniformData{.name = _name, .value = _value});
    }

    bool MaterialFields::TryGetFloat(const std::string &_name, float &_outValue) const
    {
        for (const FloatUniformData &uniform : m_floatUniformData)
        {
            if (uniform.name == _name)
            {
                _outValue = uniform.value;
                return true;
            }
        }

        return false;
    }

    void MaterialFields::RemoveFloat(const std::string &_name)
    {
        m_floatUniformData.erase(
            std::remove_if(
                m_floatUniformData.begin(),
                m_floatUniformData.end(),
                [&](const FloatUniformData &_uniform)
                {
                    return _uniform.name == _name;
                }),
            m_floatUniformData.end());
    }

    void MaterialFields::SetVec2(const std::string &_name, const Vector2 &_value)
    {
        for (Vec2UniformData &uniform : m_vec2UniformData)
        {
            if (uniform.name == _name)
            {
                uniform.value = _value;
                RemoveInt(_name);
                RemoveFloat(_name);
                RemoveVec3(_name);
                RemoveVec4(_name);
                RemoveColor(_name);
                RemoveTexture(_name);
                return;
            }
        }

        RemoveInt(_name);
        RemoveFloat(_name);
        RemoveVec3(_name);
        RemoveVec4(_name);
        RemoveColor(_name);
        RemoveTexture(_name);
        m_vec2UniformData.push_back(Vec2UniformData{.name = _name, .value = _value});
    }

    bool MaterialFields::TryGetVec2(const std::string &_name, Vector2 &_outValue) const
    {
        for (const Vec2UniformData &uniform : m_vec2UniformData)
        {
            if (uniform.name == _name)
            {
                _outValue = uniform.value;
                return true;
            }
        }

        return false;
    }

    void MaterialFields::RemoveVec2(const std::string &_name)
    {
        m_vec2UniformData.erase(
            std::remove_if(
                m_vec2UniformData.begin(),
                m_vec2UniformData.end(),
                [&](const Vec2UniformData &_uniform)
                {
                    return _uniform.name == _name;
                }),
            m_vec2UniformData.end());
    }

    void MaterialFields::SetVec3(const std::string &_name, const Vector3 &_value)
    {
        for (Vec3UniformData &uniform : m_vec3UniformData)
        {
            if (uniform.name == _name)
            {
                uniform.value = _value;
                RemoveInt(_name);
                RemoveFloat(_name);
                RemoveVec2(_name);
                RemoveVec4(_name);
                RemoveColor(_name);
                RemoveTexture(_name);
                return;
            }
        }

        RemoveInt(_name);
        RemoveFloat(_name);
        RemoveVec2(_name);
        RemoveVec4(_name);
        RemoveColor(_name);
        RemoveTexture(_name);
        m_vec3UniformData.push_back(Vec3UniformData{.name = _name, .value = _value});
    }

    bool MaterialFields::TryGetVec3(const std::string &_name, Vector3 &_outValue) const
    {
        for (const Vec3UniformData &uniform : m_vec3UniformData)
        {
            if (uniform.name == _name)
            {
                _outValue = uniform.value;
                return true;
            }
        }

        return false;
    }

    void MaterialFields::RemoveVec3(const std::string &_name)
    {
        m_vec3UniformData.erase(
            std::remove_if(
                m_vec3UniformData.begin(),
                m_vec3UniformData.end(),
                [&](const Vec3UniformData &_uniform)
                {
                    return _uniform.name == _name;
                }),
            m_vec3UniformData.end());
    }

    void MaterialFields::SetVec4(const std::string &_name, const Vector4 &_value)
    {
        for (Vec4UniformData &uniform : m_vec4UniformData)
        {
            if (uniform.name == _name)
            {
                uniform.value = _value;
                RemoveInt(_name);
                RemoveFloat(_name);
                RemoveVec2(_name);
                RemoveVec3(_name);
                RemoveColor(_name);
                RemoveTexture(_name);
                return;
            }
        }

        RemoveInt(_name);
        RemoveFloat(_name);
        RemoveVec2(_name);
        RemoveVec3(_name);
        RemoveColor(_name);
        RemoveTexture(_name);
        m_vec4UniformData.push_back(Vec4UniformData{.name = _name, .value = _value});
    }

    bool MaterialFields::TryGetVec4(const std::string &_name, Vector4 &_outValue) const
    {
        for (const Vec4UniformData &uniform : m_vec4UniformData)
        {
            if (uniform.name == _name)
            {
                _outValue = uniform.value;
                return true;
            }
        }

        return false;
    }

    void MaterialFields::RemoveVec4(const std::string &_name)
    {
        m_vec4UniformData.erase(
            std::remove_if(
                m_vec4UniformData.begin(),
                m_vec4UniformData.end(),
                [&](const Vec4UniformData &_uniform)
                {
                    return _uniform.name == _name;
                }),
            m_vec4UniformData.end());
    }

    void MaterialFields::SetColor(const std::string &_name, const Color &_value)
    {
        for (ColorUniformData &uniform : m_colorUniformData)
        {
            if (uniform.name == _name)
            {
                uniform.value = _value;
                RemoveInt(_name);
                RemoveFloat(_name);
                RemoveVec2(_name);
                RemoveVec3(_name);
                RemoveVec4(_name);
                RemoveTexture(_name);
                return;
            }
        }

        RemoveInt(_name);
        RemoveFloat(_name);
        RemoveVec2(_name);
        RemoveVec3(_name);
        RemoveVec4(_name);
        RemoveTexture(_name);
        m_colorUniformData.push_back(ColorUniformData{.name = _name, .value = _value});
    }

    bool MaterialFields::TryGetColor(const std::string &_name, Color &_outValue) const
    {
        for (const ColorUniformData &uniform : m_colorUniformData)
        {
            if (uniform.name == _name)
            {
                _outValue = uniform.value;
                return true;
            }
        }

        return false;
    }

    void MaterialFields::RemoveColor(const std::string &_name)
    {
        m_colorUniformData.erase(
            std::remove_if(
                m_colorUniformData.begin(),
                m_colorUniformData.end(),
                [&](const ColorUniformData &_uniform)
                {
                    return _uniform.name == _name;
                }),
            m_colorUniformData.end());
    }

    void MaterialFields::SetTexture(const std::string &_name, i32 _textureId)
    {
        for (TextureUniformData &uniform : m_textureUniformData)
        {
            if (uniform.name == _name)
            {
                uniform.textureId = _textureId;
                RemoveInt(_name);
                RemoveFloat(_name);
                RemoveVec2(_name);
                RemoveVec3(_name);
                RemoveVec4(_name);
                RemoveColor(_name);
                return;
            }
        }

        RemoveInt(_name);
        RemoveFloat(_name);
        RemoveVec2(_name);
        RemoveVec3(_name);
        RemoveVec4(_name);
        RemoveColor(_name);
        m_textureUniformData.push_back(TextureUniformData{.name = _name, .textureId = _textureId});
    }

    bool MaterialFields::TryGetTexture(const std::string &_name, i32 &_outTextureId) const
    {
        for (const TextureUniformData &uniform : m_textureUniformData)
        {
            if (uniform.name == _name)
            {
                _outTextureId = uniform.textureId;
                return true;
            }
        }

        return false;
    }

    void MaterialFields::RemoveTexture(const std::string &_name)
    {
        m_textureUniformData.erase(
            std::remove_if(
                m_textureUniformData.begin(),
                m_textureUniformData.end(),
                [&](const TextureUniformData &_uniform)
                {
                    return _uniform.name == _name;
                }),
            m_textureUniformData.end());
    }

    void MaterialFields::Clear()
    {
        m_intUniformData.clear();
        m_floatUniformData.clear();
        m_vec2UniformData.clear();
        m_vec3UniformData.clear();
        m_vec4UniformData.clear();
        m_colorUniformData.clear();
        m_textureUniformData.clear();
    }
}
