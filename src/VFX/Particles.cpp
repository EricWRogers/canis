#include <Canis/VFX/Particles.hpp>

#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Editor.hpp>

#include <algorithm>
#include <cmath>
#include <random>

namespace Canis
{
    namespace
    {
        ComponentConf particleEmitterConf = {};
        SystemConf particleEmitterSystemConf = {};

        struct SimpleParticle
        {
            Vector3 velocity = Vector3(0.0f);
            Vector3 localPosition = Vector3(0.0f);
            float gravity = 0.0f;
            float drag = 0.0f;
            float age = 0.0f;
            float lifetime = 0.5f;
            Vector3 startScale = Vector3(1.0f);
            Vector3 endScale = Vector3(0.0f);
            entt::entity emitterHandle = entt::null;
            entt::entity impactEmitter = entt::null;
            Vector3 collisionOrigin{0};
            int collisionMode = 0;
            float collisionPlaneHeight = 0;
            bool localSpace = false;
            bool useAgeUniforms = true;
            Color startColor = Color(1.0f);
            Color endColor = Color(1.0f);
        };

        std::mt19937 &GetParticleRng()
        {
            static thread_local std::mt19937 rng(std::random_device{}());
            return rng;
        }

        float RandomRange(ParticleEmitter &emitter, const float _minValue, const float _maxValue)
        {
            if (emitter.randomState == 0)
                emitter.randomState =
                    emitter.randomSeed > 0 ? uint32_t(emitter.randomSeed) : GetParticleRng()();
            if (emitter.randomState == 0)
                emitter.randomState = 1;
            // Per-emitter state avoids coupling effects to registry iteration order.
            auto &state = emitter.randomState;
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const float unit = float(state >> 8) * (1.f / 16777216.f);
            return std::min(_minValue, _maxValue) + std::abs(_maxValue - _minValue) * unit;
        }

        Vector3 RandomVector3(ParticleEmitter &emitter, const Vector3 &_minValue,
                              const Vector3 &_maxValue)
        {
            // Explicit sequencing makes seeded results portable across compilers.
            const float x = RandomRange(emitter, _minValue.x, _maxValue.x);
            const float y = RandomRange(emitter, _minValue.y, _maxValue.y);
            const float z = RandomRange(emitter, _minValue.z, _maxValue.z);
            return {x, y, z};
        }

        void ResolveEmitterAssets(ParticleEmitter &_emitter)
        {
            if (_emitter.cachedModelPath != _emitter.modelPath)
            {
                _emitter.cachedModelPath = _emitter.modelPath;
                _emitter.cachedModelId = -1;
            }
            if (_emitter.cachedMaterialPath != _emitter.materialPath)
            {
                _emitter.cachedMaterialPath = _emitter.materialPath;
                _emitter.cachedMaterialId = -1;
            }
            if (_emitter.cachedModelId < 0 && !_emitter.modelPath.empty())
                _emitter.cachedModelId = AssetManager::LoadModel(_emitter.modelPath);

            if (_emitter.cachedMaterialId < 0 && !_emitter.materialPath.empty())
                _emitter.cachedMaterialId = AssetManager::LoadMaterial(_emitter.materialPath);
        }

        void CleanupDeadParticles(ParticleEmitter &_emitter, const entt::registry &_registry)
        {
            _emitter.particles.erase(std::remove_if(_emitter.particles.begin(),
                                                    _emitter.particles.end(),
                                                    [&_registry](const entt::entity _entityHandle) {
                                                        return !_registry.valid(_entityHandle);
                                                    }),
                                     _emitter.particles.end());
        }

        void DestroyEmitterParticles(ParticleEmitter &_emitter, entt::registry &_registry)
        {
            for (const entt::entity particleHandle : _emitter.particles)
            {
                if (_registry.valid(particleHandle))
                    _registry.destroy(particleHandle);
            }

            _emitter.particles.clear();
        }

