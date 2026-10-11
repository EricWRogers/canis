#pragma once
#include <Canis/Math.hpp>
#include <filesystem>
namespace Canis
{
    bool TriangleOccludesCell(Vector3 _eyeMin, Vector3 _eyeMax, Vector3 _targetMin, Vector3 _targetMax,
                              Vector3 _a, Vector3 _b, Vector3 _c, float _epsilon = .002f);
    struct VisibilityBakeOptions
    {
        float chunkSize = 32, cellSize = 8;
        Vector3 minimum{0};
        glm::ivec3 dimensions{1};
        int occluders = 96;
    };
    void BakeStaticVisibility(const std::filesystem::path& _source, const std::filesystem::path& _output,
                              const VisibilityBakeOptions& _options = {});
} // namespace Canis
