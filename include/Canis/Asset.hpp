#pragma once
#include <string>

#include <Canis/Debug.hpp>
#include <Canis/Shader.hpp>
#include <Canis/IOManager.hpp>
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

    class Skybox : public Asset
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
} // end of Canis namespace