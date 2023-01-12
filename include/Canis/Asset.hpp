#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL_mixer.h>

#include <GL/glew.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <map>

#include <Canis/Debug.hpp>
#include <Canis/Shader.hpp>
#include <Canis/IOManager.hpp>
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

        virtual bool Load(std::string path) = 0;
        virtual bool Free() = 0;
    };

    class TextureAsset : public Asset
    {
    private:
        GLTexture m_texture;
    public:
        bool Load(std::string path) override;
        bool Free() override;
        GLTexture GetTexture() { return m_texture; }
    };

    class SkyboxAsset : public Asset
    {
    private:
        Canis::Shader *skyboxShader;
        unsigned int skyboxVAO, skyboxVBO;
        unsigned int cubemapTexture;
    public:
        bool Load(std::string path) override;
        bool Free() override;
        unsigned int GetVAO() { return skyboxVAO; }
        unsigned int GetTexture() { return cubemapTexture; }
        Canis::Shader* GetShader() { return skyboxShader; }
    };

    class ModelAsset : public Asset
    {
    private:
        std::vector<Canis::Vertex> m_vertices;
        unsigned int m_vao;
        unsigned int m_vbo;
        int m_size;
    public:
        bool Load(std::string path) override;
        bool Free() override;
        unsigned int GetVAO() { return m_vao; }
        unsigned int GetVBO() { return m_vbo; }
        int GetSize() { return m_size; }
        std::vector<Canis::Vertex> GetVertices() { return m_vertices; }
    };

    /*class MeshAsset : public Asset
    {
    private:
        std::string m_pathToBaseMesh = "";
    public:
        bool Load(std::string path) override;
        bool Free() override;
    };*/

    class TextAsset : public Asset
    {
    private:
        unsigned int m_vao, m_vbo, m_font_size;
    public:
        TextAsset(unsigned int _font_size) { m_font_size = _font_size; }
        std::map<GLchar, Character> Characters;
        bool Load(std::string path) override;
        bool Free() override;
        unsigned int GetVAO() { return m_vao; }
        unsigned int GetVBO() { return m_vbo; }
        unsigned int GetFontSize() { return m_font_size; }
    };

    class SoundAsset : public Asset
    {
    private:
        Mix_Chunk *chunk;
    public:
        bool Load(std::string path) override;
        bool Free() override;
        
        void Play();
    };

    class MusicAsset : public Asset
    {
    private:
        Mix_Music *music = nullptr;
    public:
        bool Load(std::string path) override;
        bool Free() override;
        
        void Play(int loops);
        void Stop();
    };
} // end of Canis namespace