#pragma once
#include <vector>
#include <fstream>

namespace canis
{
	class IOManager
	{
	public:
		static bool readFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer);
	};
} // end of canis namespace