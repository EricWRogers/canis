#pragma once

namespace Canis
{
	struct MeshComponent
	{
		int id = 0;
		unsigned int vao = 0;
		int size = 0;
		bool castShadow = false;
	};
} // end of Canis namespace