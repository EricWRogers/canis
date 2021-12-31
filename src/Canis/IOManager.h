#pragma once
#include <vector>
#include <fstream>
#include <string>

#include "GLTexture.h"
#include "External/picoPNG.h"
#include "Debug.h"


namespace Canis
{
	bool ReadFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer);

	GLTexture LoadPNGToGLTexture(std::string filePath, GLint sourceFormat, GLint format);
} // end of Canis namespace