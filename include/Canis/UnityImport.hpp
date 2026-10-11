#pragma once
#include <filesystem>
#include <string>
#include <functional>
namespace Canis
{
    struct UnityImportResult
    {
        int prefabs = 0, models = 0, renderers = 0, entities = 0, lights = 0;
        std::filesystem::path scene;
    };
    UnityImportResult ImportUnityScene(const std::filesystem::path& _assetRoot,
                                       const std::filesystem::path& _scene,
                                       const std::filesystem::path& _destination,
                                       const std::function<void(const std::string&)>& _progress = {});
} // namespace Canis
