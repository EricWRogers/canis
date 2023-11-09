#pragma once

namespace Canis
{
	struct MeshComponent
	{
		int id = 0;
		bool castShadow = false;
		int material = 0;
		bool useInstance = false;
	};
} // end of Canis namespace