        bool UpdateLocalParticleTransform(SimpleParticle &_particle, Transform &_particleTransform,
                                          entt::registry &_registry, const Vector3 &_particleScale)
        {
            if (!_registry.valid(_particle.emitterHandle) ||
                !_registry.all_of<Transform>(_particle.emitterHandle))
            {
                return false;
            }

            const Transform &emitterTransform = _registry.get<Transform>(_particle.emitterHandle);
            if (!emitterTransform.IsActiveInHierarchy())
                return false;

            const Vector4 worldPosition =
                emitterTransform.GetModelMatrix() * Vector4(_particle.localPosition, 1.0f);
            _particleTransform.position = Vector3(worldPosition);
            _particleTransform.rotation = emitterTransform.GetGlobalRotation();
            _particleTransform.scale = emitterTransform.GetGlobalScale() * _particleScale;
            return true;
        }

        Vector3 Integrate(SimpleParticle &particle, float dt)
        {
            const Vector3 acceleration(0, particle.gravity, 0), velocity = particle.velocity;
            if (particle.drag < .0001f)
            {
                particle.velocity += acceleration * dt;
                return velocity * dt + acceleration * (.5f * dt * dt);
            }
            const float decay = std::exp(-particle.drag * dt);
            const float travel = -std::expm1(-particle.drag * dt) / particle.drag;
            particle.velocity = velocity * decay + acceleration * travel;
            return velocity * travel + acceleration * ((dt - travel) / particle.drag);
        }

