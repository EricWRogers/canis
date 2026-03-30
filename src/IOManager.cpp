#include <Canis/IOManager.hpp>
#include <string>
#include <fstream>
#include <cstdlib>
#include <filesystem>


#include <Canis/Math.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>
//#include <Canis/External/picoPNG.h>

#include <stb_image.h>

#include <SDL3/SDL.h>

namespace Canis
{
	bool ReadFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer)
	{
		std::ifstream file(filePath, std::ios::binary);
		if (file.fail())
		{
			perror(filePath.c_str());
			return false;
		}

		// seek to the end
		file.seekg(0, std::ios::end);

		// get file size
		int fileSize = file.tellg();

		// return to the begining so we can read the file
		file.seekg(0, std::ios::beg);

		fileSize -= file.tellg();

		buffer.resize(fileSize);
		file.read((char *)&(buffer[0]), fileSize);

		file.close();

		return true;
	}

	GLTexture LoadImageToGLTexture(std::string filePath, int sourceFormat, int format)
	{
		GLTexture texture;
		int nrChannels;

		glGenTextures(1, &texture.id);
		glBindTexture(GL_TEXTURE_2D, texture.id);

		stbi_set_flip_vertically_on_load(false);
		SDL_IOStream* io = SDL_IOFromFile(filePath.c_str(), "rb");

		if (io != NULL)
		{
			size_t imageDataLength = 0;
			void* imageData = SDL_LoadFile_IO(io, &imageDataLength, true);

			// convert to stbi thing
			stbi_uc *data = stbi_load_from_memory(static_cast<stbi_uc *>(imageData), imageDataLength, &texture.width, &texture.height, &nrChannels, 4);
			if (data)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, sourceFormat, texture.width, texture.height, 0, format, GL_UNSIGNED_BYTE, data);
			}
			else
			{
				Debug::Error("Failed to load texture %s", filePath.c_str());
			}
			stbi_image_free(data);
			// SDL_RWclose(file);
			// SDL_free(imageData);
		}
		else
		{
			Debug::Error("Failed to open file at path : %s", filePath.c_str());
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// Problem for future ERIC
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);//GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//GL_LINEAR);

		glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);

		stbi_set_flip_vertically_on_load(true);

		return texture;
	}

	// TODO : Add ERROR Check
    std::string GetFileName(std::string _path)
    {
        for (int i = _path.size(); i > -1; i--)
        {
            if (_path[i] == '.')
            {
                _path.erase(_path.begin() + i, _path.end());
            }

            if (_path[i] == '/')
            {
                _path.erase(_path.begin(), _path.begin() + i + 1);
                break;
            }

            if (_path[i] == '\\') // check on windows
            {
                _path.erase(_path.begin(), _path.begin() + i + 1);
                break;
            }
        }

        return _path;
    }

    // TODO : Add ERROR Check
    std::string GetFileExtension(std::string _path)
    {
        for (int i = _path.size(); i > -1; i--)
        {
            if (_path[i] == '.')
            {
                _path.erase(_path.begin(), _path.begin() + i + 1);
                break;
            }
        }

        return _path;
    }

	std::vector<std::string> FindFilesInFolder(const std::string &_folder, const std::string &_extension)
    {
        namespace fs = std::filesystem;

        std::vector<std::string> files;

        if (!fs::exists(_folder) || !fs::is_directory(_folder))
        {
            Debug::Log("FindFilesInFolder: folder not found: %s", _folder.c_str());
            return files;
        }

        for (const auto &entry : fs::recursive_directory_iterator(_folder))
        {
            if (entry.is_regular_file() && entry.path().extension() != ".meta" && entry.path().filename().string()[0] != '.') // && entry.path().extension() == _extension)
            {
                files.push_back(entry.path().generic_string());
            }
        }

        return files;
    }

	bool FileExists(const char *_path)
    {
        FILE *file = fopen(_path, "r");
        if (file)
        {
            fclose(file);
            return true;
        }
        return false;
    }

	void OpenInVSCode(const std::string& _filePath)
	{
	#if defined(__EMSCRIPTEN__)
		Debug::Warning("OpenInVSCode is unavailable in web builds.");
	#else
	#if defined(_WIN32)
		std::string cmd = "code --reuse-window \"" + _filePath + "\"";
	#elif defined(__APPLE__)
		// macOS: assume VS Code installed via Homebrew or standard path
		std::string cmd = "code --reuse-window \"" + _filePath + "\"";
	#else
		// Linux: assumes "code" is in PATH
		std::string cmd = "code --reuse-window \"" + _filePath + "\"";
	#endif
		int exitCode = std::system(cmd.c_str());
		
		if (exitCode != 0)
			Debug::Warning("Warning OpenInVSCode exit code: %i command: %s", exitCode, cmd.c_str());
	#endif
	}

    /*
	// loads a cubemap texture from 6 individual texture faces
	// order:
	// +X (right)
	// -X (left)
	// +Y (top)
	// -Y (bottom)
	// +Z (front)
	// -Z (back)
	extern unsigned int LoadImageToCubemap(std::vector<std::string> faces, GLint sourceFormat)
	{
		stbi_set_flip_vertically_on_load(false);

		unsigned int textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

		int width, height, nrChannels;

		for (unsigned int i = 0; i < faces.size(); i++)
		{
			unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
			if (data)
			{
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, sourceFormat, width, height, 0, sourceFormat, GL_UNSIGNED_BYTE, data);
				stbi_image_free(data);
			}
			else
			{
				std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
				stbi_image_free(data);
			}
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		stbi_set_flip_vertically_on_load(true);

		return textureID;
	}

	bool LoadOBJ(
		std::string path,
		std::vector<glm::vec3> &out_vertices,
		std::vector<glm::vec2> &out_uvs,
		std::vector<glm::vec3> &out_normals)
	{
		std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
		std::vector<glm::vec3> temp_vertices;
		std::vector<glm::vec2> temp_uvs;
		std::vector<glm::vec3> temp_normals;

		FILE *file = fopen(path.c_str(), "r");
		if (file == NULL)
		{
			FatalError("Can not open model: " + path);
		}

		while (1)
		{

			char lineHeader[128];
			// read the first word of the line
			int res = fscanf(file, "%s", lineHeader);
			if (res == EOF)
				break; // EOF = End Of File. Quit the loop.

			// else : parse lineHeader

			int warningHolder = 0;

			if (strcmp(lineHeader, "v") == 0)
			{
				glm::vec3 vertex;
				warningHolder = fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
				temp_vertices.push_back(vertex);
			}
			else if (strcmp(lineHeader, "vt") == 0)
			{
				glm::vec2 uv;
				warningHolder = fscanf(file, "%f %f\n", &uv.x, &uv.y);
				uv.y = -uv.y; // Invert V coordinate since we will only use DDS texture, which are inverted. Remove if you want to use TGA or BMP loaders.
				temp_uvs.push_back(uv);
			}
			else if (strcmp(lineHeader, "vn") == 0)
			{
				glm::vec3 normal;
				warningHolder = fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
				temp_normals.push_back(normal);
			}
			else if (strcmp(lineHeader, "f") == 0)
			{
				std::string vertex1, vertex2, vertex3;
				unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
				int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);
				if (matches != 9)
				{
					printf("File can't be read by our simple parser :-( Try exporting with other options\n");
					fclose(file);
					return false;
				}
				vertexIndices.push_back(vertexIndex[0]);
				vertexIndices.push_back(vertexIndex[1]);
				vertexIndices.push_back(vertexIndex[2]);
				uvIndices.push_back(uvIndex[0]);
				uvIndices.push_back(uvIndex[1]);
				uvIndices.push_back(uvIndex[2]);
				normalIndices.push_back(normalIndex[0]);
				normalIndices.push_back(normalIndex[1]);
				normalIndices.push_back(normalIndex[2]);
			}
			else
			{
				// Probably a comment, eat up the rest of the line
				char stupidBuffer[1000];
				char *realStupidBuffer;
				realStupidBuffer = fgets(stupidBuffer, 1000, file);
			}
		}

		// For each vertex of each triangle
		for (unsigned int i = 0; i < vertexIndices.size(); i++)
		{
			out_vertices.push_back(temp_vertices[vertexIndices[i] - 1]);
			out_uvs.push_back(temp_uvs[uvIndices[i] - 1]);
			out_normals.push_back(temp_normals[normalIndices[i] - 1]);
		}

		fclose(file);

        if (out_vertices.size() == 0)
        {
            Canis::Warning("Model file " + path + " was empty or error loading");
            return false;
        }
        else
        {
		    return true;
        }
	}

	bool LoadOBJ(std::string path,
		std::vector <Vertex> & _vertices,
		std::vector <unsigned int> & _indices
	){
		std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
		std::vector<glm::vec3> temp_vertices;
		std::vector<glm::vec2> temp_uvs;
		std::vector<glm::vec3> temp_normals;

		FILE *file = fopen(path.c_str(), "r");
		if (file == NULL)
		{
			FatalError("Can not open model: " + path);
		}

		while (1)
		{

			char lineHeader[128];
			// read the first word of the line
			int res = fscanf(file, "%s", lineHeader);
			if (res == EOF)
				break; // EOF = End Of File. Quit the loop.

			int warningHolder = 0;

			if (strcmp(lineHeader, "v") == 0)
			{
				glm::vec3 vertex;
				warningHolder = fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
				temp_vertices.push_back(vertex);
			}
			else if (strcmp(lineHeader, "vt") == 0)
			{
				glm::vec2 uv;
				warningHolder = fscanf(file, "%f %f\n", &uv.x, &uv.y);
				uv.y = -uv.y; // Invert V coordinate since we will only use DDS texture, which are inverted. Remove if you want to use TGA or BMP loaders.
				temp_uvs.push_back(uv);
			}
			else if (strcmp(lineHeader, "vn") == 0)
			{
				glm::vec3 normal;
				warningHolder = fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
				temp_normals.push_back(normal);
			}
			else if (strcmp(lineHeader, "f") == 0)
			{
				std::string vertex1, vertex2, vertex3;
				unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
				int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);
				if (matches != 9)
				{
					printf("File can't be read by our simple parser :-( Try exporting with other options\n");
					fclose(file);
					return false;
				}
				vertexIndices.push_back(vertexIndex[0]);
				vertexIndices.push_back(vertexIndex[1]);
				vertexIndices.push_back(vertexIndex[2]);
				uvIndices.push_back(uvIndex[0]);
				uvIndices.push_back(uvIndex[1]);
				uvIndices.push_back(uvIndex[2]);
				normalIndices.push_back(normalIndex[0]);
				normalIndices.push_back(normalIndex[1]);
				normalIndices.push_back(normalIndex[2]);
			}
			else
			{
				// Probably a comment, eat up the rest of the line
				char stupidBuffer[1000];
				char *realStupidBuffer;
				realStupidBuffer = fgets(stupidBuffer, 1000, file);
			}
		}

		Vertex vertex;

		// For each vertex of each triangle
		for (unsigned int i = 0; i < vertexIndices.size(); i++)
		{
			bool found = false;

            for (int v = 0; v < _indices.size(); v++)
            {
                if (vertexIndices[i] == vertexIndices[v] &&
                    uvIndices[i] == uvIndices[v] &&
                    normalIndices[i] == normalIndices[v])
                {
                    _indices.push_back(v);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
				_indices.push_back(_vertices.size());
				vertex.position = temp_vertices[vertexIndices[i] - 1];
				vertex.texCoords = temp_uvs[uvIndices[i] - 1];
				vertex.normal = temp_normals[normalIndices[i] - 1];
				_vertices.push_back(vertex);
            }
		}

		fclose(file);
        
        if (_vertices.size() == 0)
        {
            Canis::Warning("Model file " + path + " was empty or error loading");
            return false;
        }
        else
        {
		    return true;
        }

	}*/

} // end of Canis namespace
