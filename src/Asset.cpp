#include <Canis/Canis.hpp>
#include <Canis/Asset.hpp>
#include <Canis/Debug.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/External/OpenGl.hpp>
#include <memory>
#include <string.h>
#include <SDL_mixer.h>
#include <unordered_map>
#include <Canis/External/TMXLoader/TMXLoader.h>
#include <Canis/Yaml.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include <Canis/External/TinyGLTF.hpp>

namespace Canis
{
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
        m_skyboxShader = new Shader();
        m_skyboxShader->Compile("assets/shaders/skybox.vs", "assets/shaders/skybox.fs");
        m_skyboxShader->AddAttribute("aPos");
        m_skyboxShader->Link();

        std::vector<std::string> faces;

        faces.push_back(std::string(_path).append("skybox_left.png"));
        faces.push_back(std::string(_path).append("skybox_right.png"));
        faces.push_back(std::string(_path).append("skybox_up.png"));
        faces.push_back(std::string(_path).append("skybox_down.png"));
        faces.push_back(std::string(_path).append("skybox_front.png"));
        faces.push_back(std::string(_path).append("skybox_back.png"));

        m_cubemapTexture = Canis::LoadImageToCubemap(faces, GL_RGBA);

        glGenVertexArrays(1, &m_skyboxVAO);
        glGenBuffers(1, &m_skyboxVBO);
        glBindVertexArray(m_skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, skyboxVertices.size() * sizeof(float), &skyboxVertices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

        return true;
    }

    bool SkyboxAsset::Free()
    {
        delete m_skyboxShader;
        glDeleteTextures(1, &m_cubemapTexture);
        glDeleteBuffers(1, &m_skyboxVBO);
        glDeleteVertexArrays(1, &m_skyboxVAO);
        return true;
    }

