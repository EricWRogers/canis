#pragma once
#include <string>

#include "GLTexture.h"
#include "picoPNG.h"
#include "IOManager.hpp"
#include "Debug.h"

namespace Canis
{

	class ImageLoader
	{
	public:
		static GLTexture loadPNG(std::string filePath);
	};
} // end of Canis namespace