        bool SpawnParticle(ParticleEmitter &_emitter, Transform &_emitterTransform,
                           entt::registry &_registry, const entt::entity _emitterHandle,
                           float initialAge = 0, bool forceWorldSpace = false)
        {
            if (!std::isfinite(initialAge) || !std::isfinite(_emitter.particleLifetime))
                return false;
            if (_emitter.particles.size() >= size_t(std::max(0, _emitter.maxParticles)) ||
                _emitter.spawnedThisFrame >= std::max(0, _emitter.maxSpawnPerFrame) ||
                initialAge >= std::max(.001f, _emitter.particleLifetime))
                return false;
            ResolveEmitterAssets(_emitter);

            const entt::entity particleHandle = _registry.create();
            Transform &particleTransform = _registry.emplace<Transform>(particleHandle);
            Model &particleModel = _registry.emplace<Model>(particleHandle);
            Material &particleMaterial = _registry.emplace<Material>(particleHandle);
            SimpleParticle &particle = _registry.emplace<SimpleParticle>(particleHandle);

            const Vector3 extents = glm::abs(_emitter.spawnExtents);
            Vector3 randomOffset =
                _emitter.spawnShape == 1 ? Vector3(0) : RandomVector3(_emitter, -extents, extents);
            if (_emitter.spawnShape == 2)
                randomOffset.y = 0;
            const Vector3 randomDirection =
                RandomVector3(_emitter, _emitter.minVelocity, _emitter.maxVelocity);
            const float directionLength = glm::length(randomDirection);
            const Vector3 normalizedDirection = (directionLength > 0.0001f)
                                                    ? (randomDirection / directionLength)
                                                    : Vector3(0.0f, 1.0f, 0.0f);
            const float speed = RandomRange(_emitter, _emitter.speedMin, _emitter.speedMax);
            const Vector3 startScale =
                RandomVector3(_emitter, _emitter.startScaleMin, _emitter.startScaleMax);
            const Quaternion emitterRotation = _emitterTransform.GetGlobalRotation();

            particle.emitterHandle = _emitterHandle;
            if (_emitter.localSpace && !forceWorldSpace)
            {
                particle.localSpace = true;
                particle.emitterHandle = _emitterHandle;
                particle.localPosition = randomOffset;
                UpdateLocalParticleTransform(particle, particleTransform, _registry, startScale);
            }
            else
            {
                const Vector4 worldPosition =
                    _emitterTransform.GetModelMatrix() * Vector4(randomOffset, 1.0f);
                particleTransform.position = Vector3(worldPosition);
                particleTransform.rotation = emitterRotation;
                particleTransform.scale = startScale;
            }

            particleModel.modelId = _emitter.cachedModelId;
            particleModel.color = _emitter.startColor;
            particleModel.castShadow = _emitter.castShadow;

            particleMaterial.materialId = _emitter.cachedMaterialId;
            particleMaterial.color = _emitter.startColor;
            particle.useAgeUniforms = _emitter.useAgeUniforms;
            if (particle.useAgeUniforms)
            {
                particleMaterial.materialFields.SetFloat("particleAge", 0.0f);
                particleMaterial.materialFields.SetFloat(
                    "particleLifetime", std::max(_emitter.particleLifetime, 0.001f));
                particleMaterial.materialFields.SetFloat("particleNormalizedAge", 0.0f);
            }

            // Velocity ranges are always authored relative to the emitter.
            // World-space particles inherit its orientation once at spawn;
            // local-space particles continue following it for their lifetime.
            particle.velocity =
                (particle.localSpace ? normalizedDirection
                                     : glm::normalize(emitterRotation * normalizedDirection)) *
                speed;
            particle.gravity = _emitter.gravity;
            particle.drag = std::max(_emitter.drag, 0.0f);
            particle.age = 0.0f;
            particle.lifetime = std::max(_emitter.particleLifetime, 0.001f);
            particle.startScale = startScale;
            particle.endScale = _emitter.endScale;
            particle.startColor = _emitter.startColor;
            particle.endColor = _emitter.endColor;
            particle.collisionMode = _emitter.collisionMode;
            particle.collisionPlaneHeight = _emitter.collisionPlaneHeight;
            if (_emitter.impactEmitter && _emitter.impactEmitter.TryGet() != _emitter.entity &&
                &_emitter.impactEmitter.scene == &_emitter.entity->scene)
                particle.impactEmitter = _emitter.impactEmitter.GetHandle();
            particle.collisionOrigin = particleTransform.position;
            particle.age = initialAge;
            const Vector3 displacement = Integrate(particle, initialAge);
            const float t = Clamp01(initialAge / particle.lifetime);
            const Vector3 scale = glm::mix(startScale, particle.endScale, t);
            if (particle.localSpace)
            {
                particle.localPosition += displacement;
                UpdateLocalParticleTransform(particle, particleTransform, _registry, scale);
            }
            else
            {
                particleTransform.position += displacement;
                particleTransform.scale = scale;
            }
            particleModel.color = particleMaterial.color =
                glm::mix(particle.startColor, particle.endColor, t);
            if (particle.useAgeUniforms)
            {
                particleMaterial.materialFields.SetFloat("particleAge", initialAge);
                particleMaterial.materialFields.SetFloat("particleNormalizedAge", t);
            }

            _emitter.particles.push_back(particleHandle);
            ++_emitter.totalSpawned;
            ++_emitter.spawnedThisFrame;
            return true;
        }

        void EmitBurst(ParticleEmitter &_emitter, Transform &_emitterTransform,
                       entt::registry &_registry, const entt::entity _emitterHandle,
                       bool forceWorldSpace = false)
        {
            const int particlesToSpawn = std::min(
                std::max(_emitter.burstCount, 0),
                std::max(0, std::min(_emitter.maxSpawnPerFrame - _emitter.spawnedThisFrame,
                                     _emitter.maxParticles - int(_emitter.particles.size()))));
            _emitter.totalDropped += std::max(_emitter.burstCount, 0) - particlesToSpawn;
            for (int i = 0; i < particlesToSpawn; ++i)
                SpawnParticle(_emitter, _emitterTransform, _registry, _emitterHandle, 0,
                              forceWorldSpace);
        }

