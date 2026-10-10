#pragma once
#include <Canis/Math.hpp>
#include <Canis/Entity.hpp>
#include <memory>
#include <string>
#include <vector>

namespace Canis
{
    struct LODLevel
    {
        std::string modelPath;
        int modelId = -1;
        float screenHeight = 0;
    };
    struct LODChain
    {
        std::string sourcePath;
        int sourceId = -1;
        std::vector<LODLevel> levels;
    };
    struct LODProfile
    {
        std::string path;
        std::vector<LODChain> chains;
        uint64_t revision = 0;
        bool Save() const;
    };
    struct LODGroup
    {
        static constexpr const char* ScriptName = "Canis::LODGroup";
        LODGroup() = default;
        explicit LODGroup(Entity& owner) : entity(&owner) {}
        void Create() {}
        Entity* entity = nullptr;
        bool enabled = true;
        std::string profilePath;
        float bias = 1;
        float hysteresis = .15f;
        float cullHeight = 0;
        int shadowBias = 1;
        int forcedLevel = -1;
        bool debugColors = false;
        // Diagnostics from the most recently rendered view; never serialized.
        int activeLevel = 0;
        float projectedHeight = 0;
    };
    std::shared_ptr<LODProfile> GetLODProfile(const std::string& path, bool reload = false);
    int SelectLOD(const std::vector<LODLevel>& levels, float height, int previous, float hysteresis);
    float ProjectedLODHeight(const Vector3& minimum, const Vector3& maximum,
        const Matrix4& model, const Matrix4& view, const Matrix4& projection);
}
