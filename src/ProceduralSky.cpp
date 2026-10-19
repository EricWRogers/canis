#include <Canis/AssetPipeline.hpp>
#include <Canis/ProceduralSky.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stb_image_write.h>
#include <stdexcept>
#include <vector>

namespace Canis {
namespace {
float Smooth(float minimum, float maximum, float value) {
    const float t = std::clamp((value - minimum) / (maximum - minimum), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

float AngularGlow(Vector3 direction, Vector3 light, float radius) {
    // Angular distance keeps the halo round through cube edges and poles.
    const float angle = std::acos(std::clamp(glm::dot(direction, light), -1.f, 1.f));
    return std::exp(-(angle * angle) / (radius * radius));
}

std::string AssetPath(const std::filesystem::path &path) {
    return std::filesystem::absolute(path)
        .lexically_normal()
        .lexically_relative(std::filesystem::current_path())
        .generic_string();
}
} // namespace

Vector3 SkyboxFaceDirection(int face, float u, float v) {
    const float x = 2.f * u - 1.f, y = 2.f * v - 1.f;
    Vector3 direction;
    switch (face) {
    case 0:
        direction = {1, -y, -x};
        break;
    case 1:
        direction = {-1, -y, x};
        break;
    case 2:
        direction = {x, 1, y};
        break;
    case 3:
        direction = {x, -1, -y};
        break;
    case 4:
        direction = {x, -y, 1};
        break;
    case 5:
        direction = {-x, -y, -1};
        break;
    default:
        throw std::invalid_argument("Skybox face must be between zero and five");
    }
    return glm::normalize(direction);
}

Vector3 EvaluateLevelOneSky(LevelOneSky sky, Vector3 direction) {
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z) ||
        glm::dot(direction, direction) < 1.e-12f)
        throw std::invalid_argument("Sky direction must be finite and nonzero");
    direction = glm::normalize(direction);
    const float elevation = std::max(direction.y, 0.f);
    const float horizon = std::exp(-elevation / .13f);
    const float belowHorizon = Smooth(-.20f, .02f, direction.y);
    if (sky == LevelOneSky::Dusk) {
        // Incoming directional light is (-.82,-.14,-.55). The visible low
        // sun therefore sits in the opposite direction above the horizon.
        const Vector3 sun = glm::normalize(Vector3(.82f, .14f, .55f));
        const float facingSun = std::pow(std::max(glm::dot(direction, sun), 0.f), 3.f);
        Vector3 color = glm::mix(Vector3(.024f, .048f, .115f), Vector3(.10f, .105f, .13f), horizon);
        color += Vector3(.32f, .085f, .018f) * horizon * (.22f + .78f * facingSun);
        color += Vector3(.19f, .065f, .012f) * AngularGlow(direction, sun, .24f);
        color += Vector3(.33f, .14f, .035f) * AngularGlow(direction, sun, .045f);
        const float sunAngle = std::acos(std::clamp(glm::dot(direction, sun), -1.f, 1.f));
        color += Vector3(.65f, .48f, .24f) * (1.f - Smooth(.0075f, .0095f, sunAngle));
        return glm::mix(Vector3(.013f, .018f, .030f), color, belowHorizon);
    }
    const Vector3 moon = glm::normalize(Vector3(-.55f, .72f, -.42f));
    Vector3 color = glm::mix(Vector3(.0025f, .0050f, .0110f), Vector3(.010f, .018f, .031f), horizon);
    color += Vector3(.006f, .012f, .025f) * AngularGlow(direction, moon, .24f);
    const float moonAngle = std::acos(std::clamp(glm::dot(direction, moon), -1.f, 1.f));
    color += Vector3(.10f, .14f, .21f) * (1.f - Smooth(.007f, .009f, moonAngle));
    return glm::mix(Vector3(.0020f, .0035f, .0060f), color, belowHorizon);
}

void GenerateLevelOneSkies(const std::filesystem::path &directory, int resolution) {
    if (resolution < 16 || resolution > 4096)
        throw std::invalid_argument("Skybox face resolution must be between 16 and 4096");
    const auto textures = directory / "textures";
    std::filesystem::create_directories(textures);
    static const char *faceNames[] = {"right", "left", "top", "bottom", "front", "back"};
    std::vector<unsigned char> pixels(size_t(resolution) * resolution * 3);
    for (const auto sky : {LevelOneSky::Dusk, LevelOneSky::StormNight}) {
        const std::string name = sky == LevelOneSky::Dusk ? "dusk" : "storm-night";
        std::ofstream definition(directory / (name + ".skybox"));
        if (!definition)
            throw std::runtime_error("Cannot write generated skybox definition");
        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < resolution; ++y)
                for (int x = 0; x < resolution; ++x) {
                    const Vector3 color = EvaluateLevelOneSky(
                        sky, SkyboxFaceDirection(face, (x + .5f) / resolution, (y + .5f) / resolution));
                    for (int c = 0; c < 3; ++c)
                        pixels[(size_t(y) * resolution + x) * 3 + c] = static_cast<unsigned char>(
                            std::lround(std::pow(std::clamp(color[c], 0.f, 1.f), 1.f / 2.2f) * 255.f));
                }
            const auto image = textures / (name + "-" + faceNames[face] + ".png");
            if (!stbi_write_png(image.string().c_str(), resolution, resolution, 3, pixels.data(),
                                resolution * 3))
                throw std::runtime_error("Cannot write generated skybox face: " + image.string());
            Pipeline::WriteMeta(image, "TEXTURE");
            definition << faceNames[face] << ":\n  path: " << AssetPath(image) << '\n';
        }
        definition.close();
        if (!definition)
            throw std::runtime_error("Cannot finish generated skybox definition");
        Pipeline::WriteMeta(directory / (name + ".skybox"), "SKYBOX");
    }
}
} // namespace Canis
