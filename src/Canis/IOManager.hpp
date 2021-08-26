#pragma once
#include <vector>
#include <fstream>

namespace Canis
{
	class IOManager
	{
	public:
		static bool readFileToBuffer(std::string filePath, std::vector<unsigned char> &buffer);
	};
} // end of Canis namespace