#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.hpp"
#include <Canis/Shader.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include "AssimpGLMHelper.hpp"
#include "Animation.hpp"

using namespace std;

namespace LearnOpenGL
{
class AnimatedModel 
{
public:
    // model data
    vector<Mesh> meshes;
	vector<Animation> animations;
    string directory;
    bool gammaCorrection;
	
    // constructor, expects a filepath to a 3D model.
    AnimatedModel(string const &path, bool gamma = false) : gammaCorrection(gamma)
    {
        loadModel(path);
    }

    // draws the model, and thus all its meshes
    void Draw(Canis::Shader &shader)
    {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

	auto& GetBoneInfoMap() { return m_BoneInfoMap; }
	int& GetBoneCount() { return m_BoneCounter; }
	

private:

	std::map<string, BoneInfo> m_BoneInfoMap;
	int m_BoneCounter = 0;

    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path);

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene);

	void SetVertexBoneDataToDefault(Vertex& vertex);

	Mesh processMesh(aiMesh* mesh, const aiScene* scene);

	void SetVertexBoneDataToDefault(Vertex& vertex);

	Mesh processMesh(aiMesh* mesh, const aiScene* scene);

	void SetVertexBoneData(Vertex& vertex, int boneID, float weight);

	void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);

	void ReadMissingBones(Animation& _animation, const aiAnimation* _aiAnimation);

	void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src);

	unsigned int TextureFromFile(const char* path, const string& directory, bool gamma = false);
    
    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Canis::GLTexture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName);
};
}
