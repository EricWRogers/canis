#pragma once
#include <Canis/AssetHandle.hpp>

namespace Canis
{
	struct MeshComponent
	{
		MeshHandle modelHandle;
		bool castShadow = false;
		int material = -1;
		bool useInstance = false;
		bool castDepth = true;
		bool animatedModel = false;
	};
} // end of Canis namespace