        void EmitContinuous(ParticleEmitter &_emitter, Transform &_emitterTransform,
                            entt::registry &_registry, const entt::entity _emitterHandle,
                            const float _deltaTime, float inactiveTail = 0)
        {
            if (!std::isfinite(_emitter.spawnInterval))
                return;
            const double interval = std::max(_emitter.spawnInterval, 0.001f);
            const double accumulated = double(_emitter.spawnAccumulator) + _deltaTime;
            const double due = std::floor(accumulated / interval + 1e-6);
            _emitter.spawnAccumulator = float(std::max(0., accumulated - due * interval));
            const int particlesPerStep = std::max(_emitter.spawnCount, 1);
            const int budget =
                std::max(0, std::min(_emitter.maxSpawnPerFrame - _emitter.spawnedThisFrame,
                                     _emitter.maxParticles - int(_emitter.particles.size())));
            const int count = int(std::min(double(budget), due * particlesPerStep));
            const int steps = count > 0 ? 1 + (count - 1) / particlesPerStep : 0;
            int remaining = count;
            for (int step = 0; step < steps; ++step)
            {
                const float age =
                    inactiveTail + _emitter.spawnAccumulator + float((steps - step - 1) * interval);
                for (int i = 0; i < particlesPerStep && remaining > 0; ++i, --remaining)
                    if (!SpawnParticle(_emitter, _emitterTransform, _registry, _emitterHandle, age))
                        ++_emitter.totalDropped;
            }
            _emitter.totalDropped +=
                uint64_t(std::min(due * particlesPerStep - count, double(UINT64_MAX / 2)));
        }
    } // namespace