    void LoadMeshData(const tinygltf::Model &_model,
                      std::vector<glm::vec3> &_positions,
                      std::vector<glm::vec3> &_normals,
                      std::vector<glm::vec2> &_texCoords,
                      std::vector<unsigned int> &_indices)
    {
        unsigned int indicesOffset = 0;
        for (const tinygltf::Mesh &mesh : _model.meshes)
        {
            for (const tinygltf::Primitive &primitive : mesh.primitives)
            {
                const auto &attributes = primitive.attributes;
                indicesOffset = _positions.size();

                // Indices
                if (primitive.indices > 0)
                {
                    const tinygltf::Accessor &indexAccessor = _model.accessors[primitive.indices];
                    const tinygltf::BufferView &bufferView = _model.bufferViews[indexAccessor.bufferView];
                    const tinygltf::Buffer &buffer = _model.buffers[bufferView.buffer];

                    const void *dataPtr = &(buffer.data[bufferView.byteOffset + indexAccessor.byteOffset]);

                    // Depending on the component type, the indices can be byte, short or unsigned int
                    switch (indexAccessor.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    {
                        const unsigned int *buf = static_cast<const unsigned int *>(dataPtr);
                        for (size_t index = 0; index < indexAccessor.count; ++index)
                        {
                            _indices.push_back(buf[index] + indicesOffset);
                        }
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    {
                        const unsigned short *buf = static_cast<const unsigned short *>(dataPtr);
                        for (size_t index = 0; index < indexAccessor.count; ++index)
                        {
                            _indices.push_back(buf[index] + indicesOffset);
                        }
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    {
                        const unsigned char *buf = static_cast<const unsigned char *>(dataPtr);
                        for (size_t index = 0; index < indexAccessor.count; ++index)
                        {
                            _indices.push_back(buf[index] + indicesOffset);
                        }
                        break;
                    }
                    }
                }

                // Positions
                if (attributes.find("POSITION") != attributes.end())
                {
                    const tinygltf::Accessor &accessor = _model.accessors[attributes.at("POSITION")];
                    const tinygltf::BufferView &bufferView = _model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer &buffer = _model.buffers[bufferView.buffer];
                    const float *bufferPos = reinterpret_cast<const float *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                    for (size_t i = 0; i < accessor.count; i++)
                    {
                        _positions.push_back(glm::vec3(bufferPos[i * 3], bufferPos[i * 3 + 1], bufferPos[i * 3 + 2]));
                    }
                }

                // Normals
                if (attributes.find("NORMAL") != attributes.end())
                {
                    const tinygltf::Accessor &accessor = _model.accessors[attributes.at("NORMAL")];
                    const tinygltf::BufferView &bufferView = _model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer &buffer = _model.buffers[bufferView.buffer];
                    const float *bufferNormals = reinterpret_cast<const float *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                    for (size_t i = 0; i < accessor.count; i++)
                    {
                        _normals.push_back(glm::vec3(bufferNormals[i * 3], bufferNormals[i * 3 + 1], bufferNormals[i * 3 + 2]));
                    }
                }

                // Texture Coordinates
                if (attributes.find("TEXCOORD_0") != attributes.end())
                {
                    const tinygltf::Accessor &accessor = _model.accessors[attributes.at("TEXCOORD_0")];
                    const tinygltf::BufferView &bufferView = _model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer &buffer = _model.buffers[bufferView.buffer];
                    const float *bufferTexCoords = reinterpret_cast<const float *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                    for (size_t i = 0; i < accessor.count; i++)
                    {
                        _texCoords.push_back(glm::vec2(bufferTexCoords[i * 2], bufferTexCoords[i * 2 + 1]));
                    }
                }
            }
        }
    }

    bool ModelAsset::Load(std::string _path)
    {
        std::string extention = _path;

        while (extention.size() > 4)
        {
            extention.erase(extention.begin());
        }

        if (extention == ".obj")
        {
            std::string binFileName = _path + ".bin";
            SDL_RWops *file = SDL_RWFromFile(binFileName.c_str(), "r+b");

            if (file == nullptr)
            {
                std::vector<glm::vec3> modelVertices;
                std::vector<glm::vec2> uvs;
                std::vector<glm::vec3> normals;

                Canis::LoadOBJ(_path, modelVertices, uvs, normals);

                for (int i = 0; i < modelVertices.size(); i++)
                {
                    Canis::Vertex v = {};
                    v.position = modelVertices[i];
                    v.normal = normals[i];
                    v.texCoords = uvs[i];
                    vertices.push_back(v);
                }

                size = vertices.size();

                CalculateIndicesFromVertices(vertices);

                // save data
                file = SDL_RWFromFile(binFileName.c_str(), "w+b");
                // write indices size
                size_t indicesSize = indices.size();
                SDL_RWwrite(file, &indicesSize, sizeof(size_t), 1);
                // write indices
                SDL_RWwrite(file, indices.data(), sizeof(unsigned int), indicesSize);
                // write vertices size
                size_t verticesSize = vertices.size();
                SDL_RWwrite(file, &verticesSize, sizeof(size_t), 1);
                // write vertices
                SDL_RWwrite(file, vertices.data(), sizeof(Canis::Vertex), verticesSize);
            }
            else
            {
                size_t indicesSize = 0;
                SDL_RWread(file, &indicesSize, sizeof(size_t), 1);
                indices.reserve(indicesSize);
                indices.resize(indicesSize);
                SDL_RWread(file, indices.data(), sizeof(unsigned int), indicesSize);

                size_t verticesSize = 0;
                SDL_RWread(file, &verticesSize, sizeof(size_t), 1);
                vertices.reserve(verticesSize);
                vertices.resize(verticesSize);
                SDL_RWread(file, vertices.data(), sizeof(Canis::Vertex), verticesSize);
            }

            SDL_RWclose(file);
        }
        else if (".glb" == extention || "gltf" == extention)
        {
            using namespace tinygltf;

            Model model;
            TinyGLTF loader;
            std::string err;
            std::string warn;
            bool ret = false;

            if (".glb" == extention)
                ret = loader.LoadBinaryFromFile(&model, &err, &warn, _path);
            else
                ret = loader.LoadASCIIFromFile(&model, &err, &warn, _path);

            if (!warn.empty())
            {
                printf("Warn: %s\n", warn.c_str());
            }

            if (!err.empty())
            {
                printf("Err: %s\n", err.c_str());
            }

            if (!ret)
            {
                printf("Failed to parse glTF\n");
                return -1;
            }

            std::vector<glm::vec3> modelVertices;
            std::vector<glm::vec2> uvs;
            std::vector<glm::vec3> normals;

            LoadMeshData(model, modelVertices, normals, uvs, indices);

            for (int i = 0; i < modelVertices.size(); i++)
            {
                Canis::Vertex v = {};
                v.position = modelVertices[i];
                v.normal = normals[i];
                v.texCoords = uvs[i];
                vertices.push_back(v);
            }

            size = vertices.size();
        }
        else
        {
            Warning("Model type not supported: " + _path);
        }

        Bind();

        return true;
    }

    void ModelAsset::Bind()
    {
        size = vertices.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Canis::Vertex), &vertices[0], GL_STATIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        // normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // texture coords
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);
    }

