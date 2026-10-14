#pragma once
#include <vector>

#include <string>
#include <filesystem>
#include <memory>
#include <unordered_map>

#include "Data/GLTexture.hpp"
//#include "Data/Vertex.hpp"


namespace Canis
{
    struct AssetDirectorySnapshot
    {
        std::vector<std::string> paths, added, changed, removed;
        std::unordered_map<std::string, std::filesystem::file_time_type> writeTimes;
    };
    // Filesystem-only: workers never access the asset library or graphics context.
    std::shared_ptr<const AssetDirectorySnapshot> ReadAssetDirectorySnapshot(
        const std::string& folder, std::shared_ptr<const AssetDirectorySnapshot> previous = {});
    struct DecodedImage
    {
        int width = 0, height = 0;
        std::vector<unsigned char> pixels;
        std::string error;
    };
    // CPU-only, thread-local stb settings; safe on asset workers.
    DecodedImage DecodeImageRGBA(const std::string &path);
    size_t ImageRGBAByteSize(const std::string &path);
    // Requires the rendering thread's current GL context.
    GLTexture UploadDecodedImage(const DecodedImage &image);
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
