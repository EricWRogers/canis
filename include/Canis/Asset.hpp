#pragma once
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <map>

#include <Canis/Shader.hpp>
#include <Canis/Data/GLTexture.hpp>
#include <Canis/Data/Character.hpp>
#include <Canis/Data/Vertex.hpp>
#include <Canis/Data/DefaultMeshData.hpp>

#include <yaml-cpp/node/node.h>

namespace Canis
{
    class Asset
    {
    public:
        Asset();
        ~Asset();

        Asset(const Asset &) = delete;
        Asset &operator=(const Asset &) = delete;

        virtual bool Load(std::string _path) = 0;
        virtual bool Free() = 0;
    };

    class TextureAsset : public Asset
    {
    private:
        GLTexture m_texture;
        std::string m_path = "";

    public:
        bool Load(std::string _path) override;
        bool Free() override;
        GLTexture GetGLTexture() { return m_texture; }
        GLTexture *GetPointerToTexture() { return &m_texture; }
        std::string GetPath() { return m_path; }
    };

    class SkyboxAsset : public Asset
    {
    private:
        Canis::Shader *m_skyboxShader;
        unsigned int m_skyboxVAO, m_skyboxVBO;
        unsigned int m_cubemapTexture;

    public:
        bool Load(std::string _path) override;
        bool Free() override;
        unsigned int GetVAO() { return m_skyboxVAO; }
        unsigned int GetTexture() { return m_cubemapTexture; }
        Canis::Shader *GetShader() { return m_skyboxShader; }
    };

    class ModelAsset : public Asset
    {
    public:
        bool Load(std::string _path) override;
        bool Free() override;
        bool LoadWithVertex(const std::vector<Canis::Vertex> &_vertices);
        void Bind();

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        unsigned int vao;
        unsigned int vbo;
        unsigned int ebo;
        int size;

    private:
        void CalculateIndicesFromVertices(const std::vector<Canis::Vertex> &_vertices);
    };

    class InstanceMeshAsset : public Asset
    {
    public:
        bool Load(std::string _path) override;
        bool Free() override;
        bool Load(std::string _path, const std::vector<glm::mat4> &_modelMatrices);
        bool Load(const std::vector<Vertex> &_vertices, const std::vector<unsigned int> &_indices, const std::vector<glm::mat4> &_modelMatrices);

        unsigned int buffer;
        
        ModelAsset model;

        std::vector<glm::mat4> modelMatrices = {};
    private:
        void Bind();
    };

    class TextAsset : public Asset
    {
    private:
        std::string m_path = "";
        unsigned int m_texture;
        unsigned int m_vao, m_vbo, m_fontSize;
        // guessing the size
        const static int atlasWidth = 1024;
        const static int atlasHeight = 1024;
        GLubyte m_atlasData[atlasWidth * atlasHeight] = {};
    public:
        TextAsset(unsigned int _fontSize) { m_fontSize = _fontSize; }
        Character characters[127];
        bool Load(std::string _path) override;
        bool Free() override;
        unsigned int GetTexture() { return m_texture; }
        unsigned int GetVAO() { return m_vao; }
        unsigned int GetVBO() { return m_vbo; }
        unsigned int GetFontSize() { return m_fontSize; }

        std::string GetPath() { return m_path; }
    };

    class SoundAsset : public Asset
    {
    private:
        void *m_chunk = nullptr;
        float m_volume = 0.0f;

    public:
        bool Load(std::string _path) override;
        bool Free() override;

        void Play();
        void Play(float _volume);
        int Play(float _volume, bool _loop);
    };

    class MusicAsset : public Asset
    {
    private:
        void *m_music = nullptr;
        float m_volume = 0.0f;

    public:
        bool Load(std::string _path) override;
        bool Free() override;

        void Play(int _loops);
        void Play(int _loops, float _volume);
        void UpdateVolume();
        void Mute();
        void UnMute();
        void Stop();
    };

    class ShaderAsset : public Asset
    {
    private:
        Canis::Shader *m_shader;

    public:
        ShaderAsset() { m_shader = new Canis::Shader(); }

        bool Load(std::string _path) override;
        bool Free() override;

        Canis::Shader *GetShader() { return m_shader; }
    };

    struct SpriteFrame
    {
        float timeOnFrame = 0.0f;
        unsigned int textureId = 0u;
        unsigned short int offsetX = 0u;
        unsigned short int offsetY = 0u;
        unsigned short int row = 0u;
        unsigned short int col = 0u;
        unsigned short int width = 0u;
        unsigned short int height = 0u;
    };

    class SpriteAnimationAsset : public Asset
    {
    private:
    public:
        SpriteAnimationAsset() {}

        bool Load(std::string _path) override;
        bool Free() override;

        std::vector<SpriteFrame> frames = {};
    };

    class TiledMapAsset : public Asset
    {
    public:
        TiledMapAsset() {}

        bool Load(std::string _path) override;
        bool Free() override;

        unsigned int GetTileWidth();
        unsigned int GetTileHeight();

        std::vector<std::vector<unsigned int>> GetTiles(std::string _layer);

        void *GetLoader() { return loader; }

    private:
        void *loader = nullptr;
    };

    enum MaterialInfo
    {
        HASSHADER = 1u,
        HASALBEDO = 2u,
        HASSPECULAR = 4u,
        HASEMISSION = 8u,
        HASSCREENTEXTURE = 16u,
        HASCOLOR = 32u,
        HASEMISSIONCOLOR = 64u,
        HASEMISSIONUSINGALEDOPLUSINTESITY = 128u,
        HASDEPTH = 256u,
        HASBACKFACECULLING = 512u,
        HASFRONTFACECULLING = 1024u,
        HASCUSTOMDEPTHSHADER = 2048u,
    };

    class MaterialFields
    {
    private:
        struct FloatUniformData
        {
            std::string name;
            float value;
        };
        std::vector<FloatUniformData> floatUniformData = {};

    public:
        void Use(Shader &_shader);

        void SetFloat(const std::string &_string, float _value);
    };

    struct MaterialAsset
    {
        unsigned int info = 0u;
        unsigned int shaderId = 0u;
        unsigned int depthShaderId = 0u;
        unsigned int albedoId = 0u;
        unsigned int specularId = 0u;
        unsigned int emissionId = 0u;
        glm::vec4 color;
        glm::vec3 emissionColor;
        float emissionUsingAlbedoPlusIntesity;
        std::vector<std::string> texNames;
        std::vector<unsigned int> texId;
        MaterialFields materialFields;
    };

    class PrefabAsset : public Asset
    {
    private:
        YAML::Node m_node;

    public:
        bool Load(const std::string _path);
        bool Free();

        YAML::Node &GetNode();
    };
} // end of Canis namespace