    void ModelAsset::CalculateIndicesFromVertices(const std::vector<Canis::Vertex> &_vertices)
    {
        size = _vertices.size();

        bool found = false;
        unsigned int s = vertices.size();

        for (GLuint i = 0; i < s; i++)
        {
            found = false;

            for (int v = s - 1; v > -1; v--)
            {
                if (memcmp(&_vertices[i], &vertices[v], sizeof(Vertex)) == 0)
                {
                    indices.push_back(v);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                vertices.push_back(_vertices[i]);
                s = vertices.size();
                indices.push_back(s - 1);
            }
        }
    }

    bool ModelAsset::Free()
    {
        vertices.resize(0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
        glDeleteVertexArrays(1, &vao);

        return true;
    }

    bool ModelAsset::LoadWithVertex(const std::vector<Canis::Vertex> &_vertices)
    {
        CalculateIndicesFromVertices(_vertices);

        Bind();

        return true;
    }

    bool InstanceMeshAsset::Load(std::string _path)
    {
        return false;
    }

    bool InstanceMeshAsset::Load(unsigned int _modelID, const std::vector<glm::mat4> &_modelMatrices, unsigned int _vao)
    {
        modelMatrices = _modelMatrices;

        modelID = _modelID;

        glBindVertexArray(_vao);

        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, modelMatrices.size() * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);

        glBindVertexArray(_vao);
        // set attribute pointers for matrix (4 times vec4)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)0);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)(sizeof(glm::vec4)));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)(2 * sizeof(glm::vec4)));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)(3 * sizeof(glm::vec4)));

        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);

        return true;
    }

    bool InstanceMeshAsset::Free()
    {
        glDeleteBuffers(1, &buffer);
        return false;
    }