    void RegisterParticleEmitterComponent(App &_app)
    {
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, playOnStart);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, paused);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, simulationSpeed);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, looping);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, burstMode);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, destroyEntityWhenDone);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, localSpace);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, spawnShape);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, useAgeUniforms);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, maxParticles);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, maxSpawnPerFrame);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, randomSeed);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, collisionMode);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, collisionPlaneHeight);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, impactEmitter);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, burstCount);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, spawnCount);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, emissionDuration);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, spawnInterval);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, particleLifetime);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, gravity);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, drag);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, speedMin);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, speedMax);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, spawnExtents);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, minVelocity);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, maxVelocity);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, startScaleMin);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, startScaleMax);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, endScale);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, startColor);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, endColor);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, modelPath);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, materialPath);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, castShadow);

        DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(particleEmitterConf, Canis::ParticleEmitter,
                                              Canis::Transform);

        particleEmitterConf.DrawInspector = [](Editor &_editor, Entity &_entity,
                                               const ScriptConf &_conf) -> void {
            Canis::ParticleEmitter *component =
                _entity.HasComponent<Canis::ParticleEmitter>()
                    ? &_entity.GetComponent<Canis::ParticleEmitter>()
                    : nullptr;
            if (component == nullptr)
                return;

            if (ImGui::Button("Restart"))
            {
                DestroyEmitterParticles(*component, _entity.scene.GetRegistry());
                component->editorPreviewPaused = false;
                component->Play();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
            {
                component->playing = false;
                component->pendingDestroy = false;
                component->editorPreviewPaused = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                DestroyEmitterParticles(*component, _entity.scene.GetRegistry());
                component->playing = false;
                component->pendingDestroy = false;
                component->editorPreviewPaused = true;
            }

            DrawInspectorField(_editor, "playOnStart", _conf.name.c_str(), component->playOnStart);
            DrawInspectorField(_editor, "paused", _conf.name.c_str(), component->paused);
            DrawInspectorField(_editor, "simulationSpeed", _conf.name.c_str(), component->simulationSpeed);
            DrawInspectorField(_editor, "looping", _conf.name.c_str(), component->looping);
            DrawInspectorField(_editor, "burstMode", _conf.name.c_str(), component->burstMode);
            DrawInspectorField(_editor, "destroyEntityWhenDone", _conf.name.c_str(),
                               component->destroyEntityWhenDone);
            DrawInspectorField(_editor, "localSpace", _conf.name.c_str(), component->localSpace);
            const char *shapes[] = {"Box", "Point", "Plane (XZ)"};
            component->spawnShape = std::clamp(component->spawnShape, 0, 2);
            DrawInspectorComboField("Spawn shape", _conf.name.c_str(), &component->spawnShape,
                                    shapes, 3);
            DrawInspectorField(_editor, "useAgeUniforms", _conf.name.c_str(),
                               component->useAgeUniforms);
            DrawInspectorField(_editor, "maxParticles", _conf.name.c_str(),
                               component->maxParticles);
            DrawInspectorField(_editor, "maxSpawnPerFrame", _conf.name.c_str(),
                               component->maxSpawnPerFrame);
            DrawInspectorField(_editor, "randomSeed", _conf.name.c_str(), component->randomSeed);
            const char *collisionModes[] = {"None", "World plane", "Native colliders"};
            component->collisionMode = std::clamp(component->collisionMode, 0, 2);
            DrawInspectorComboField("Collision", _conf.name.c_str(), &component->collisionMode,
                                    collisionModes, 3);
            if (component->collisionMode == 1)
                DrawInspectorField(_editor, "collisionPlaneHeight", _conf.name.c_str(),
                                   component->collisionPlaneHeight);
            DrawInspectorField(_editor, "impactEmitter", _conf.name.c_str(),
                               component->impactEmitter);
            ImGui::Text("Alive: %zu / %d", component->particles.size(), component->maxParticles);
            ImGui::Text("Spawned: %llu  Dropped: %llu  Hits: %llu",
                        static_cast<unsigned long long>(component->totalSpawned),
                        static_cast<unsigned long long>(component->totalDropped),
                        static_cast<unsigned long long>(component->totalImpacts));
            DrawInspectorField(_editor, "burstCount", _conf.name.c_str(), component->burstCount);
            DrawInspectorField(_editor, "spawnCount", _conf.name.c_str(), component->spawnCount);
            DrawInspectorField(_editor, "emissionDuration", _conf.name.c_str(),
                               component->emissionDuration);
            DrawInspectorField(_editor, "spawnInterval", _conf.name.c_str(),
                               component->spawnInterval);
            DrawInspectorField(_editor, "particleLifetime", _conf.name.c_str(),
                               component->particleLifetime);
            DrawInspectorField(_editor, "gravity", _conf.name.c_str(), component->gravity);
            DrawInspectorField(_editor, "drag", _conf.name.c_str(), component->drag);
            DrawInspectorField(_editor, "speedMin", _conf.name.c_str(), component->speedMin);
            DrawInspectorField(_editor, "speedMax", _conf.name.c_str(), component->speedMax);
            DrawInspectorField(_editor, "spawnExtents", _conf.name.c_str(),
                               component->spawnExtents);
            DrawInspectorField(_editor, "minVelocity", _conf.name.c_str(), component->minVelocity);
            DrawInspectorField(_editor, "maxVelocity", _conf.name.c_str(), component->maxVelocity);
            DrawInspectorField(_editor, "startScaleMin", _conf.name.c_str(),
                               component->startScaleMin);
            DrawInspectorField(_editor, "startScaleMax", _conf.name.c_str(),
                               component->startScaleMax);
            DrawInspectorField(_editor, "endScale", _conf.name.c_str(), component->endScale);
            DrawInspectorField(_editor, "startColor", _conf.name.c_str(), component->startColor);
            DrawInspectorField(_editor, "endColor", _conf.name.c_str(), component->endColor);

            auto drawAssetPathDropTarget =
                [](const char *_label, std::string &_path, int &_cachedId,
                   const MetaFileAsset::FileType _expectedType, auto _loader) -> void {
                std::string assetLabel = "[ empty ]";
                if (!_path.empty())
                {
                    if (MetaFileAsset *meta = AssetManager::GetMetaFile(_path))
                        assetLabel = meta->name;
                    else
                        assetLabel = _path;
                }

                ImGui::Text("%s", _label);
                ImGui::SameLine();
                ImGui::Button(assetLabel.c_str(), ImVec2(180, 0));

                if (!ImGui::BeginDragDropTarget())
                    return;

                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped =
                        *static_cast<const AssetDragData *>(payload->Data);
                    std::string path = std::string(dropped.path);
                    if (path.empty())
                        path = AssetManager::GetPath(dropped.uuid);

                    if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                    {
                        if (meta->type == _expectedType)
                        {
                            _path = path;
                            _cachedId = _loader(path);
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            };

            drawAssetPathDropTarget(
                "model", component->modelPath, component->cachedModelId,
                MetaFileAsset::FileType::MODEL,
                [](const std::string &_path) -> int { return AssetManager::LoadModel(_path); });

            drawAssetPathDropTarget(
                "material", component->materialPath, component->cachedMaterialId,
                MetaFileAsset::FileType::MATERIAL,
                [](const std::string &_path) -> int { return AssetManager::LoadMaterial(_path); });

            DrawInspectorField(_editor, "castShadow", _conf.name.c_str(), component->castShadow);
        };

        _app.RegisterComponent(particleEmitterConf);
    }

    DEFAULT_UNREGISTER_COMPONENT(particleEmitterConf, ParticleEmitter)

    void RegisterParticleEmitterSystem(App &_app)
    {
        DEFAULT_SYSTEM_CONFIG(particleEmitterSystemConf, Canis::ParticleEmitterSystem,
                              Canis::SystemPipeline::Update);
        _app.RegisterSystem(particleEmitterSystemConf);
    }

    DEFAULT_UNREGISTER_SYSTEM(particleEmitterSystemConf, ParticleEmitter)

    ParticleEmitterSystem::ParticleEmitterSystem() : System()
    {
        m_name = "Canis::ParticleEmitterSystem";
    }

    void ParticleEmitterSystem::Create()
    {
    }

    void ParticleEmitterSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        UpdateInternal(_registry, _deltaTime, nullptr, false);
    }

    void ParticleEmitterSystem::UpdateEditorPreview(entt::registry &_registry, float _deltaTime,
                                                    Entity *_selectedEntity)
    {
        UpdateInternal(_registry, _deltaTime, _selectedEntity, true);
    }

    void ParticleEmitterSystem::UpdateInternal(entt::registry &_registry, float _deltaTime,
                                               Entity *_previewEmitter, bool _editorPreview)
    {
        if (!std::isfinite(_deltaTime) || _deltaTime < 0)
            _deltaTime = 0;
        auto &expiredParticles = m_expiredParticles;
        expiredParticles.clear();
        m_impacts.clear();
        for (auto handle : _registry.view<ParticleEmitter>())
        {
            auto &emitter = _registry.get<ParticleEmitter>(handle);
            emitter.spawnedThisFrame = 0;
            CleanupDeadParticles(emitter, _registry);
            while (emitter.particles.size() > size_t(std::max(0, emitter.maxParticles)))
            {
                _registry.destroy(emitter.particles.back());
                emitter.particles.pop_back();
                ++emitter.totalDropped;
            }
        }
        auto particleView = _registry.view<SimpleParticle, Transform, Model, Material>();
        for (auto [entityHandle, particle, transform, model, material] : particleView.each())
        {
            if (!_registry.valid(particle.emitterHandle) ||
                !_registry.all_of<ParticleEmitter>(particle.emitterHandle))
            {
                expiredParticles.push_back(entityHandle);
                continue;
            }
            const auto &owner = _registry.get<ParticleEmitter>(particle.emitterHandle);
            if (!_editorPreview && owner.paused)
                continue;
            const float playbackSpeed = _editorPreview ? 1.f :
                (std::isfinite(owner.simulationSpeed) ? std::max(0.f, owner.simulationSpeed) : 0.f);
            const float step =
                std::min(_deltaTime * playbackSpeed, std::max(0.f, particle.lifetime - particle.age));
            particle.age += step;
            const Vector3 displacement = Integrate(particle, step);

            const float t = Clamp01(particle.age / std::max(particle.lifetime, 0.001f));
            const Vector3 particleScale = glm::mix(particle.startScale, particle.endScale, t);

            if (particle.localSpace)
            {
                particle.localPosition += displacement;
                if (!UpdateLocalParticleTransform(particle, transform, _registry, particleScale))
                {
                    expiredParticles.push_back(entityHandle);
                    continue;
                }
            }
            else
            {
                transform.position += displacement;
                transform.scale = particleScale;
            }

            bool collided = false;
            Vector3 hitPoint{0}, hitNormal(0, 1, 0);
            if (particle.collisionMode == 1 &&
                particle.collisionOrigin.y >= particle.collisionPlaneHeight &&
                transform.position.y <= particle.collisionPlaneHeight &&
                transform.position.y < particle.collisionOrigin.y)
            {
                float fraction = (particle.collisionPlaneHeight - particle.collisionOrigin.y) /
                                 (transform.position.y - particle.collisionOrigin.y);
                hitPoint = glm::mix(particle.collisionOrigin, transform.position, fraction);
                collided = true;
            }
            else if (particle.collisionMode == 2 && scene)
            {
                auto travel = transform.position - particle.collisionOrigin;
                float distance = glm::length(travel);
                RaycastHit hit;
                if (distance > .00001f &&
                    scene->Raycast(particle.collisionOrigin, travel / distance, hit, distance))
                {
                    collided = true;
                    hitPoint = hit.point;
                    hitNormal = hit.normal;
                }
            }
            particle.collisionOrigin = transform.position;
            if (collided)
            {
                auto &owner = _registry.get<ParticleEmitter>(particle.emitterHandle);
                ++owner.totalImpacts;
                if (_registry.valid(particle.impactEmitter))
                    m_impacts.push_back({particle.impactEmitter, hitPoint, hitNormal});
                expiredParticles.push_back(entityHandle);
                continue;
            }

            const Color color = glm::mix(particle.startColor, particle.endColor, t);
            model.color = color;
            material.color = color;
            if (particle.useAgeUniforms)
            {
                material.materialFields.SetFloat("particleAge", particle.age);
                material.materialFields.SetFloat("particleLifetime", particle.lifetime);
                material.materialFields.SetFloat("particleNormalizedAge", t);
            }

            if (particle.age >= particle.lifetime)
                expiredParticles.push_back(entityHandle);
        }

        for (const entt::entity entityHandle : expiredParticles)
        {
            if (_registry.valid(entityHandle))
                _registry.destroy(entityHandle);
        }

        auto emitterView = _registry.view<Canis::ParticleEmitter, Transform>();
        for (auto [entityHandle, emitter, transform] : emitterView.each())
        {
            if (emitter.entity == nullptr)
                continue;

            CleanupDeadParticles(emitter, _registry);
            const float emitterDelta = _editorPreview ? _deltaTime :
                (emitter.paused || !std::isfinite(emitter.simulationSpeed) ? 0.f : _deltaTime * std::max(0.f, emitter.simulationSpeed));
            if (!_editorPreview && (emitter.paused || !std::isfinite(emitter.simulationSpeed) || emitter.simulationSpeed <= 0.f))
                continue;
            if (!emitter.entity->Active() || !transform.IsActiveInHierarchy())
                continue;

            if (_editorPreview)
            {
                const bool selected = emitter.entity == _previewEmitter;
                if (!selected)
                {
                    if (emitter.editorPreviewSelected)
                    {
                        DestroyEmitterParticles(emitter, _registry);
                        emitter.playing = false;
                        emitter.pendingDestroy = false;
                        emitter.initialized = false;
                        emitter.editorPreviewSelected = false;
                    }
                    continue;
                }

                if (!emitter.editorPreviewSelected)
                {
                    emitter.editorPreviewSelected = true;
                    emitter.editorPreviewPaused = false;
                    emitter.Play();
                }
                else if (!emitter.editorPreviewPaused && !emitter.playing &&
                         emitter.particles.empty())
                {
                    // Keep short one-shot effects visible while selected.
                    emitter.Play();
                }
            }
            else if (emitter.editorPreviewSelected)
            {
                DestroyEmitterParticles(emitter, _registry);
                emitter.playing = false;
                emitter.pendingDestroy = false;
                emitter.initialized = false;
                emitter.editorPreviewSelected = false;
            }

            if (!emitter.initialized)
            {
                emitter.initialized = true;
                emitter.elapsed = 0.0f;
                emitter.spawnAccumulator = 0.0f;
                emitter.hasTriggeredBurst = false;
                emitter.pendingDestroy = false;
                emitter.playing = emitter.playOnStart;
            }

            if (emitter.playing)
            {
                const float previousElapsed = emitter.elapsed;
                emitter.elapsed += emitterDelta;

                if (emitter.burstMode)
                {
                    if (!emitter.hasTriggeredBurst)
                    {
                        EmitBurst(emitter, transform, _registry, entityHandle);
                        emitter.hasTriggeredBurst = true;

                        if (emitter.looping)
                        {
                            emitter.elapsed = 0.0f;
                        }
                        else
                        {
                            if (_editorPreview)
                                emitter.playing = false;
                            else
                                emitter.Stop();
                        }
                    }
                    else if (emitter.looping &&
                             emitter.elapsed >= std::max(emitter.emissionDuration, 0.01f))
                    {
                        const double period = std::max(emitter.emissionDuration, .01f);
                        const double due = std::floor(emitter.elapsed / period);
                        const int budget =
                            std::max(0, emitter.maxSpawnPerFrame - emitter.spawnedThisFrame);
                        const int size = std::max(1, emitter.burstCount);
                        const int bursts =
                            int(std::min(due, double(budget > 0 ? 1 + (budget - 1) / size : 0)));
                        for (int i = 0; i < bursts; ++i)
                            EmitBurst(emitter, transform, _registry, entityHandle);
                        emitter.totalDropped +=
                            uint64_t(std::min((due - bursts) * std::max(0, emitter.burstCount),
                                              double(UINT64_MAX / 2)));
                        emitter.elapsed = float(std::fmod(emitter.elapsed, period));
                    }
                }
                else
                {
                    const float activeTime =
                        emitter.looping
                            ? emitterDelta
                            : std::min(emitterDelta,
                                       std::max(0.f, emitter.emissionDuration - previousElapsed));
                    EmitContinuous(emitter, transform, _registry, entityHandle, activeTime,
                                   emitterDelta - activeTime);

                    if (!emitter.looping &&
                        emitter.elapsed >= std::max(emitter.emissionDuration, 0.0f))
                    {
                        if (_editorPreview)
                            emitter.playing = false;
                        else
                            emitter.Stop();
                    }
                }
            }

            if (!_editorPreview && emitter.pendingDestroy && emitter.particles.empty())
                emitter.entity->Destroy();
        }
        // Spawning after registry iteration avoids invalidating the particle view.
        // Targets are normal authored emitters; their caps also bound impact chains.
        for (const auto &impact : m_impacts)
        {
            if (!_registry.valid(impact.emitter) ||
                !_registry.all_of<ParticleEmitter, Transform>(impact.emitter))
                continue;
            auto &target = _registry.get<ParticleEmitter>(impact.emitter);
            auto &targetTransform = _registry.get<Transform>(impact.emitter);
            if (!target.entity || !target.entity->Active() ||
                !targetTransform.IsActiveInHierarchy())
                continue;
            CleanupDeadParticles(target, _registry);
            Transform frame;
            frame.position = impact.position + impact.normal * .015f;
            Vector3 normal = glm::normalize(impact.normal);
            auto cross = glm::cross(Vector3(0, 1, 0), normal);
            frame.rotation =
                normal.y < -.9999f
                    ? glm::angleAxis(3.14159265f, Vector3(1, 0, 0))
                    : glm::normalize(Quaternion(1 + normal.y, cross.x, cross.y, cross.z));
            EmitBurst(target, frame, _registry, impact.emitter, true);
        }
    }
} // namespace Canis
