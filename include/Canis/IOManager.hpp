#pragma once
#include <vector>
#include <stdio.h>
#include <fstream>
#include <cstring>
#include <string>
#include <glm/glm.hpp>
#include <SDL.h>
#include <stb_image.h>

#include "Data/GLTexture.hpp"
#include "External/picoPNG.h"
#include "Debug.hpp"


namespace Canis
{
	extern bool ReadFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer);

	extern GLTexture LoadImageToGLTexture(std::string filePath, GLint sourceFormat, GLint format);

	extern unsigned int LoadImageToCubemap(std::vector<std::string> faces, GLint sourceFormat);

	extern bool LoadOBJ(std::string path,
		std::vector < glm::vec3 > & out_vertices,
		std::vector < glm::vec2 > & out_uvs,
		std::vector < glm::vec3 > & out_normals
	);
} // end of Canis namespace