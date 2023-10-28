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


namespace Canis
{
    class Asset
    {
    public:
        Asset();
        ~Asset();

        Asset(const Asset&) = delete;
        Asset& operator= (const Asset&) = delete;

        virtual bool Load(std::string _path) = 0;
        virtual bool Free() = 0;
    };

    class TextureAsset : public Asset
    {
    private:
        GLTexture m_texture;
    public:
        bool Load(std::string _path) override;
        bool Free() override;
        GLTexture GetTexture() { return m_texture; }
        GLTexture* GetPointerToTexture() { return &m_texture; }
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
        Canis::Shader* GetShader() { return m_skyboxShader; }
    };

    class ModelAsset : public Asset
    {
    public:
        bool Load(std::string _path) override;
        bool Free() override;
        bool LoadWithVertex(const std::vector<Canis::Vertex> &_vertices);

        std::vector<Vertex> vertices;
        std::vector<int> indices;
        unsigned int vao;
        unsigned int vbo;
        unsigned int ebo;
        int size;
    };

    class TextAsset : public Asset
    {
    private:
        unsigned int m_vao, m_vbo, m_font_size;
    public:
        TextAsset(unsigned int _font_size) { m_font_size = _font_size; }
        std::map<GLchar, Character> Characters;
        bool Load(std::string _path) override;
        bool Free() override;
        unsigned int GetVAO() { return m_vao; }
        unsigned int GetVBO() { return m_vbo; }
        unsigned int GetFontSize() { return m_font_size; }
    };

    class SoundAsset : public Asset
    {
    private:
        void *m_chunk;
    public:
        bool Load(std::string _path) override;
        bool Free() override;
        
        void Play();
    };

    class MusicAsset : public Asset
    {
    private:
        void *m_music = nullptr;
    public:
        bool Load(std::string _path) override;
        bool Free() override;
        
        void Play(int _loops);
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

        Canis::Shader* GetShader() { return m_shader; }
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

    enum MaterialInfo
    {
        HASSHADER                           = 1u,
        HASALBEDO                           = 2u,
        HASSPECULAR                         = 4u,
        HASEMISSION                         = 8u,
        HASNOISE                            = 16u,
        HASSCREENTEXTURE                    = 32u,
        HASCOLOR                            = 64u,
        HASEMISSIONCOLOR                    = 128u,
        HASEMISSIONUSINGALEDOPLUSINTESITY   = 256u
    };
    
    /*
    color: [1.0, 1.0, 1.0, 1.0]
    emission_color: [0.0, 0.0, 0.0]
    emission_using_albedo_plus_intesity: 0.0
    */
    struct MaterialAsset
    {
        unsigned int info;
        unsigned int shaderId;
        unsigned int albedoId;
        unsigned int specularId;
        unsigned int emissionId;
        unsigned int noiseId;
        glm::vec4 color;
        glm::vec3 emissionColor;
        float emissionUsingAlbedoPlusIntesity;
    };
} // end of Canis namespace