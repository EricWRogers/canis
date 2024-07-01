#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Canis/Debug.hpp>
#include <Canis/External/OpenGl.hpp>
#include <Canis/Shader.hpp>
#include <Canis/Data/GLTexture.hpp>

#include <string>
#include <vector>
using namespace std;

#define MAX_BONE_INFLUENCE 4

namespace LearnOpenGL
{
struct Vertex {
    // position
    glm::vec3 Position;
    // normal
    glm::vec3 Normal;
    // texCoords
    glm::vec2 TexCoords;
	//bone indexes which will influence this vertex
	int m_BoneIDs[MAX_BONE_INFLUENCE];
	//weights from each bone
	float m_Weights[MAX_BONE_INFLUENCE];
};

class Mesh {
public:
    // mesh Data
    vector<Vertex>       vertices;
    vector<unsigned int> indices;
    vector<Canis::GLTexture> diffuseTextures = {};
    vector<Canis::GLTexture> specularTextures = {};
    vector<Canis::GLTexture> normalTextures = {};
    vector<Canis::GLTexture> heightTextures = {};

    unsigned int VAO;

    // constructor
    Mesh(vector<Vertex> vertices, vector<unsigned int> indices)
    {
        this->vertices = vertices;
        this->indices = indices;

        // now that we have all the required data, set the vertex buffers and its attribute pointers.
        setupMesh();
    }

    void AttachTexture(const std::string &_name, int _index, int _slot, Canis::GLTexture _texture, Canis::Shader &_shader)
    {
        // activate texture slot
        glActiveTexture(GL_TEXTURE0 + _slot);
        
        // now set the sampler to the correct texture unit
        _shader.SetInt(_name + std::to_string(_index), _slot);
            
        // and finally bind the texture
        glBindTexture(GL_TEXTURE_2D, _texture.id);
    }

    // render the mesh
    void Draw(Canis::Shader &shader) 
    {
        // bind appropriate textures
        unsigned int slot = 0;

        Canis::Log(std::to_string(diffuseTextures.size()));

        for (int i = 0; i < diffuseTextures.size(); i++)
            AttachTexture("texture_diffuse", i, slot++, diffuseTextures[i], shader);
        
        for (int i = 0; i < specularTextures.size(); i++)
            AttachTexture("texture_specular", i, slot++, specularTextures[i], shader);
        
        for (int i = 0; i < normalTextures.size(); i++)
            AttachTexture("texture_normal", i, slot++, normalTextures[i], shader);
        
        for (int i = 0; i < heightTextures.size(); i++)
            AttachTexture("texture_height", i, slot++, heightTextures[i], shader);
        
        // draw mesh
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // always good practice to set everything back to defaults once configured.
        glActiveTexture(GL_TEXTURE0);
    }

private:
    // render data 
    unsigned int VBO, EBO;

    // initializes all the buffer objects/arrays
    void setupMesh()
    {
        // create buffers/arrays
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        // load data into vertex buffers
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // A great thing about structs is that their memory layout is sequential for all its items.
        // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
        // again translates to 3/2 floats which translates to a byte array.
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);  

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // set the vertex attribute pointers
        // vertex Positions
        glEnableVertexAttribArray(0);	
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        // vertex normals
        glEnableVertexAttribArray(1);	
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        // vertex texture coords
        glEnableVertexAttribArray(2);	
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
		// ids
		glEnableVertexAttribArray(3);
		glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));
		// weights
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));
        glBindVertexArray(0);
    }
};
}