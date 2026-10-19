#pragma once

#include <Canis/Math.hpp>
#include <filesystem>

namespace Canis {
enum class LevelOneSky { Dusk, StormNight };

// OpenGL cubemap face order: +X, -X, +Y, -Y, +Z, -Z. v=0 is the first
// uploaded image row, matching SkyboxAsset's non-flipped texture upload.
Vector3 SkyboxFaceDirection(int face, float u, float v);

// Linear radiance, independent of any cube face. Clouds remain a separate
// three-dimensional scene effect instead of being painted into these skies.
Vector3 EvaluateLevelOneSky(LevelOneSky sky, Vector3 direction);

// Emits deterministic, gamma-encoded PNG faces, .skybox definitions, and
// metadata. Does not require an OpenGL context or a running editor.
void GenerateLevelOneSkies(const std::filesystem::path &directory, int resolution = 512);
} // namespace Canis
