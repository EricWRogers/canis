#pragma once
#include <vector>

#include <string>

#include "Data/GLTexture.hpp"
//#include "Data/Vertex.hpp"


namespace Canis
{
	extern bool ReadFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer);

	extern GLTexture LoadImageToGLTexture(std::string filePath, int sourceFormat, int format);

    extern std::string GetFileName(std::string _path);

    extern std::string GetFileExtension(std::string _path);

	extern std::vector<std::string> FindFilesInFolder(const std::string &_folder, const std::string &_extension);

	extern bool FileExists(const char *_path);

	extern void OpenInVSCode(const std::string& _filePath);

	/*extern unsigned int LoadImageToCubemap(std::vector<std::string> faces, GLint sourceFormat);

	extern bool LoadOBJ(std::string path,
		std::vector < glm::vec3 > & out_vertices,
		std::vector < glm::vec2 > & out_uvs,
		std::vector < glm::vec3 > & out_normals
	);

	extern bool LoadOBJ(std::string path,
		std::vector < Vertex > & _vertices,
		std::vector < unsigned int > & _indices
	);*/
} // end of Canis namespace