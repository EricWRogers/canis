#include <Canis/VFX/Particles.hpp>

#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Editor.hpp>

#include <algorithm>
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
            bool localSpace = false;
            Color startColor = Color(1.0f);
            Color endColor = Color(1.0f);
        };

        std::mt19937& GetParticleRng()
        {
            static thread_local std::mt19937 rng(std::random_device{}());
            return rng;
        }

        float RandomRange(const float _minValue, const float _maxValue)
        {
            // Inspector-authored ranges remain valid even when a negative
            // forward range is entered in descending numeric order.
            std::uniform_real_distribution<float> distribution(
                std::min(_minValue, _maxValue),
                std::max(_minValue, _maxValue));
            return distribution(GetParticleRng());
        }

        Vector3 RandomVector3(const Vector3& _minValue, const Vector3& _maxValue)
        {
            return Vector3(
                RandomRange(_minValue.x, _maxValue.x),
                RandomRange(_minValue.y, _maxValue.y),
                RandomRange(_minValue.z, _maxValue.z));
        }

        void ResolveEmitterAssets(ParticleEmitter& _emitter)
        {
            if (_emitter.cachedModelId < 0 && !_emitter.modelPath.empty())
                _emitter.cachedModelId = AssetManager::LoadModel(_emitter.modelPath);

            if (_emitter.cachedMaterialId < 0 && !_emitter.materialPath.empty())
                _emitter.cachedMaterialId = AssetManager::LoadMaterial(_emitter.materialPath);
        }

        void CleanupDeadParticles(ParticleEmitter& _emitter, const entt::registry& _registry)
        {
            _emitter.particles.erase(
                std::remove_if(
                    _emitter.particles.begin(),
                    _emitter.particles.end(),
                    [&_registry](const entt::entity _entityHandle)
                    {
                        return !_registry.valid(_entityHandle);
                    }),
                _emitter.particles.end());
        }

        void DestroyEmitterParticles(ParticleEmitter& _emitter, entt::registry& _registry)
        {
            for (const entt::entity particleHandle : _emitter.particles)
            {
                if (_registry.valid(particleHandle))
                    _registry.destroy(particleHandle);
            }

            _emitter.particles.clear();
        }

        bool UpdateLocalParticleTransform(
            SimpleParticle& _particle,
            Transform& _particleTransform,
            entt::registry& _registry,
            const Vector3& _particleScale)
        {
            if (!_registry.valid(_particle.emitterHandle) ||
                !_registry.all_of<Transform>(_particle.emitterHandle))
            {
                return false;
            }

            const Transform& emitterTransform = _registry.get<Transform>(_particle.emitterHandle);
            if (!emitterTransform.IsActiveInHierarchy())
                return false;

            const Vector4 worldPosition = emitterTransform.GetModelMatrix() * Vector4(_particle.localPosition, 1.0f);
            _particleTransform.position = Vector3(worldPosition);
            _particleTransform.rotation = emitterTransform.GetGlobalRotation();
            _particleTransform.scale = emitterTransform.GetGlobalScale() * _particleScale;
            return true;
        }

        void SpawnParticle(
            ParticleEmitter& _emitter,
            Transform& _emitterTransform,
            entt::registry& _registry,
            const entt::entity _emitterHandle)
        {
            ResolveEmitterAssets(_emitter);

            const entt::entity particleHandle = _registry.create();
            Transform& particleTransform = _registry.emplace<Transform>(particleHandle);
            Model& particleModel = _registry.emplace<Model>(particleHandle);
            Material& particleMaterial = _registry.emplace<Material>(particleHandle);
            SimpleParticle& particle = _registry.emplace<SimpleParticle>(particleHandle);

            const Vector3 randomOffset = RandomVector3(-_emitter.spawnExtents, _emitter.spawnExtents);
            const Vector3 randomDirection = RandomVector3(_emitter.minVelocity, _emitter.maxVelocity);
            const float directionLength = glm::length(randomDirection);
            const Vector3 normalizedDirection = (directionLength > 0.0001f)
                ? (randomDirection / directionLength)
                : Vector3(0.0f, 1.0f, 0.0f);
            const float speed = RandomRange(_emitter.speedMin, _emitter.speedMax);
            const Vector3 startScale = RandomVector3(_emitter.startScaleMin, _emitter.startScaleMax);
            const Quaternion emitterRotation = _emitterTransform.GetGlobalRotation();

            if (_emitter.localSpace && _emitter.entity != nullptr)
            {
                particle.localSpace = true;
                particle.emitterHandle = _emitterHandle;
                particle.localPosition = randomOffset;
                UpdateLocalParticleTransform(particle, particleTransform, _registry, startScale);
            }
            else
            {
                const Vector4 worldPosition = _emitterTransform.GetModelMatrix() * Vector4(randomOffset, 1.0f);
                particleTransform.position = Vector3(worldPosition);
                particleTransform.rotation = emitterRotation;
                particleTransform.scale = startScale;
            }

            particleModel.modelId = _emitter.cachedModelId;
            particleModel.color = _emitter.startColor;
            particleModel.castShadow = _emitter.castShadow;

            particleMaterial.materialId = _emitter.cachedMaterialId;
            particleMaterial.color = _emitter.startColor;
            particleMaterial.materialFields.SetFloat("particleAge", 0.0f);
            particleMaterial.materialFields.SetFloat("particleLifetime", std::max(_emitter.particleLifetime, 0.001f));
            particleMaterial.materialFields.SetFloat("particleNormalizedAge", 0.0f);

            // Velocity ranges are always authored relative to the emitter.
            // World-space particles inherit its orientation once at spawn;
            // local-space particles continue following it for their lifetime.
            particle.velocity = (_emitter.localSpace
                ? normalizedDirection
                : glm::normalize(emitterRotation * normalizedDirection)) * speed;
            particle.gravity = _emitter.gravity;
            particle.drag = std::max(_emitter.drag, 0.0f);
            particle.age = 0.0f;
            particle.lifetime = std::max(_emitter.particleLifetime, 0.001f);
            particle.startScale = startScale;
            particle.endScale = _emitter.endScale;
            particle.startColor = _emitter.startColor;
            particle.endColor = _emitter.endColor;

            _emitter.particles.push_back(particleHandle);
        }

        void EmitBurst(
            ParticleEmitter& _emitter,
            Transform& _emitterTransform,
            entt::registry& _registry,
            const entt::entity _emitterHandle)
        {
            const int particlesToSpawn = std::max(_emitter.burstCount, 0);
            for (int i = 0; i < particlesToSpawn; ++i)
                SpawnParticle(_emitter, _emitterTransform, _registry, _emitterHandle);
        }

        void EmitContinuous(
            ParticleEmitter& _emitter,
            Transform& _emitterTransform,
            entt::registry& _registry,
            const entt::entity _emitterHandle,
            const float _deltaTime)
        {
            _emitter.spawnAccumulator += _deltaTime;
            const float interval = std::max(_emitter.spawnInterval, 0.001f);
            const int particlesPerStep = std::max(_emitter.spawnCount, 1);

            while (_emitter.spawnAccumulator >= interval)
            {
                _emitter.spawnAccumulator -= interval;
                for (int i = 0; i < particlesPerStep; ++i)
                    SpawnParticle(_emitter, _emitterTransform, _registry, _emitterHandle);
            }
        }
    }

    void RegisterParticleEmitterComponent(App& _app)
    {
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, playOnStart);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, looping);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, burstMode);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, destroyEntityWhenDone);
        REGISTER_PROPERTY(particleEmitterConf, Canis::ParticleEmitter, localSpace);
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

        DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(particleEmitterConf, Canis::ParticleEmitter, Canis::Transform);

        particleEmitterConf.DrawInspector = [](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void
        {
            Canis::ParticleEmitter* component = _entity.HasComponent<Canis::ParticleEmitter>()
                ? &_entity.GetComponent<Canis::ParticleEmitter>()
                : nullptr;
            if (component == nullptr)
                return;

            DrawInspectorField(_editor, "playOnStart", _conf.name.c_str(), component->playOnStart);
            DrawInspectorField(_editor, "looping", _conf.name.c_str(), component->looping);
            DrawInspectorField(_editor, "burstMode", _conf.name.c_str(), component->burstMode);
            DrawInspectorField(_editor, "destroyEntityWhenDone", _conf.name.c_str(), component->destroyEntityWhenDone);
            DrawInspectorField(_editor, "localSpace", _conf.name.c_str(), component->localSpace);
            DrawInspectorField(_editor, "burstCount", _conf.name.c_str(), component->burstCount);
            DrawInspectorField(_editor, "spawnCount", _conf.name.c_str(), component->spawnCount);
            DrawInspectorField(_editor, "emissionDuration", _conf.name.c_str(), component->emissionDuration);
            DrawInspectorField(_editor, "spawnInterval", _conf.name.c_str(), component->spawnInterval);
            DrawInspectorField(_editor, "particleLifetime", _conf.name.c_str(), component->particleLifetime);
            DrawInspectorField(_editor, "gravity", _conf.name.c_str(), component->gravity);
            DrawInspectorField(_editor, "drag", _conf.name.c_str(), component->drag);
            DrawInspectorField(_editor, "speedMin", _conf.name.c_str(), component->speedMin);
            DrawInspectorField(_editor, "speedMax", _conf.name.c_str(), component->speedMax);
            DrawInspectorField(_editor, "spawnExtents", _conf.name.c_str(), component->spawnExtents);
            DrawInspectorField(_editor, "minVelocity", _conf.name.c_str(), component->minVelocity);
            DrawInspectorField(_editor, "maxVelocity", _conf.name.c_str(), component->maxVelocity);
            DrawInspectorField(_editor, "startScaleMin", _conf.name.c_str(), component->startScaleMin);
            DrawInspectorField(_editor, "startScaleMax", _conf.name.c_str(), component->startScaleMax);
            DrawInspectorField(_editor, "endScale", _conf.name.c_str(), component->endScale);
            DrawInspectorField(_editor, "startColor", _conf.name.c_str(), component->startColor);
            DrawInspectorField(_editor, "endColor", _conf.name.c_str(), component->endColor);

            auto drawAssetPathDropTarget = [](const char* _label, std::string& _path, int& _cachedId, const MetaFileAsset::FileType _expectedType, auto _loader) -> void
            {
                std::string assetLabel = "[ empty ]";
                if (!_path.empty())
                {
                    if (MetaFileAsset* meta = AssetManager::GetMetaFile(_path))
                        assetLabel = meta->name;
                    else
                        assetLabel = _path;
                }

                ImGui::Text("%s", _label);
                ImGui::SameLine();
                ImGui::Button(assetLabel.c_str(), ImVec2(180, 0));

                if (!ImGui::BeginDragDropTarget())
                    return;

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                    std::string path = std::string(dropped.path);
                    if (path.empty())
                        path = AssetManager::GetPath(dropped.uuid);

                    if (MetaFileAsset* meta = AssetManager::GetMetaFile(path))
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
                "model",
                component->modelPath,
                component->cachedModelId,
                MetaFileAsset::FileType::MODEL,
                [](const std::string& _path) -> int { return AssetManager::LoadModel(_path); });

            drawAssetPathDropTarget(
                "material",
                component->materialPath,
                component->cachedMaterialId,
                MetaFileAsset::FileType::MATERIAL,
                [](const std::string& _path) -> int { return AssetManager::LoadMaterial(_path); });

            DrawInspectorField(_editor, "castShadow", _conf.name.c_str(), component->castShadow);
        };

        _app.RegisterComponent(particleEmitterConf);
    }

    DEFAULT_UNREGISTER_COMPONENT(particleEmitterConf, ParticleEmitter)

    void RegisterParticleEmitterSystem(App& _app)
    {
        DEFAULT_SYSTEM_CONFIG(particleEmitterSystemConf, Canis::ParticleEmitterSystem, Canis::SystemPipeline::Update);
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

    void ParticleEmitterSystem::Update(entt::registry& _registry, float _deltaTime)
    {
        UpdateInternal(_registry, _deltaTime, nullptr, false);
    }

    void ParticleEmitterSystem::UpdateEditorPreview(
        entt::registry& _registry,
        float _deltaTime,
        Entity* _selectedEntity)
    {
        UpdateInternal(_registry, _deltaTime, _selectedEntity, true);
    }

    void ParticleEmitterSystem::UpdateInternal(
        entt::registry& _registry,
        float _deltaTime,
        Entity* _previewEmitter,
        bool _editorPreview)
    {
        std::vector<entt::entity> expiredParticles = {};
        auto particleView = _registry.view<SimpleParticle, Transform, Model, Material>();
        for (auto [entityHandle, particle, transform, model, material] : particleView.each())
        {
            particle.age += _deltaTime;
            particle.velocity.y += particle.gravity * _deltaTime;
            particle.velocity *= std::max(0.0f, 1.0f - (particle.drag * _deltaTime));

            const float t = Clamp01(particle.age / std::max(particle.lifetime, 0.001f));
            const Vector3 particleScale = glm::mix(particle.startScale, particle.endScale, t);

            if (particle.localSpace)
            {
                particle.localPosition += particle.velocity * _deltaTime;
                if (!UpdateLocalParticleTransform(particle, transform, _registry, particleScale))
                {
                    expiredParticles.push_back(entityHandle);
                    continue;
                }
            }
            else
            {
                transform.position += particle.velocity * _deltaTime;
                transform.scale = particleScale;
            }

            const Color color = glm::mix(particle.startColor, particle.endColor, t);
            model.color = color;
            material.color = color;
            material.materialFields.SetFloat("particleAge", particle.age);
            material.materialFields.SetFloat("particleLifetime", particle.lifetime);
            material.materialFields.SetFloat("particleNormalizedAge", t);

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
            if (emitter.entity == nullptr || !emitter.entity->Active())
                continue;

            CleanupDeadParticles(emitter, _registry);

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
                    emitter.Play();
                }
                else if (!emitter.playing && emitter.particles.empty())
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
                emitter.elapsed += _deltaTime;

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
                    else if (emitter.looping && emitter.elapsed >= std::max(emitter.emissionDuration, 0.01f))
                    {
                        emitter.elapsed = 0.0f;
                        emitter.hasTriggeredBurst = false;
                    }
                }
                else
                {
                    EmitContinuous(emitter, transform, _registry, entityHandle, _deltaTime);

                    if (!emitter.looping && emitter.elapsed >= std::max(emitter.emissionDuration, 0.0f))
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
    }
}
