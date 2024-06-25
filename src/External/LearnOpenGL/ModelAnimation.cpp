#include <Canis/External/LearnOpenGL/ModelAnimation.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/External/OpenGl.hpp>
#include <stb_image.h>

namespace LearnOpenGL
{
    void AnimatedModel::loadModel(string const &path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);
        // check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);

		// load animations
		for (int i = 0; i < scene->mNumAnimations; i++)
		{
			Animation animation;
			auto aiAnimation = scene->mAnimations[i];
			Canis::Log("number of animations: " + std::to_string(scene->mNumAnimations));
			animation.duration = aiAnimation->mDuration;
			animation.ticksPerSecond = aiAnimation->mTicksPerSecond;
			aiMatrix4x4 globalTransformation = scene->mRootNode->mTransformation;
			globalTransformation = globalTransformation.Inverse();
			ReadHierarchyData(animation.rootNode, scene->mRootNode);
			ReadMissingBones(animation, aiAnimation);
			animations.push_back(animation);
		}
    }

    void AnimatedModel::processNode(aiNode *node, const aiScene *scene)
    {
        // process each mesh located at the current node
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene. 
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }

    void AnimatedModel::SetVertexBoneDataToDefault(Vertex& vertex)
	{
		for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
		{
			vertex.m_BoneIDs[i] = -1;
			vertex.m_Weights[i] = 0.0f;
		}
	}

	Mesh AnimatedModel::processMesh(aiMesh* mesh, const aiScene* scene)
	{
		vector<Vertex> vertices;
		vector<unsigned int> indices;

		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			SetVertexBoneDataToDefault(vertex);
			vertex.Position = AssimpGLMHelpers::GetGLMVec(mesh->mVertices[i]);
			vertex.Normal = AssimpGLMHelpers::GetGLMVec(mesh->mNormals[i]);
			
			if (mesh->mTextureCoords[0])
			{
				glm::vec2 vec;
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = mesh->mTextureCoords[0][i].y;
				vertex.TexCoords = vec;
			}
			else
				vertex.TexCoords = glm::vec2(0.0f, 0.0f);

			vertices.push_back(vertex);
		}
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		ExtractBoneWeightForVertices(vertices,mesh,scene);

		Mesh m = Mesh(vertices, indices);

		m.diffuseTextures = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
		m.specularTextures = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
		m.normalTextures = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
		m.heightTextures = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");

		return m;
	}

	void AnimatedModel::SetVertexBoneData(Vertex& vertex, int boneID, float weight)
	{
		for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
		{
			if (vertex.m_BoneIDs[i] < 0)
			{
				vertex.m_Weights[i] = weight;
				vertex.m_BoneIDs[i] = boneID;
				break;
			}
		}
	}

	void AnimatedModel::ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
	{
		auto& boneInfoMap = m_BoneInfoMap;
		int& boneCount = m_BoneCounter;

		for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			int boneID = -1;
			std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
			if (boneInfoMap.find(boneName) == boneInfoMap.end())
			{
				BoneInfo newBoneInfo;
				newBoneInfo.id = boneCount;
				newBoneInfo.offset = AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
				boneInfoMap[boneName] = newBoneInfo;
				boneID = boneCount;
				boneCount++;
			}
			else
			{
				boneID = boneInfoMap[boneName].id;
			}
			assert(boneID != -1);
			auto weights = mesh->mBones[boneIndex]->mWeights;
			int numWeights = mesh->mBones[boneIndex]->mNumWeights;

			for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex)
			{
				int vertexId = weights[weightIndex].mVertexId;
				float weight = weights[weightIndex].mWeight;
				assert(vertexId <= vertices.size());
				SetVertexBoneData(vertices[vertexId], boneID, weight);
			}
		}
	}

	void AnimatedModel::ReadMissingBones(Animation& _animation, const aiAnimation* _aiAnimation)
	{
		int size = _aiAnimation->mNumChannels;

		auto& boneInfoMap = GetBoneInfoMap();//getting m_BoneInfoMap from Model class
		int& boneCount = GetBoneCount(); //getting the m_BoneCounter from Model class

		//reading channels(bones engaged in an _aiAnimation and their keyframes)
		for (int i = 0; i < size; i++)
		{
			auto channel = _aiAnimation->mChannels[i];
			std::string boneName = channel->mNodeName.data;

			if (boneInfoMap.find(boneName) == boneInfoMap.end())
			{
				boneInfoMap[boneName].id = boneCount;
				boneCount++;
			}
			_animation.bones.push_back(Bone(channel->mNodeName.data,
				boneInfoMap[channel->mNodeName.data].id, channel));
		}

		_animation.boneInfoMap = boneInfoMap;
	}

	void AnimatedModel::ReadHierarchyData(AssimpNodeData& dest, const aiNode* src)
	{
		assert(src);

		dest.name = src->mName.data;
		dest.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(src->mTransformation);
		dest.childrenCount = src->mNumChildren;

		for (int i = 0; i < src->mNumChildren; i++)
		{
			AssimpNodeData newData;
			ReadHierarchyData(newData, src->mChildren[i]);
			dest.children.push_back(newData);
		}
	}

	unsigned int AnimatedModel::TextureFromFile(const char* path, const string& directory, bool gamma)
	{
		string filename = string(path);
		filename = directory + '/' + filename;

		unsigned int textureID;
		glGenTextures(1, &textureID);

		int width, height, nrComponents;
		unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
		if (data)
		{
			GLenum format;
			if (nrComponents == 1)
				format = GL_RED;
			else if (nrComponents == 3)
				format = GL_RGB;
			else if (nrComponents == 4)
				format = GL_RGBA;

			glBindTexture(GL_TEXTURE_2D, textureID);
			glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			stbi_image_free(data);
		}
		else
		{
			std::cout << "Texture failed to load at path: " << path << std::endl;
			stbi_image_free(data);
		}

		return textureID;
	}
    
    vector<Canis::GLTexture> AnimatedModel::loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
    {
        vector<Canis::GLTexture> textures;

        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            textures.push_back(Canis::AssetManager::GetTexture(directory + "/" + str.C_Str())->GetGLTexture());
        }
        return textures;
    }
}