#include <Canis/AssetManager.hpp>
#include <Canis/AssetPipeline.hpp>
#include <Canis/LightingBake.hpp>
#include <Canis/UnityImport.hpp>
#include <Canis/VisibilityBake.hpp>
#include <fstream>
#include <iostream>
int main(int argc, char** argv)
{
    try
    {
        if (argc == 5 && std::string(argv[1]) == "import-unity")
        {
            auto r = Canis::ImportUnityScene(argv[2], argv[3], argv[4],
                                             [](const std::string& s) { std::cout << s << std::endl; });
            std::cout << "CANIS_UNITY_IMPORT_PASS models=" << r.models << " prefabs=" << r.prefabs
                      << " entities=" << r.entities << " renderers=" << r.renderers << " scene=" << r.scene
                      << '\n';
            return 0;
        }
        if (argc >= 4 &&
            (std::string(argv[1]) == "bake-lighting" || std::string(argv[1]) == "bake-lighting-scenarios"))
        {
            // Build the native metadata index for material overrides without opening
            // a window.
            for (auto it = std::filesystem::recursive_directory_iterator("assets");
                 it != std::filesystem::recursive_directory_iterator(); ++it)
                if (it->is_regular_file() && it->path().extension() == ".meta")
                {
                    auto p = it->path();
                    p.replace_extension();
                    Canis::AssetManager::LoadMetaFile(p.string());
                }
            auto doc = YAML::LoadFile(argv[2]);
            Canis::LightingBakeOptions opts;
            if (argc > 4)
                opts.vertexSpacing = std::stof(argv[4]);
            if (argc > 5)
                opts.indirectSamples = std::stoi(argv[5]);
            const bool scenarios = std::string(argv[1]) == "bake-lighting-scenarios";
            Canis::LightingBakeResult result;
            if (scenarios)
            {
                std::vector<Canis::LightingScenario> definitions;
                if (doc["Environment"]["LightingScenarios"])
                    for (const auto& scenario : doc["Environment"]["LightingScenarios"])
                        definitions.push_back(Canis::DeserializeLightingScenario(scenario));
                for (int i = 6; i < argc; ++i)
                    opts.selectedScenarioIds.push_back(argv[i]);
                result =
                    Canis::BakeLightingScenarios(doc, argv[3], definitions, opts,
                                                 [](const std::string& s) { std::cout << s << std::endl; });
            }
            else
                result = Canis::BakeSceneLighting(doc, argv[3], opts,
                                                  [](const std::string& s) { std::cout << s << std::endl; });
            auto output = std::filesystem::path(argv[2]);
            output.replace_filename(output.stem().string() +
                                    (scenarios ? "-scenarios.scene" : "-baked.scene"));
            std::ofstream f(output);
            f << result.scene;
            f.close();
            Canis::Pipeline::WriteMeta(output, "SCENE");
            std::cout << (scenarios ? "CANIS_LIGHTING_SCENARIO_BAKE_PASS models="
                                    : "CANIS_LIGHTING_BAKE_PASS models=")
                      << result.models.size() << " vertices=" << result.vertices
                      << " lights=" << result.lights << " scene=" << output << '\n';
            return 0;
        }
        if (argc >= 4 && std::string(argv[1]) == "bake-visibility")
        {
            Canis::VisibilityBakeOptions options;
            if (argc > 4)
                options.chunkSize = std::stof(argv[4]);
            if (argc > 5)
                options.cellSize = std::stof(argv[5]);
            if (argc > 8)
                options.minimum = {std::stof(argv[6]), std::stof(argv[7]), std::stof(argv[8])};
            if (argc > 11)
                options.dimensions = {std::stoi(argv[9]), std::stoi(argv[10]), std::stoi(argv[11])};
            Canis::BakeStaticVisibility(argv[2], argv[3], options);
            std::cout << "CANIS_VISIBILITY_BAKE_PASS\n";
            return 0;
        }
        std::cerr << "Usage: canis-asset-tool import-unity <asset-root> "
                     "<scene.unity> <destination>\n";
        std::cerr << "       canis-asset-tool bake-lighting <scene> <directory> "
                     "[spacing] [samples]\n";
        std::cerr << "       canis-asset-tool bake-lighting-scenarios <scene> "
                     "<directory> [spacing] [samples] [scenario-id ...]\n";
        return 2;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Native asset pipeline: " << e.what() << '\n';
        return 1;
    }
}
