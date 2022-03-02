#pragma once
#include <vector>
#include <fstream>
#include <string>

#include "Data/GLTexture.hpp"
#include "External/picoPNG.h"
#include "Debug.hpp"


namespace Canis
{
	extern bool ReadFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer);

	extern GLTexture LoadPNGToGLTexture(std::string filePath, GLint sourceFormat, GLint format);
} // end of Canis namespace