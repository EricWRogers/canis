#pragma once
#include <Canis/Asset.hpp> // MaterialFields
#include <Canis/AssetHandle.hpp>

namespace Canis
{
	struct Mesh
	{
		MeshHandle modelHandle;
		bool castShadow = true;
		int material = -1;
		bool useInstance = false;
		bool castDepth = true;
		bool animatedModel = false;
		bool overrideMaterialField = false;
		MaterialFields overrideMaterialFields; // this could be a seperate component
	};
} // end of Canis namespace