#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Components.hpp>
#include <Canis/Shader.hpp>
#include <Canis/OpenGL.hpp>
#include <stdexcept>
#include <Canis/AssetPipeline.hpp>
#include <Canis/AssetManager.hpp>
#include <fstream>
#include <cstring>

namespace Canis
{
    void MeshRenderer3DSystem::DestroyLightingProbes()
    {
        if (!m_probeTextures.empty())
            glDeleteTextures(static_cast<GLsizei>(m_probeTextures.size()), m_probeTextures.data());
        m_probeTextures.clear();
        m_probeIrradiance.clear();
    }
    static uint64_t ProbeSceneFingerprint(entt::registry& registry, const Scene& scene)
    {
        uint64_t hash = 1469598103934665603ULL;
        auto bytes = [&](const void* pointer, size_t size)
        {
            auto* p = static_cast<const unsigned char*>(pointer);
            for (size_t i = 0; i < size; i++)
            {
                hash ^= p[i];
                hash *= 1099511628211ULL;
            }
        };
        auto fileStamp = [&](const std::string& path)
        {
            bytes(path.data(), path.size());
            std::error_code error;
            auto size = std::filesystem::file_size(path, error);
            bytes(&size, sizeof(size));
            auto time = std::filesystem::last_write_time(path, error).time_since_epoch().count();
            bytes(&time, sizeof(time));
        };
        // Capture filtering changes radiance even when all source assets stay
        // unchanged. Rebuild caches captured with nearest-sampled normal maps.
        constexpr uint32_t captureRevision = 2;
        bytes(&captureRevision, sizeof(captureRevision));
        const auto ambient = scene.GetEnvironmentAmbientLight();
        bytes(&ambient, sizeof(ambient));
        const float ambientIntensity = scene.GetEnvironmentAmbientLightIntensity();
        bytes(&ambientIntensity, sizeof(ambientIntensity));
        const auto skyPath = AssetManager::GetPath(scene.GetEnvironmentSkyboxUUID());
        fileStamp(skyPath);
        fileStamp("assets/shaders/model3d.vs");
        fileStamp("assets/shaders/model3d.fs");
        // Texture/material edits also change captured radiance. This scan occurs
        // only when loading or recapturing probes, never during ordinary frames.
        if (std::filesystem::exists("assets"))
            for (const auto& entry : std::filesystem::recursive_directory_iterator("assets"))
            {
                auto extension = entry.path().extension().string();
                if (entry.is_regular_file() && (extension == ".material" || extension == ".png" ||
                                                extension == ".jpg" || extension == ".shader"))
                    fileStamp(entry.path().string());
            }
        for (auto handle : registry.view<Transform, Model>())
        {
            auto& t = registry.get<Transform>(handle);
            auto& m = registry.get<Model>(handle);
            if (!m.staticModel || !t.IsActiveInHierarchy())
                continue;
            auto matrix = t.GetModelMatrix();
            bytes(&matrix, sizeof(matrix));
            auto* asset = AssetManager::GetModel(m.modelId);
            if (!asset)
                continue;
            auto path = asset->GetPath();
            bytes(path.data(), path.size());
            std::error_code error;
            auto size = std::filesystem::file_size(path, error);
            bytes(&size, sizeof(size));
            auto modified = std::filesystem::last_write_time(path, error).time_since_epoch().count();
            bytes(&modified, sizeof(modified));
            auto bin = std::filesystem::path(path);
            bin.replace_extension(".bin");
            if (std::filesystem::exists(bin))
            {
                auto time = std::filesystem::last_write_time(bin, error).time_since_epoch().count();
                bytes(&time, sizeof(time));
            }
            bytes(&m.color, sizeof(m.color));
            if (const auto* material = registry.try_get<Material>(handle))
            {
                bytes(&material->color, sizeof(material->color));
                if (material->materialId >= 0)
                    if (auto* asset = AssetManager::GetMaterial(material->materialId))
                        fileStamp(AssetManager::GetPath(material->materialId));
                for (auto id : material->materialIds)
                    if (id >= 0)
                        if (auto* asset = AssetManager::GetMaterial(id))
                            fileStamp(AssetManager::GetPath(id));
            }
        }
        for (auto handle : registry.view<DirectionalLight>())
        {
            const auto& light = registry.get<DirectionalLight>(handle);
            bytes(&light.direction, sizeof(light.direction));
            bytes(&light.color, sizeof(light.color));
            bytes(&light.intensity, sizeof(light.intensity));
            bytes(&light.enabled, sizeof(light.enabled));
        }
        for (auto handle : registry.view<PointLight, Transform>())
        {
            auto light = registry.get<PointLight>(handle);
            auto p = registry.get<Transform>(handle).GetGlobalPosition();
            bytes(&p, sizeof(p));
            bytes(&light.color, sizeof(light.color));
            bytes(&light.intensity, sizeof(light.intensity));
            bytes(&light.range, sizeof(light.range));
            bytes(&light.enabled, sizeof(light.enabled));
        }
        return hash;
    }
    void MeshRenderer3DSystem::UpdateLightingProbes(entt::registry& registry)
    {
        std::vector<LightingProbe> settings;
        for (auto p : scene->GetLightingProbes())
        {
            if (settings.size() == MaxLightingProbes)
                break;
            p.Sanitize();
            if (p.enabled)
                settings.push_back(p);
        }
        if (!m_probesDirty && settings == m_probeSettings)
            return;
        DestroyLightingProbes();
        m_probeSettings = settings;
        m_probesDirty = false;
        if (settings.empty())
            return;
        // One synchronous capture on load or explicit recapture; never once per eye.
        constexpr int size = 64;
        GLint drawFbo, readFbo, viewport[4], renderbuffer, active, pack;
        GLfloat clear[4];
        GLboolean depthMask;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        const bool scissor = glIsEnabled(GL_SCISSOR_TEST);
        const bool stereoFrame = m_stereoFrame;
        const int stereoPass = m_stereoPass;
        GLuint fbo = 0, depth = 0;
        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(1, &depth);
        auto restore = [&]()
        {
            m_capturingProbe = false;
            m_stereoFrame = stereoFrame;
            m_stereoPass = stereoPass;
            m_stereoPrepared = false;
            m_stereoShadowReady = false;
            InvalidateStaticShadowCache();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            glClearColor(clear[0], clear[1], clear[2], clear[3]);
            glDepthMask(depthMask);
            if (scissor)
                glEnable(GL_SCISSOR_TEST);
            else
                glDisable(GL_SCISSOR_TEST);
            glPixelStorei(GL_PACK_ALIGNMENT, pack);
            glActiveTexture(active);
            glDeleteFramebuffers(1, &fbo);
            glDeleteRenderbuffers(1, &depth);
        };
        try
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
            glDisable(GL_SCISSOR_TEST);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            m_capturingProbe = true;
            m_stereoFrame = false;
            m_stereoPass = 0;
            const Vector3 directions[] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                          {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
            const Vector3 ups[] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
            std::vector<float> pixels(size * size * 4);
            const bool forceCapture = m_forceProbeCapture;
            m_forceProbeCapture = false;
            const uint64_t sceneFingerprint = ProbeSceneFingerprint(registry, *scene);
            for (const auto& probe : settings)
            {
                GLuint texture = 0;
                glGenTextures(1, &texture);
                m_probeTextures.push_back(texture);
                glActiveTexture(GL_TEXTURE8);
                glBindSampler(8, 0);
                glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
                for (int face = 0; face < 6; ++face)
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F, size, size, 0, GL_RGBA,
                                 GL_FLOAT, nullptr);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                std::array<Vector3, 9> sh{};
                float totalWeight = 0;
                std::vector<float> cube(size * size * 4 * 6);
                uint64_t fingerprint = sceneFingerprint;
                auto mix = [&](float value)
                {
                    uint32_t word;
                    std::memcpy(&word, &value, 4);
                    fingerprint ^= word;
                    fingerprint *= 1099511628211ULL;
                };
                for (int i = 0; i < 3; i++)
                {
                    mix(probe.position[i]);
                    mix(probe.halfExtents[i]);
                }
                mix(probe.nearClip);
                mix(probe.farClip);
                bool cached = false;
                if (!forceCapture && !probe.cachePath.empty())
                {
                    std::ifstream file(probe.cachePath, std::ios::binary);
                    uint64_t header[3]{};
                    if (file.read(reinterpret_cast<char*>(header), sizeof(header)) &&
                        header[0] == 0x43414e4953505231ULL && header[1] == fingerprint && header[2] == size)
                    {
                        file.read(reinterpret_cast<char*>(cube.data()), cube.size() * sizeof(float));
                        file.read(reinterpret_cast<char*>(sh.data()), sizeof(sh));
                        cached = bool(file);
                    }
                }
                if (cached)
                {
                    for (int face = 0; face < 6; face++)
                        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, 0, 0, size, size, GL_RGBA,
                                        GL_FLOAT, cube.data() + face * pixels.size());
                    m_probeIrradiance.push_back(sh);
                    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
                    continue;
                }
                m_probePosition = probe.position;
                m_probeNear = probe.nearClip;
                m_probeFar = probe.farClip;
                m_probeProjection = glm::perspective(glm::radians(90.f), 1.f, m_probeNear, m_probeFar);
                for (int face = 0; face < 6; ++face)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, texture, 0);
                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                        throw std::runtime_error("Lighting probe HDR framebuffer is incomplete");
                    glViewport(0, 0, size, size);
                    glDepthMask(GL_TRUE);
                    glClearColor(0, 0, 0, 1);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    m_probeView = glm::lookAt(probe.position, probe.position + directions[face], ups[face]);
                    Update(registry, 0);
                    glReadPixels(0, 0, size, size, GL_RGBA, GL_FLOAT, pixels.data());
                    std::copy(pixels.begin(), pixels.end(), cube.begin() + face * pixels.size());
                    for (int y = 0; y < size; ++y)
                        for (int x = 0; x < size; ++x)
                        {
                            const float u = 2 * (x + .5f) / size - 1, v = 2 * (y + .5f) / size - 1;
                            const float weight = 1 / std::pow(1 + u * u + v * v, 1.5f);
                            const auto basis = ProbeSHBasis(ProbeTexelDirection(face, u, v));
                            const int i = (y * size + x) * 4;
                            Vector3 radiance(pixels[i], pixels[i + 1], pixels[i + 2]);
                            for (int c = 0; c < 3; ++c)
                                if (!std::isfinite(radiance[c]))
                                    radiance[c] = 0;
                            radiance = glm::max(radiance, Vector3(0));
                            for (int j = 0; j < 9; ++j)
                                sh[j] += radiance * (basis[j] * weight);
                            totalWeight += weight;
                        }
                }
                // Cosine convolution divided by PI: irradiance ready to multiply by albedo.
                for (int j = 0; j < 9; ++j)
                    sh[j] *= (4 * glm::pi<float>() / totalWeight) * (j == 0 ? 1.f : j < 4 ? 2.f / 3.f : .25f);
                if (!probe.cachePath.empty())
                {
                    std::filesystem::create_directories(std::filesystem::path(probe.cachePath).parent_path());
                    auto temp = probe.cachePath + ".tmp";
                    std::ofstream file(temp, std::ios::binary);
                    uint64_t header[] = {0x43414e4953505231ULL, fingerprint, size};
                    file.write(reinterpret_cast<const char*>(header), sizeof(header));
                    file.write(reinterpret_cast<const char*>(cube.data()), cube.size() * sizeof(float));
                    file.write(reinterpret_cast<const char*>(sh.data()), sizeof(sh));
                    file.close();
                    if (file)
                        std::filesystem::rename(temp, probe.cachePath);
                    else
                        std::filesystem::remove(temp);
                }
                m_probeIrradiance.push_back(sh);
                glActiveTexture(GL_TEXTURE8);
                glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            }
            restore();
        }
        catch (...)
        {
            restore();
            DestroyLightingProbes();
            m_probesDirty = true;
            throw;
        }
    }
    void MeshRenderer3DSystem::BindLightingProbes(Shader& shader)
    {
        shader.SetBool("canisProbeCapture", m_capturingProbe);
        shader.SetInt("canisProbeCount", m_capturingProbe ? 0 : static_cast<int>(m_probeTextures.size()));
        for (int i = 0; i < MaxLightingProbes; ++i)
        {
            const std::string index = std::to_string(i);
            shader.SetInt("canisProbeMap" + index, 8 + i);
            glActiveTexture(GL_TEXTURE8 + i);
            glBindSampler(8 + i, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP,
                          !m_capturingProbe && i < int(m_probeTextures.size()) ? m_probeTextures[i] : 0);
            if (m_capturingProbe || i >= int(m_probeIrradiance.size()))
                continue;
            const auto& p = m_probeSettings[i];
            shader.SetVec3("canisProbePosition[" + index + "]", p.position);
            shader.SetVec3("canisProbeExtent[" + index + "]", p.halfExtents);
            shader.SetVec3("canisProbeParams[" + index + "]",
                           Vector3(p.blendDistance, p.diffuseIntensity, p.specularIntensity));
            for (int j = 0; j < 9; ++j)
                shader.SetVec3("canisProbeSH[" + std::to_string(i * 9 + j) + "]", m_probeIrradiance[i][j]);
        }
        glActiveTexture(GL_TEXTURE0);
    }
} // namespace Canis