    bool TextAsset::Load(std::string _path)
    {
        m_path = _path;
        // FreeType
        // --------
        // All functions return a value different than 0 whenever an error occurred
        FT_Library ft;
        FT_Face face;

        if (FT_Init_FreeType(&ft))
        {
            Canis::Error("ERROR::FREETYPE: Could not init FreeType Library");
        }

        if (FT_New_Face(ft, _path.c_str(), 0, &face))
        {
            Canis::Error("ERROR::FREETYPE: Failed to load font");
        }
        else
        {
            // set size to load glyphs as
            FT_Set_Pixel_Sizes(face, 0, m_font_size);

            // disable byte-alignment restriction
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            // load first 128 characters of ASCII set
            for (unsigned char c = 0; c < 128; c++)
            {
                // Load character glyph
                if (FT_Load_Char(face, c, FT_LOAD_RENDER))
                {
                    std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
                    continue;
                }
                // generate texture
                unsigned int texture;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                #ifdef __EMSCRIPTEN__
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_LUMINANCE,
                    face->glyph->bitmap.width,
                    face->glyph->bitmap.rows,
                    0,
                    GL_LUMINANCE,
                    GL_UNSIGNED_BYTE,
                    face->glyph->bitmap.buffer);
                #else
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RED,
                    face->glyph->bitmap.width,
                    face->glyph->bitmap.rows,
                    0,
                    GL_RED,
                    GL_UNSIGNED_BYTE,
                    face->glyph->bitmap.buffer);
                #endif
                
                // set texture options
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                // now store character for later use
                Character character = {
                    texture,
                    glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                    glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                    static_cast<unsigned int>(face->glyph->advance.x)};
                Characters.insert(std::pair<char, Character>(c, character));
            }
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        // configure m_vao/m_vbo for texture quads
        // -----------------------------------
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
    }

    bool TextAsset::Free()
    {
        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);
        return true;
    }

    bool SoundAsset::Load(std::string _path)
    {
        m_chunk = Mix_LoadWAV(_path.c_str());

        if (m_chunk == nullptr)
        {
            Canis::Warning("Warning failed to load: " + _path);
        }

        return m_chunk != nullptr;
    }

    bool SoundAsset::Free()
    {
        Mix_FreeChunk((Mix_Chunk *)m_chunk);
        m_chunk = nullptr;
        return true;
    }

    void SoundAsset::Play()
    {
        int channel = Play(1.0f, false);
    }

    void SoundAsset::Play(float _volume)
    {
        int channel = Play(_volume, false);
    }

    int SoundAsset::Play(float _volume, bool _loop)
    {
        m_volume = _volume;
        int channel = -1;

        if (_loop)
            channel = Mix_PlayChannel(-1, (Mix_Chunk *)m_chunk, -1);
        else
            channel = Mix_PlayChannel(-1, (Mix_Chunk *)m_chunk, 0);

        Mix_Volume(channel, 128 * _volume * GetProjectConfig().volume * GetProjectConfig().sfxVolume);
        return channel;
    }

    bool MusicAsset::Load(std::string _path)
    {
        m_music = Mix_LoadMUS(_path.c_str());

        if (m_music == nullptr)
        {
            Canis::Warning("Warning failed to load: " + _path);
        }

        return m_music != nullptr;
    }

    bool MusicAsset::Free()
    {
        Mix_FreeMusic((Mix_Music *)m_music);
        m_music = nullptr;
        return true;
    }

    void MusicAsset::Play(int _loops)
    {
        Play(_loops, 1.0f);
    }

    void MusicAsset::Play(int _loops, float _volume)
    {
        m_volume = _volume;
        Mix_PlayMusic((Mix_Music *)m_music, _loops);

        UpdateVolume();
    }

    void MusicAsset::UpdateVolume()
    {
        if (GetProjectConfig().mute)
            Mix_VolumeMusic(0.0f);
        else
            Mix_VolumeMusic(128 * m_volume * GetProjectConfig().volume * GetProjectConfig().musicVolume);
    }

    void MusicAsset::Mute()
    {
        Mix_VolumeMusic(0.0f);
    }

    void MusicAsset::UnMute()
    {
        Mix_VolumeMusic(128 * m_volume * GetProjectConfig().volume);
    }

    void MusicAsset::Stop()
    {
        Mix_HaltMusic();
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

    bool SpriteAnimationAsset::Load(std::string _path)
    {
        return true;
    }

    bool SpriteAnimationAsset::Free()
    {
        return true;
    }

    bool TiledMapAsset::Load(std::string _path)
    {
        if (loader == nullptr)
            loader = new TMXLoader();

        ((TMXLoader *)loader)->loadMap("map", _path);
        //((TMXLoader*)loader)->printMapData("testmap");

        return true;
    }

    bool TiledMapAsset::Free()
    {
        if (loader != nullptr)
            delete ((TMXLoader *)loader);

        return true;
    }

    bool PrefabAsset::Load(const std::string _path)
    {
        m_node = YAML::LoadFile(_path);
        return true;
    }

    bool PrefabAsset::Free()
    {
        return true;
    }

    YAML::Node &PrefabAsset::GetNode()
    {
        return m_node;
    }
} // end of Canis namespace