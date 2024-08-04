#pragma once
#include <Canis/Asset.hpp> // MaterialFields
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
		bool overrideMaterialField = false;
		MaterialFields overrideMaterialFields; // this could be a seperate component
	};
} // end of Canis namespace