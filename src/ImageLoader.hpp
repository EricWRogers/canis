#pragma once
#include <string>

#include "GLTexture.h"
#include "picoPNG.h"
#include "IOManager.hpp"
#include "Debug.h"

namespace canis
{

	class ImageLoader
	{
	public:
		static GLTexture loadPNG(std::string filePath);
	};
} // end of canis namespace