#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Components.hpp>
#include <Canis/Shader.hpp>
#include <Canis/OpenGL.hpp>
#include <stdexcept>
#include <Canis/AssetPipeline.hpp>
#include <Canis/LightingBake.hpp>
#include <Canis/Debug.hpp>
#include <Canis/AssetManager.hpp>
#include <fstream>
#include <cstring>
#include <set>
#include <functional>

namespace Canis
{
    static bool ProbeArraysAvailable()
    {
#if defined(__EMSCRIPTEN__)
        return false;
#else
        return GLEW_VERSION_4_0 || GLEW_ARB_texture_cube_map_array;
#endif
    }
    void MeshRenderer3DSystem::DestroyLightingProbes()
    {
#if defined(__APPLE__)
        if (m_emptyProbeArray) glDeleteTextures(1, &m_emptyProbeArray);
        m_emptyProbeArray = 0;
#endif
        if (!m_probeTextures.empty())
            glDeleteTextures(static_cast<GLsizei>(m_probeTextures.size()), m_probeTextures.data());
        m_probeTextures.clear();
        m_probeIrradiance.clear();
        if(m_scenarioProbeArray)glDeleteTextures(1,&m_scenarioProbeArray);
        m_scenarioProbeArray=0;m_scenariosReady=false;m_scenarioProbeRepairable=false;
        m_scenarioProbeIrradiance.clear();
        m_scenarioReadySignatures.clear();
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
        std::set<std::string> dependencies;
        std::function<void(const std::string&)> dependency;
        std::function<void(const YAML::Node&)> references;
        references = [&](const YAML::Node& node)
        {
            if (node.IsMap())
            {
                if (node["path"] && node["path"].IsScalar()) dependency(node["path"].as<std::string>());
                else if (node["uuid"] && node["uuid"].IsScalar())
                    dependency(AssetManager::GetPath(UUID(node["uuid"].as<uint64_t>())));
                for (const auto& child : node) references(child.second);
            }
            else if (node.IsSequence()) for (const auto& child : node) references(child);
        };
        dependency = [&](const std::string& name)
        {
            if (name.empty() || name == "Path was not found in AssetLibrary" || name.starts_with("data:")) return;
            const auto path = std::filesystem::path(name).lexically_normal();
            if (!dependencies.insert(path.generic_string()).second) return;
            const auto extension = path.extension().string();
            // Native shaders are referenced by their shared .vs/.fs prefix.
            if (extension.empty() || extension == ".shader")
            {
                dependency(path.string() + ".vs");
                dependency(path.string() + ".fs");
                return;
            }
            try
            {
                if (extension == ".material" || extension == ".skybox" || extension == ".shadergraph")
                    references(YAML::LoadFile(path.string()));
                else if (extension == ".gltf" || extension == ".glb")
                {
                    std::ifstream file(path, std::ios::binary);
                    Pipeline::Json gltf;
                    if (extension == ".gltf") file >> gltf;
                    else
                    {
                        uint32_t header[5]{};
                        if (!file.read(reinterpret_cast<char*>(header), sizeof(header)) ||
                            header[0] != 0x46546c67 || header[3] > 64 * 1024 * 1024 || header[4] != 0x4e4f534a) return;
                        std::string json(header[3], '\0');
                        if (!file.read(json.data(), json.size())) return;
                        gltf = Pipeline::Json::parse(json);
                    }
                    for (const char* category : {"buffers", "images"})
                        if (gltf.contains(category)) for (const auto& item : gltf[category])
                            if (item.contains("uri"))
                            {
                                const auto uri = item["uri"].get<std::string>();
                                if (!uri.starts_with("data:")) dependency((path.parent_path() / uri).generic_string());
                            }
                }
            }
            catch (const std::exception&) { /* The file's own missing/changed stamp still invalidates it. */ }
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
        constexpr uint32_t captureRevision = 5;
        bytes(&captureRevision, sizeof(captureRevision));
        const auto cloudConfig=YAML::Dump(SerializeCloudVolumeSettings(scene.GetCloudVolumeSettings()));
        bytes(cloudConfig.data(),cloudConfig.size());
        auto wetness=scene.GetSurfaceWetnessSettings();wetness.Sanitize();
        bytes(&wetness.enabled,sizeof(wetness.enabled));
        // Captures always use the dry baseline. Animating amount or tuning the
        // wet response must never invalidate/rebuild resident reflection data.
        if(wetness.enabled)bytes(&wetness.dryRoughness,sizeof(wetness.dryRoughness));
        const auto ambient = scene.GetEnvironmentAmbientLight();
        bytes(&ambient, sizeof(ambient));
        const float ambientIntensity = scene.GetEnvironmentAmbientLightIntensity();
        bytes(&ambientIntensity, sizeof(ambientIntensity));
        const auto skyPath = AssetManager::GetPath(scene.GetEnvironmentSkyboxUUID());
        dependency(skyPath);
        dependency("assets/shaders/model3d.vs");
        dependency("assets/shaders/model3d.fs");
        dependency("assets/shaders/skybox.fs");
        for (const auto& scenario : scene.GetLightingScenarios())
            dependency(scenario.skyboxPath.empty() ? AssetManager::GetPath(UUID(scenario.skyboxUUID)) : scenario.skyboxPath);
        // Capture only depends on static receivers and their actual material,
        // texture and sky references. Importing a hand atlas or editing another
        // scene must not disable this scene's weather. Sort both asset paths and
        // scene UUIDs: filesystem/entt insertion order is not authored radiance.
        const auto sorted = [&](auto view)
        {
            std::vector<entt::entity> handles(view.begin(), view.end());
            auto id = [&](entt::entity handle) {
                const auto* metadata = registry.try_get<EntityMetadata>(handle);
                return metadata ? uint64_t(metadata->uuid) : uint64_t(entt::to_integral(handle));
            };
            std::sort(handles.begin(), handles.end(), [&](auto a, auto b) { return id(a) < id(b); });
            return handles;
        };
        const auto assetReference = [&](int id)
        {
            const auto path = id >= 0 ? AssetManager::GetPath(id) : std::string{};
            const auto length = path.size();
            bytes(&length, sizeof(length));
            bytes(path.data(), path.size());
            dependency(path);
        };
        const auto fieldDependencies = [&](const MaterialFields& fields)
        {
            const auto values = [&](auto uniforms)
            {
                std::sort(uniforms.begin(), uniforms.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
                const auto count = uniforms.size();
                bytes(&count, sizeof(count));
                for (const auto& uniform : uniforms)
                {
                    bytes(uniform.name.data(), uniform.name.size());
                    bytes(&uniform.value, sizeof(uniform.value));
                }
            };
            values(fields.GetIntUniforms());values(fields.GetFloatUniforms());
            values(fields.GetVec2Uniforms());values(fields.GetVec3Uniforms());
            values(fields.GetVec4Uniforms());values(fields.GetColorUniforms());
            auto textures = fields.GetTextureUniforms();
            std::sort(textures.begin(), textures.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
            for (const auto& texture : textures)
            {
                bytes(texture.name.data(), texture.name.size());
                assetReference(texture.textureId);
            }
        };
        auto materialDependencies = [&](const MaterialAsset& material)
        {
            for (auto id : {material.shaderId, material.albedoId, material.normalId, material.specularId,
                            material.roughnessId, material.metallicId, material.emissionId})
                assetReference(id);
            bytes(&material.info, sizeof(material.info));
            bytes(&material.color, sizeof(material.color));
            bytes(&material.emissionColor, sizeof(material.emissionColor));
            bytes(&material.emissionIntensity, sizeof(material.emissionIntensity));
            bytes(&material.specularValue, sizeof(material.specularValue));
            bytes(&material.roughnessValue, sizeof(material.roughnessValue));
            bytes(&material.roughnessChannel, sizeof(material.roughnessChannel));
            bytes(&material.metallicValue, sizeof(material.metallicValue));
            bytes(&material.normalScale, sizeof(material.normalScale));
            fieldDependencies(material.materialFields);
        };
        for (auto handle : sorted(registry.view<Transform, Model>()))
        {
            auto& t = registry.get<Transform>(handle);
            auto& m = registry.get<Model>(handle);
            const auto* body = registry.try_get<Rigidbody>(handle);
            if (!m.staticModel || !t.IsActiveInHierarchy() ||
                (body && body->active && body->motionType != RigidbodyMotionType::STATIC))
                continue;
            auto matrix = t.GetModelMatrix();
            bytes(&matrix, sizeof(matrix));
            auto* asset = AssetManager::GetModel(m.modelId);
            if (!asset)
                continue;
            auto path = asset->GetPath();
            bytes(path.data(), path.size());
            dependency(path);
            bytes(&m.nodeIndex, sizeof(m.nodeIndex));
            bytes(&m.applyNodeTransform, sizeof(m.applyNodeTransform));
            bytes(&m.color, sizeof(m.color));
            bytes(&m.weatherSurface, sizeof(m.weatherSurface));
            bytes(&m.castShadow, sizeof(m.castShadow));
            if (const auto* material = registry.try_get<Material>(handle))
            {
                bytes(&material->color, sizeof(material->color));
                assetReference(material->materialId);
                if (material->materialId >= 0)
                    if (auto* asset = AssetManager::GetMaterial(material->materialId))
                    {
                        materialDependencies(*asset);
                    }
                const auto slots = material->materialIds.size();
                bytes(&slots, sizeof(slots));
                for (auto id : material->materialIds)
                {
                    assetReference(id);
                    if (id >= 0)
                        if (auto* asset = AssetManager::GetMaterial(id))
                        {
                            materialDependencies(*asset);
                        }
                }
                fieldDependencies(material->materialFields);
            }
        }
        for (auto handle : sorted(registry.view<DirectionalLight>()))
        {
            const auto& light = registry.get<DirectionalLight>(handle);
            if (light.mode == LightMode::Realtime) continue;
            bytes(&light.mode, sizeof(light.mode));
            bytes(&light.direction, sizeof(light.direction));
            bytes(&light.color, sizeof(light.color));
            bytes(&light.intensity, sizeof(light.intensity));
            bytes(&light.enabled, sizeof(light.enabled));
        }
        for (auto handle : sorted(registry.view<PointLight, Transform>()))
        {
            auto light = registry.get<PointLight>(handle);
            if (light.mode == LightMode::Realtime) continue;
            bytes(&light.mode, sizeof(light.mode));
            auto p = registry.get<Transform>(handle).GetGlobalPosition();
            bytes(&p, sizeof(p));
            bytes(&light.color, sizeof(light.color));
            bytes(&light.intensity, sizeof(light.intensity));
            bytes(&light.range, sizeof(light.range));
            bytes(&light.enabled, sizeof(light.enabled));
        }
        for (auto handle : sorted(registry.view<SpotLight, Transform>()))
        {
            const auto& light = registry.get<SpotLight>(handle);
            if (light.mode == LightMode::Realtime) continue;
            const auto& t = registry.get<Transform>(handle);
            auto position = t.GetGlobalPosition();
            auto rotation = t.GetGlobalRotation();
            bytes(&position, sizeof(position));
            bytes(&rotation, sizeof(rotation));
            bytes(&light.mode, sizeof(light.mode));
            bytes(&light.color, sizeof(light.color));
            bytes(&light.intensity, sizeof(light.intensity));
            bytes(&light.range, sizeof(light.range));
            bytes(&light.spotAngle, sizeof(light.spotAngle));
            bytes(&light.innerAngle, sizeof(light.innerAngle));
            bytes(&light.enabled, sizeof(light.enabled));
        }
        for (const auto& path : dependencies) fileStamp(path);
        return hash;
    }
    static std::string ScenarioProbeSignature(const Scene& scene)
    {
        // Rendering checks metadata only: disk dependency validation belongs to
        // explicit preparation, never to the per-frame transition path.
        uint64_t hash=1469598103934665603ULL;
        auto bytes=[&](const void* data,size_t size) {for(size_t i=0;i<size;++i){hash^=static_cast<const unsigned char*>(data)[i];hash*=1099511628211ULL;}};
        auto string=[&](const std::string& value){bytes(value.data(),value.size());};
        const auto& clouds=scene.GetCloudVolumeSettings();
        bytes(&clouds.enabled,sizeof(clouds.enabled));bytes(&clouds.baseHeight,sizeof(clouds.baseHeight));
        bytes(&clouds.thickness,sizeof(clouds.thickness));bytes(&clouds.center,sizeof(clouds.center));
        bytes(&clouds.halfExtents,sizeof(clouds.halfExtents));bytes(&clouds.noiseScale,sizeof(clouds.noiseScale));
        bytes(&clouds.windVelocity,sizeof(clouds.windVelocity));bytes(&clouds.steps,sizeof(clouds.steps));
        bytes(&clouds.extinction,sizeof(clouds.extinction));
        auto wetness=scene.GetSurfaceWetnessSettings();wetness.Sanitize();
        bytes(&wetness.enabled,sizeof(wetness.enabled));
        if(wetness.enabled)bytes(&wetness.dryRoughness,sizeof(wetness.dryRoughness));
        for(const auto& scenario : scene.GetLightingScenarios()) {
            string(scenario.id);string(scenario.skyboxPath);string(scenario.manifestPath);
            string(scenario.sourceFingerprint);string(scenario.settingsFingerprint);
            bytes(&scenario.ambientLight,sizeof(scenario.ambientLight));bytes(&scenario.ambientIntensity,sizeof(scenario.ambientIntensity));
            bytes(&scenario.sunEnabled,sizeof(scenario.sunEnabled));bytes(&scenario.sunDirection,sizeof(scenario.sunDirection));
            bytes(&scenario.sunColor,sizeof(scenario.sunColor));bytes(&scenario.sunIntensity,sizeof(scenario.sunIntensity));
            bytes(&scenario.skyboxUUID,sizeof(scenario.skyboxUUID));bytes(&scenario.skyboxIntensity,sizeof(scenario.skyboxIntensity));
            bytes(&scenario.cloudCoverage,sizeof(scenario.cloudCoverage));bytes(&scenario.cloudDensity,sizeof(scenario.cloudDensity));
            for(const auto& path:scenario.probeCachePaths)string(path);
        }
        return std::to_string(hash);
    }

    static std::string ScenarioReadySignature(const LightingScenario& scenario, const Scene& scene)
    {
        std::string result=LightingScenarioAuthoringFingerprint(scenario)+";"+scenario.sourceFingerprint+";"+
            scenario.settingsFingerprint+";"+scenario.manifestPath+";";
        for(const auto& path:scenario.probeCachePaths)result+=path+";";
        // Compare shared cloud authoring metadata, too: changing the layer must
        // not leave endpoint reflections marked prepared for the old volume.
        const auto& clouds=scene.GetCloudVolumeSettings();
        uint64_t hash=1469598103934665603ULL;
        auto add=[&](const auto& value) {
            for(size_t i=0;i<sizeof(value);++i) { hash^=reinterpret_cast<const unsigned char*>(&value)[i];hash*=1099511628211ULL; }
        };
        add(clouds.enabled);add(clouds.baseHeight);add(clouds.thickness);add(clouds.center);add(clouds.halfExtents);
        add(clouds.noiseScale);add(clouds.windVelocity);add(clouds.steps);add(clouds.extinction);
        auto wetness=scene.GetSurfaceWetnessSettings();wetness.Sanitize();
        add(wetness.enabled);if(wetness.enabled)add(wetness.dryRoughness);
        result+=std::to_string(hash);
        return result;
    }

    bool MeshRenderer3DSystem::LightingScenarioProbesReady(const std::string& id) const
    {
        if(!scene)return false;
        const auto* scenario=scene->FindLightingScenario(id);
        const auto signature=m_scenarioReadySignatures.find(id);
        if(!scenario || signature==m_scenarioReadySignatures.end() || signature->second!=ScenarioReadySignature(*scenario,*scene))return false;
        return m_probeSettings.empty() || (m_scenarioProbeArray && std::find(m_scenarioProbeIds.begin(),m_scenarioProbeIds.end(),id)!=m_scenarioProbeIds.end());
    }

    bool MeshRenderer3DSystem::PrepareLightingScenarios(bool captureMissing, std::string& error)
    {
        error.clear();
        m_scenarioProbeRepairable=false;
        // Warm the resident density texture before a clear-to-storm fade starts,
        // so its first nonzero coverage never generates/uploads noise in play.
        if(scene->GetCloudVolumeSettings().enabled && std::any_of(scene->GetLightingScenarios().begin(),scene->GetLightingScenarios().end(),
            [](const auto& scenario){return scenario.cloudCoverage>0 && scenario.cloudDensity>0;}))CreateCloudNoiseTexture();
        if(!ProbeArraysAvailable()) { error="Lighting scenario probe blending requires desktop OpenGL 4.0 or cube-map arrays.";m_scenarioError=error;return false; }
#if !defined(__EMSCRIPTEN__)
        DestroyLightingProbes();
        m_scenarioProbeIds.clear();
        m_scenarioProbeIrradiance.clear();
        m_scenarioProbeSignature=ScenarioProbeSignature(*scene);
        m_probeSettings.clear();
        for (auto probe : scene->GetLightingProbes()) {
            probe.Sanitize();
            if(probe.enabled && m_probeSettings.size()<MaxLightingProbes) m_probeSettings.push_back(probe);
        }
        m_probesDirty=false;
        const auto& scenarios=scene->GetLightingScenarios();
        m_scenariosReady=false;
        if (scenarios.empty()) return true;
        std::vector<const LightingScenario*> surfaceReady;
        std::string incompleteError,currentSource;
        if(captureMissing && std::any_of(scenarios.begin(),scenarios.end(),[](const auto& scenario){return !scenario.sourceFingerprint.empty();}))
            currentSource=LightingScenarioSourceFingerprint(CaptureLightingBakeScene(*scene));
        for(const auto& scenario:scenarios) {
            std::string reason;
            if(!LightingScenarioDataAvailable(scenario,reason,false)) {
                if(incompleteError.empty())incompleteError=scenario.name+": "+reason;
                continue;
            }
            if(!currentSource.empty()) {
                Debug::Log("LIGHTING_SCENARIO_SOURCE scenario=%s actual=%s expected=%s",scenario.id.c_str(),currentSource.c_str(),scenario.sourceFingerprint.c_str());
                if(currentSource!=scenario.sourceFingerprint) {
                    if(incompleteError.empty())incompleteError="Scene changed since '"+scenario.name+"' was baked. Bake surface lighting again before preparing probes.";
                    continue;
                }
            }
            // Distinct endpoint skies must be GPU resident before gameplay can
            // request a fade, including load-only preparation of valid caches.
            std::string skyPath=scenario.skyboxPath;
            if(skyPath.empty() && scenario.skyboxUUID)skyPath=AssetManager::GetPath(UUID(scenario.skyboxUUID));
            if(!skyPath.empty() && skyPath!="Path was not found in AssetLibrary") {
                GLint active=0,binding=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
                glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&binding);
                auto* sky=AssetManager::GetSkybox(skyPath);
                glBindTexture(GL_TEXTURE_CUBE_MAP,binding);glActiveTexture(active);
                if(!sky || !sky->IsLoaded()) {
                    if(incompleteError.empty())incompleteError="Could not load skybox for '"+scenario.name+"'.";
                    continue;
                }
            }
            surfaceReady.push_back(&scenario);
        }
        if(m_probeSettings.empty()) {
            for(const auto* scenario:surfaceReady)m_scenarioReadySignatures[scenario->id]=ScenarioReadySignature(*scenario,*scene);
            m_scenariosReady=incompleteError.empty();m_scenarioError=incompleteError;error=incompleteError;return m_scenariosReady;
        }
        constexpr int size=64;
        const size_t facePixels=size*size*4;
        const uint64_t sceneFingerprint=ProbeSceneFingerprint(scene->GetRegistry(),*scene);
        struct Capture { const LightingScenario* scenario; const LightingProbe* probe; std::string path; uint64_t fingerprint;
            std::vector<float> pixels; std::array<Vector3,9> sh{}; bool cached=false; };
        std::vector<Capture> captures;
        for (const auto* readyScenario : surfaceReady) {
            const auto& scenario=*readyScenario;
            const auto& settings=scenario.settingsFingerprint;
            std::vector<Capture> scenarioCaptures;
            bool complete=true;
            for (size_t i=0;i<m_probeSettings.size();++i) {
                Capture capture{&scenario,&m_probeSettings[i],i<scenario.probeCachePaths.size()?scenario.probeCachePaths[i]:"",sceneFingerprint};
                auto mix=[&](const void* data,size_t length) { for(size_t j=0;j<length;++j) {capture.fingerprint^=static_cast<const unsigned char*>(data)[j];capture.fingerprint*=1099511628211ULL;} };
                mix(scenario.id.data(),scenario.id.size()); mix(settings.data(),settings.size());
                const auto& probe=*capture.probe;
                mix(&probe.position,sizeof(probe.position)); mix(&probe.halfExtents,sizeof(probe.halfExtents));
                mix(&probe.nearClip,sizeof(probe.nearClip)); mix(&probe.farClip,sizeof(probe.farClip));
                capture.pixels.resize(facePixels*6);
                if(!capture.path.empty()) {
                    std::ifstream file(capture.path,std::ios::binary); uint64_t header[3]{};
                    if(file.read(reinterpret_cast<char*>(header),sizeof(header)) && header[0]==0x43414e4953505231ULL &&
                        header[1]==capture.fingerprint && header[2]==size) {
                        file.read(reinterpret_cast<char*>(capture.pixels.data()),capture.pixels.size()*sizeof(float));
                        file.read(reinterpret_cast<char*>(capture.sh.data()),sizeof(capture.sh));
                        capture.cached=bool(file);
                    }
                }
                if(!capture.cached && (!captureMissing || capture.path.empty())) {
                    if(!captureMissing && !capture.path.empty() && surfaceReady.size()==scenarios.size())
                        m_scenarioProbeRepairable=true;
                    if(incompleteError.empty())incompleteError="Prepare lighting probes for '"+scenario.name+"': "+(capture.path.empty()?"scenario has no probe cache paths":"cache is missing or stale");
                    complete=false;break;
                }
                scenarioCaptures.push_back(std::move(capture));
            }
            if(complete) {
                m_scenarioProbeIds.push_back(scenario.id);
                m_scenarioReadySignatures[scenario.id]=ScenarioReadySignature(scenario,*scene);
                for(auto& capture:scenarioCaptures)captures.push_back(std::move(capture));
            }
        }
        if(captures.empty()) { error=incompleteError;m_scenarioError=error;return false; }
        GLint maximumLayers=0;glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,&maximumLayers);
        if(captures.size()*6>size_t(maximumLayers)) {
            error="Lighting scenarios exceed this GPU's cube-map array capacity. Reduce scenarios or probes.";
            m_scenarioError=error;return false;
        }
        GLint drawFbo,readFbo,viewport[4],renderbuffer,active,pack; GLfloat clear[4]; GLboolean depthMask;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);
        glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
        const bool scissor=glIsEnabled(GL_SCISSOR_TEST),stereoFrame=m_stereoFrame;
        const int stereoPass=m_stereoPass;
        GLuint fbo=0,depth=0;
        auto restore=[&]() {
            m_capturingProbe=false;m_probeCaptureScenario=nullptr;m_stereoFrame=stereoFrame;m_stereoPass=stereoPass;
            m_stereoPrepared=m_stereoShadowReady=false;InvalidateStaticShadowCache();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);
            glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
            glClearColor(clear[0],clear[1],clear[2],clear[3]);glDepthMask(depthMask);
            if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
            glPixelStorei(GL_PACK_ALIGNMENT,pack);glActiveTexture(active);
            if(fbo)glDeleteFramebuffers(1,&fbo);if(depth)glDeleteRenderbuffers(1,&depth);
        };
        try {
            glGenTextures(1,&m_scenarioProbeArray);glActiveTexture(GL_TEXTURE8);glBindSampler(8,0);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_scenarioProbeArray);
            glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY,0,GL_RGBA16F,size,size,int(captures.size()*6),0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1,&fbo);glGenRenderbuffers(1,&depth);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
            glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
            glDisable(GL_SCISSOR_TEST);glPixelStorei(GL_PACK_ALIGNMENT,1);
            const Vector3 directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            const Vector3 ups[]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
            m_capturingProbe=true;m_stereoFrame=false;m_stereoPass=0;
            for(size_t i=0;i<captures.size();++i) {
                auto& capture=captures[i];
                if(!capture.cached) {
                    m_probeCaptureScenario=capture.scenario;
                    m_probePosition=capture.probe->position;m_probeNear=capture.probe->nearClip;m_probeFar=capture.probe->farClip;
                    m_probeProjection=glm::perspective(glm::radians(90.f),1.f,m_probeNear,m_probeFar);
                    float totalWeight=0;
                    for(int face=0;face<6;++face) {
                        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
                        glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,m_scenarioProbeArray,0,int(i*6+face));
                        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
                            throw std::runtime_error("Lighting scenario HDR framebuffer is incomplete");
                        glViewport(0,0,size,size);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                        m_probeView=glm::lookAt(m_probePosition,m_probePosition+directions[face],ups[face]);
                        Update(scene->GetRegistry(),0);
                        auto* pixels=capture.pixels.data()+face*facePixels;
                        glReadPixels(0,0,size,size,GL_RGBA,GL_FLOAT,pixels);
                        for(int y=0;y<size;++y)for(int x=0;x<size;++x) {
                            const float u=2*(x+.5f)/size-1,v=2*(y+.5f)/size-1;
                            const float weight=1/std::pow(1+u*u+v*v,1.5f);
                            const auto basis=ProbeSHBasis(ProbeTexelDirection(face,u,v));
                            const int offset=(y*size+x)*4;Vector3 radiance(pixels[offset],pixels[offset+1],pixels[offset+2]);
                            for(int c=0;c<3;++c)if(!std::isfinite(radiance[c]))radiance[c]=0;
                            radiance=glm::max(radiance,Vector3(0));
                            for(int j=0;j<9;++j)capture.sh[j]+=radiance*(basis[j]*weight);
                            totalWeight+=weight;
                        }
                    }
                    for(int j=0;j<9;++j)capture.sh[j]*=(4*glm::pi<float>()/totalWeight)*(j==0?1.f:j<4?2.f/3.f:.25f);
                    const auto parent=std::filesystem::path(capture.path).parent_path();
                    if(!parent.empty())std::filesystem::create_directories(parent);
                    const auto temporary=capture.path+".tmp";
                    std::ofstream file(temporary,std::ios::binary);uint64_t header[]={0x43414e4953505231ULL,capture.fingerprint,size};
                    file.write(reinterpret_cast<const char*>(header),sizeof(header));
                    file.write(reinterpret_cast<const char*>(capture.pixels.data()),capture.pixels.size()*sizeof(float));
                    file.write(reinterpret_cast<const char*>(capture.sh.data()),sizeof(capture.sh));file.close();
                    if(!file)throw std::runtime_error("Could not write lighting scenario probe cache: "+capture.path);
                    std::filesystem::rename(temporary,capture.path);
                }
                glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_scenarioProbeArray);
                glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY,0,0,0,int(i*6),size,size,6,GL_RGBA,GL_FLOAT,capture.pixels.data());
                m_scenarioProbeIrradiance.push_back(capture.sh);
            }
            glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_scenarioProbeArray);
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP_ARRAY);restore();
            m_scenariosReady=incompleteError.empty();m_scenarioError=incompleteError;error=incompleteError;return m_scenariosReady;
        } catch(const std::exception& exception) {
            restore();DestroyLightingProbes();error=exception.what();m_scenarioError=error;return false;
        }
