#include <Canis/Asset.hpp>

namespace Canis
{
    Asset::Asset() {}

    Asset::~Asset() {}

    bool Skybox::Load(std::string path)
    {
        skyboxShader = new Shader();
        skyboxShader->Compile("assets/shaders/skybox.vs", "assets/shaders/skybox.fs");
        skyboxShader->AddAttribute("aPos");
        skyboxShader->Link();

        std::vector<std::string> faces;

        faces.push_back(std::string(path).append("skybox_left.png"));
        faces.push_back(std::string(path).append("skybox_right.png"));
        faces.push_back(std::string(path).append("skybox_up.png"));
        faces.push_back(std::string(path).append("skybox_down.png"));
        faces.push_back(std::string(path).append("skybox_front.png"));
        faces.push_back(std::string(path).append("skybox_back.png"));

        cubemapTexture = Canis::LoadImageToCubemap(faces, GL_RGBA);

        Canis::Log("s " + std::to_string(skyboxVertices.size()));
        Canis::Log("path : " + faces[0]);

        glGenVertexArrays(1, &skyboxVAO);
        glGenBuffers(1, &skyboxVBO);
        glBindVertexArray(skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, skyboxVertices.size() * sizeof(float), &skyboxVertices[0], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        
        return true;
    }

    bool Skybox::Free()
    {
        return false;
    }
} // end of Canis namespace