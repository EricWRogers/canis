#include <Canis/Asset.hpp>

namespace Canis
{
    Asset::Asset() {}

    Asset::~Asset() {}

    bool Texture::Load(std::string path)
    {
        m_texture = LoadImageToGLTexture(path,GL_RGBA,GL_RGBA);
        return true;
    }

    bool Texture::Free()
    {
        return false;
    }

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

    bool Model::Load(std::string path)
    {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        
        Canis::LoadOBJ(path.c_str(), vertices, uvs, normals);

        for(int i = 0; i < vertices.size(); i++)
        {
            Canis::Vertex v = {};
            v.Position = vertices[i];
            v.Normal = normals[i];
            v.TexCoords = uvs[i];
            m_vertices.push_back(v);
        }

        m_size = m_vertices.size();
        Canis::Log("s " + std::to_string(m_vertices.size()));

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Canis::Vertex), &m_vertices[0], GL_STATIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // texture coords
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);

        return true;
    }

    bool Model::Free()
    {
        return false;
    }
} // end of Canis namespace