#else
        return false;
#endif
    }

    bool MeshRenderer3DSystem::LoadLightingScenarios(std::string& error)
    {
        if(PrepareLightingScenarios(false,error))return true;
        if(!m_scenarioProbeRepairable)return false;
        // Only a known missing/stale probe cache is repairable automatically.
        // The capture path validates the current static source fingerprint
        // against the surface bake before rendering any endpoint cubemaps.
        Debug::Log("LIGHTING_SCENARIO_PROBES_REPAIR phase=scene-load");
        const bool ready=PrepareLightingScenarios(true,error);
        if(ready)Debug::Log("LIGHTING_SCENARIO_PROBES_READY phase=scene-load");
        return ready;
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
        if (!scene->GetLightingScenarios().empty()) {
            if(!m_probesDirty && settings==m_probeSettings && m_scenarioProbeSignature==ScenarioProbeSignature(*scene)) return;
            std::string error;PrepareLightingScenarios(false,error);
            return;
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
#if !defined(__EMSCRIPTEN__)
            if(ProbeArraysAvailable()) {
            glGenTextures(1,&m_scenarioProbeArray);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_scenarioProbeArray);
            glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY,0,GL_RGBA16F,size,size,int(settings.size()*6),0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
            }
#endif
            auto packProbe=[&](const std::vector<float>& cube) {
                if(!ProbeArraysAvailable())glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
#if !defined(__EMSCRIPTEN__)
                if(ProbeArraysAvailable()) {
                    glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_scenarioProbeArray);
                    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY,0,0,0,int((m_probeIrradiance.size()-1)*6),size,size,6,GL_RGBA,GL_FLOAT,cube.data());
                }
