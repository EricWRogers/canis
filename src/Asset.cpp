#include <Canis/Asset.hpp>
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
#include <limits>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string.h>
#include <cctype>
#include <unordered_map>

#include <glm/gtc/quaternion.hpp>
#include <stb_image.h>

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
    } // namespace

    Asset::Asset() {}

    Asset::~Asset() {}

    bool TextureAsset::Load(std::string _path)
    {
        m_path = _path;
        m_texture = LoadImageToGLTexture(_path, GL_RGBA, GL_RGBA);
        return true;
    }

    bool TextureAsset::Free()
    {
        glDeleteTextures(1, &m_texture.id);
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
            case MetaFileAsset::FileType::SCENE:
                return "SCENE";
            case MetaFileAsset::FileType::ANIMATIONCLIP2D:
                return "ANIMATIONCLIP2D";
            case MetaFileAsset::FileType::MODEL:
                return "MODEL";
            case MetaFileAsset::FileType::MATERIAL:
                return "MATERIAL";
            case MetaFileAsset::FileType::SKYBOX:
                return "SKYBOX";
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
        else if (_type == "SCENE")
            return MetaFileAsset::FileType::SCENE;
        else if (_type == "ANIMATIONCLIP2D")
            return MetaFileAsset::FileType::ANIMATIONCLIP2D;
        else if (_type == "MODEL")
            return MetaFileAsset::FileType::MODEL;
        else if (_type == "MATERIAL")
            return MetaFileAsset::FileType::MATERIAL;
        else if (_type == "SKYBOX")
            return MetaFileAsset::FileType::SKYBOX;
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
            extension = GetFileExtension(_path);

            if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "bmp" || extension == "tga")
                type = FileType::TEXTURE;
            else if (extension == "scene")
                type = FileType::SCENE;
            else if (extension == "fs")
                type = FileType::FRAGMENT;
            else if (extension == "vs")
                type = FileType::VERTEX;
            else if (extension == "ac2d")
                type = FileType::ANIMATIONCLIP2D;
            else if (extension == "gltf" || extension == "glb")
                type = FileType::MODEL;
            else if (extension == "material")
                type = FileType::MATERIAL;
            else if (extension == "skybox")
                type = FileType::SKYBOX;
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
            extension = root["extension"].as<std::string>();
            size = root["size"].as<u64>();
            modified = root["modified"].as<i64>();
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

        std::string ResolveTexturePath(
            const tinygltf::Model &_gltfModel,
            const tinygltf::Primitive &_primitive,
            const std::filesystem::path &_modelDirectory)
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
            if (image.uri.empty())
                return "";

            if (image.uri.rfind("data:", 0) == 0)
            {
                Debug::Warning("Embedded image data URIs are not supported yet.");
                return "";
            }

            std::filesystem::path texturePath = _modelDirectory / image.uri;
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

        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(LoadTinyGLTFImageData, nullptr);
        tinygltf::Model gltfModel;
        std::string error;
        std::string warning;

        bool loaded = false;
        if (GetFileExtension(_path) == "glb")
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

        const std::filesystem::path modelDirectory = std::filesystem::path(_path).parent_path();
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
                std::string texturePath = ResolveTexturePath(gltfModel, primitive, modelDirectory);
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
            if (primitive.ebo != 0)
                glDeleteBuffers(1, &primitive.ebo);
            if (primitive.vbo != 0)
                glDeleteBuffers(1, &primitive.vbo);
            if (primitive.vao != 0)
                glDeleteVertexArrays(1, &primitive.vao);

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
        const std::vector<MaterialAsset*> *_slotMaterialOverrides)
    {
        const Pose3D *pose = (_pose == nullptr) ? &m_sharedPose : _pose;

        for (size_t primitiveIndex = 0; primitiveIndex < m_primitives.size(); ++primitiveIndex)
        {
            Primitive3D &primitive = m_primitives[primitiveIndex];
            Matrix4 model = _modelMatrix;
            if (primitive.nodeIndex >= 0 && primitive.nodeIndex < (i32)m_nodes.size())
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

    bool ModelAsset::BuildTriangleMesh(std::vector<Vector3> &_vertices, std::vector<u32> &_indices) const
    {
        _vertices.clear();
        _indices.clear();

        size_t totalVertexCount = 0;
        size_t totalIndexCount = 0;
        for (const Primitive3D &primitive : m_primitives)
        {
            totalVertexCount += primitive.bindVertices.size();
            totalIndexCount += primitive.indices.size();
        }

        if (totalVertexCount == 0 || totalIndexCount == 0)
            return false;

        _vertices.reserve(totalVertexCount);
        _indices.reserve(totalIndexCount);

        for (const Primitive3D &primitive : m_primitives)
        {
            if (primitive.bindVertices.empty() || primitive.indices.empty())
                continue;

            const u32 vertexOffset = static_cast<u32>(_vertices.size());
            Matrix4 nodeMatrix = Matrix4(1.0f);
            if (primitive.nodeIndex >= 0 && primitive.nodeIndex < static_cast<i32>(m_nodes.size()))
                nodeMatrix = m_nodes[primitive.nodeIndex].globalMatrix;

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

    bool SpriteAnimationAsset::Load(std::string _path)
    {
        return true;
    }

    bool SpriteAnimationAsset::Free()
    {
        return true;
    }

    void MaterialFields::Use(Shader &_shader) const
    {
        for (const FloatUniformData &uniform : m_floatUniformData)
            _shader.SetFloat(uniform.name, uniform.value);
    }

    void MaterialFields::SetFloat(const std::string &_name, float _value)
    {
        for (FloatUniformData &uniform : m_floatUniformData)
        {
            if (uniform.name == _name)
            {
                uniform.value = _value;
                return;
            }
        }

        m_floatUniformData.push_back(FloatUniformData{.name = _name, .value = _value});
    }
}
