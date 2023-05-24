#include <Canis/Asset.hpp>
#include <Canis/Debug.hpp>
#include <Canis/IOManager.hpp>
#include <memory>
#include <SDL_mixer.h>
#include <GL/glew.h>
#include <Canis/External/TMXLoader/TMXLoader.h>

namespace Canis
{
    Asset::Asset() {}

    Asset::~Asset() {}

    bool TextureAsset::Load(std::string path)
    {
        m_texture = LoadImageToGLTexture(path, GL_RGBA, GL_RGBA);
        return true;
    }

    bool TextureAsset::Free()
    {
        glDeleteTextures(1, &m_texture.id);
        return true;
    }

    bool SkyboxAsset::Load(std::string path)
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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

        return true;
    }

    bool SkyboxAsset::Free()
    {
        delete skyboxShader;
        glDeleteTextures(1, &cubemapTexture);
        glDeleteBuffers(1, &skyboxVBO);
        glDeleteVertexArrays(1, &skyboxVAO);
        return true;
    }

    bool ModelAsset::Load(std::string path)
    {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;

        Canis::LoadOBJ(path, vertices, uvs, normals);

        for (int i = 0; i < vertices.size(); i++)
        {
            Canis::Vertex v = {};
            v.position = vertices[i];
            v.normal = normals[i];
            v.texCoords = uvs[i];
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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        // normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // texture coords
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);

        return true;
    }

    bool ModelAsset::Free()
    {
        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);
        return true;
    }

    bool TextAsset::Load(std::string path)
    {
        // FreeType
        // --------
        // All functions return a value different than 0 whenever an error occurred
        FT_Library ft;
        FT_Face face;
        if (FT_Init_FreeType(&ft))
        {
            Canis::Error("ERROR::FREETYPE: Could not init FreeType Library");
        }

        if (FT_New_Face(ft, path.c_str(), 0, &face))
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

    bool SoundAsset::Load(std::string path)
    {
        chunk = Mix_LoadWAV(path.c_str());

        return chunk != nullptr;
    }

    bool SoundAsset::Free()
    {
        Mix_FreeChunk((Mix_Chunk*)chunk);
        chunk = nullptr;
        return true;
    }

    void SoundAsset::Play()
    {
        Mix_PlayChannel(-1, (Mix_Chunk*)chunk, 0);
    }

    bool MusicAsset::Load(std::string path)
    {
        music = Mix_LoadMUS(path.c_str());
        return music != nullptr;
    }

    bool MusicAsset::Free()
    {
        Mix_FreeMusic((Mix_Music*)music);
        music = nullptr;
        return true;
    }

    void MusicAsset::Play(int loops)
    {
        Mix_PlayMusic((Mix_Music*)music, loops);
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
        
        ((TMXLoader*)loader)->loadMap("map", _path);
        //((TMXLoader*)loader)->printMapData("testmap");

        return true;
    }

    bool TiledMapAsset::Free()
    {
        if (loader != nullptr)
            delete ((TMXLoader*)loader);
        
        return true;
    }
} // end of Canis namespace