#endif
            };
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
                    packProbe(cube);
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
                packProbe(cube);
            }
#if !defined(__EMSCRIPTEN__)
            if(ProbeArraysAvailable()) {
                glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,m_scenarioProbeArray);
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP_ARRAY);
                if(!m_probeTextures.empty())glDeleteTextures(int(m_probeTextures.size()),m_probeTextures.data());
                m_probeTextures.clear();
            }
#endif
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
        const auto blend=RenderLightingScenario();
        int from=0,to=0;
        bool ready=m_scenarioProbeArray!=0 || (!ProbeArraysAvailable() && !m_probeTextures.empty());
        if(blend.from && blend.to) {
            auto a=std::find(m_scenarioProbeIds.begin(),m_scenarioProbeIds.end(),blend.from->id);
            auto b=std::find(m_scenarioProbeIds.begin(),m_scenarioProbeIds.end(),blend.to->id);
            ready=ready && a!=m_scenarioProbeIds.end() && b!=m_scenarioProbeIds.end();
            from=int(a-m_scenarioProbeIds.begin())*int(m_probeSettings.size());
            to=int(b-m_scenarioProbeIds.begin())*int(m_probeSettings.size());
        }
        const int count=m_capturingProbe || !ready ? 0 : int(m_probeSettings.size());
        shader.SetBool("canisProbeCapture",m_capturingProbe);
        shader.SetInt("canisProbeCount",count);
        shader.SetInt("canisProbeArray",8);
        shader.SetInt("canisProbeLayerFrom",from);
        shader.SetInt("canisProbeLayerTo",to);
        shader.SetFloat("canisProbeBlend",blend.from && blend.to ? blend.blend : 0);
        // Do not bind the currently attached render target while capturing.
