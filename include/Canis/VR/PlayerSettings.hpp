#pragma once
#include <string>
namespace Canis::VR
{
    struct PlayerSettings
    {
        unsigned dominantHand = 1; // 0 left, 1 right; other hand teleports
        float localFloorHeight = 1.65f;
        float heightOffset = 0.0f; // Accessibility lift for seated play, metres.
        float snapDegrees = 30.0f;
        float teleportRange = 3.0f;
        float grabReach = 0.22f;
        float fadeOutSeconds = 0.08f;
        float blackSeconds = 0.03f;
        float fadeInSeconds = 0.12f;
    };
    // Missing files leave defaults/current values intact. Invalid files leave the
    // entire previous value intact, returning an actionable error.
    bool LoadPlayerSettings(const std::string& path, PlayerSettings& settings, std::string& error);
    bool ValidatePlayerSettings(const PlayerSettings& settings, std::string& error);
}
