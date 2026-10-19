#include <Canis/ProceduralSky.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <stb_image.h>
#include <vector>
#include <yaml-cpp/yaml.h>

using namespace Canis;
namespace {
void Check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::vector<char> Read(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
void Edge(Vector3 a, Vector3 b) {
    Check(glm::length(a - b) < 1.e-6f, "Cubemap edges have inconsistent directions");
    for (auto sky : {LevelOneSky::Dusk, LevelOneSky::StormNight})
        Check(glm::length(EvaluateLevelOneSky(sky, a) - EvaluateLevelOneSky(sky, b)) < 1.e-5f,
              "Cubemap radiance is discontinuous at a face boundary");
}
} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "canis-procedural-sky-tests";
    try {
        // Check all twelve cube edges, including the reversed coordinate order
        // at poles. This catches a rotated or vertically flipped image face.
        for (int i = 0; i <= 32; ++i) {
            const float t = i / 32.f;
            Edge(SkyboxFaceDirection(0, 0, t), SkyboxFaceDirection(4, 1, t));
            Edge(SkyboxFaceDirection(0, 1, t), SkyboxFaceDirection(5, 0, t));
            Edge(SkyboxFaceDirection(1, 0, t), SkyboxFaceDirection(5, 1, t));
            Edge(SkyboxFaceDirection(1, 1, t), SkyboxFaceDirection(4, 0, t));
            Edge(SkyboxFaceDirection(0, t, 0), SkyboxFaceDirection(2, 1, 1 - t));
            Edge(SkyboxFaceDirection(0, t, 1), SkyboxFaceDirection(3, 1, t));
            Edge(SkyboxFaceDirection(1, t, 0), SkyboxFaceDirection(2, 0, t));
            Edge(SkyboxFaceDirection(1, t, 1), SkyboxFaceDirection(3, 0, 1 - t));
            Edge(SkyboxFaceDirection(4, t, 0), SkyboxFaceDirection(2, t, 1));
            Edge(SkyboxFaceDirection(4, t, 1), SkyboxFaceDirection(3, t, 0));
            Edge(SkyboxFaceDirection(5, t, 0), SkyboxFaceDirection(2, 1 - t, 0));
            Edge(SkyboxFaceDirection(5, t, 1), SkyboxFaceDirection(3, 1 - t, 1));
        }
        const auto zenith = Vector3(0, 1, 0);
        Check(glm::length(EvaluateLevelOneSky(LevelOneSky::Dusk, zenith)) >
                  glm::length(EvaluateLevelOneSky(LevelOneSky::StormNight, zenith)) * 5,
              "Dusk and storm night should have visibly distinct exposure");
        const auto sun = glm::normalize(Vector3(.82f, .14f, .55f));
        const auto duskSun = EvaluateLevelOneSky(LevelOneSky::Dusk, sun);
        Check(duskSun.r > duskSun.b && duskSun.r > .7f, "Dusk sun must be warm and above the horizon");

        GenerateLevelOneSkies(directory, 32);
        std::vector<std::pair<std::filesystem::path, std::vector<char>>> firstGeneration;
        for (const auto *name : {"dusk", "storm-night"}) {
            const auto skybox = directory / (std::string(name) + ".skybox");
            const auto definition = YAML::LoadFile(skybox.string());
            Check(definition.size() == 6, "Generated skybox does not define all six faces");
            for (const auto *face : {"right", "left", "top", "bottom", "front", "back"}) {
                const auto image = std::filesystem::path(definition[face]["path"].as<std::string>());
                int width = 0, height = 0, channels = 0;
                auto *pixels = stbi_load(image.string().c_str(), &width, &height, &channels, 3);
                Check(pixels && width == 32 && height == 32 && channels == 3,
                      "Generated skybox image is invalid or has an incorrect resolution");
                stbi_image_free(pixels);
                Check(std::filesystem::exists(image.string() + ".meta"),
                      "Generated texture metadata missing");
                firstGeneration.emplace_back(image, Read(image));
            }
            Check(std::filesystem::exists(skybox.string() + ".meta"), "Generated skybox metadata missing");
        }
        GenerateLevelOneSkies(directory, 32);
        for (const auto &[image, bytes] : firstGeneration)
            Check(Read(image) == bytes, "Procedural sky regeneration must be deterministic");
        bool rejected = false;
        try {
            GenerateLevelOneSkies(directory, 0);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        Check(rejected, "Zero-resolution cubemap accepted");
        rejected = false;
        try {
            EvaluateLevelOneSky(LevelOneSky::Dusk, {std::numeric_limits<float>::quiet_NaN(), 1, 0});
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        Check(rejected, "Nonfinite sky direction accepted");
        std::filesystem::remove_all(directory);
        std::cout << "CANIS_PROCEDURAL_SKY_TESTS_PASS edges=12 skies=2 faces=12 deterministic=1\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