#if !defined(__EMSCRIPTEN__)
        if(ProbeArraysAvailable()) {
            glActiveTexture(GL_TEXTURE8);glBindSampler(8,0);
            unsigned int texture = m_capturingProbe ? 0 : m_scenarioProbeArray;
#if defined(__APPLE__)
            // Avoid Apple's software fragment fallback for an incomplete
            // samplerCubeArray, including scenes with no reflection probes.
            if (!texture)
            {
                if (!m_emptyProbeArray)
                {
                    const unsigned char black[24] = {};
                    glGenTextures(1, &m_emptyProbeArray);
                    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_emptyProbeArray);
                    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA8, 1, 1, 6,
                                 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
                    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                }
                texture = m_emptyProbeArray;
            }
#endif
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);
        } else
#endif
        {
            for(int i=0;i<MaxLightingProbes;++i) {
                shader.SetInt("canisProbeMap"+std::to_string(i),8+i);
                glActiveTexture(GL_TEXTURE8+i);glBindSampler(8+i,0);
                glBindTexture(GL_TEXTURE_CUBE_MAP,!m_capturingProbe && i<int(m_probeTextures.size())?m_probeTextures[i]:0);
            }
        }
        const auto& irradiance=blend.from && blend.to ? m_scenarioProbeIrradiance : m_probeIrradiance;
        for(int i=0;i<count;++i) {
            const auto index=std::to_string(i);const auto& p=m_probeSettings[i];
            shader.SetVec3("canisProbePosition["+index+"]",p.position);
            shader.SetVec3("canisProbeExtent["+index+"]",p.halfExtents);
            shader.SetVec3("canisProbeParams["+index+"]",Vector3(p.blendDistance,p.diffuseIntensity,
                scene->GetEnvironmentReflectionsEnabled()?p.specularIntensity:0));
            for(int j=0;j<9;++j)shader.SetVec3("canisProbeSH["+std::to_string(i*9+j)+"]",
                glm::mix(irradiance[from+i][j],irradiance[to+i][j],blend.from && blend.to ? blend.blend : 0));
        }
        glActiveTexture(GL_TEXTURE0);
    }
} // namespace Canis
