#include "IOManager.hpp"

namespace Engine
{
	bool ReadFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer)
	{
		std::ifstream file(filePath, std::ios::binary);
		if (file.fail())
		{
			perror(filePath.c_str());
			return false;
		}

		//seek to the end
		file.seekg(0, std::ios::end);

		//get file size
		int fileSize = file.tellg();

		//return to the begining so we can read the file
		file.seekg(0, std::ios::beg);

		fileSize -= file.tellg();

		buffer.resize(fileSize);
		file.read((char *)&(buffer[0]), fileSize);

		file.close();

		return true;
	}

	GLTexture LoadPNGToGLTexture(std::string filePath, GLint sourceFormat, GLint format)
	{
		GLTexture texture;

		std::vector<unsigned char> in;
		std::vector<unsigned char> out;

		unsigned long width, height;

		if (ReadFileToBuffer(filePath, in) == false)
			FatalError("Failed to loadPNG file to buffer");

		int errorCode = decodePNG(out, width, height, &(in[0]), in.size());
		if (errorCode != 0)
			FatalError("decodePNG failed with error: " + std::to_string(errorCode));

		glGenTextures(1, &texture.id);
		Log(std::to_string(texture.id));

		glBindTexture(GL_TEXTURE_2D, texture.id);
		//to give it a frame
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, &(out[0]));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(out[0]));

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);

		texture.width = width;
		texture.height = height;

		return texture;
	}
} // end of Engine namespace
