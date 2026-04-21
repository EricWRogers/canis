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
            Entity* entity = nullptr;
            Vector3 velocity = Vector3(0.0f);
            float gravity = 0.0f;
            float drag = 0.0f;
            float age = 0.0f;
            float lifetime = 0.5f;
            Vector3 startScale = Vector3(1.0f);
            Vector3 endScale = Vector3(0.0f);
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
            std::uniform_real_distribution<float> distribution(_minValue, _maxValue);
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

        void CleanupDeadParticles(ParticleEmitter& _emitter)
        {
            _emitter.particles.erase(
                std::remove_if(
                    _emitter.particles.begin(),
                    _emitter.particles.end(),
                    [](Entity* _entity)
                    {
                        return _entity == nullptr || !_entity->active;
                    }),
                _emitter.particles.end());
        }

        void SpawnParticle(ParticleEmitter& _emitter, Transform& _emitterTransform, Scene& _scene)
        {
            ResolveEmitterAssets(_emitter);

            Entity* particleEntity = _scene.CreateEntity("Particle");
            if (particleEntity == nullptr)
                return;

            Transform* particleTransform = particleEntity->AddComponent<Transform>();
            Model* particleModel = particleEntity->AddComponent<Model>();
            Material* particleMaterial = particleEntity->AddComponent<Material>();
            SimpleParticle* particle = particleEntity->AddComponent<SimpleParticle>();

            if (particleTransform == nullptr || particleModel == nullptr || particleMaterial == nullptr || particle == nullptr)
                return;

            const Vector3 randomOffset = RandomVector3(-_emitter.spawnExtents, _emitter.spawnExtents);
            const Vector3 randomDirection = RandomVector3(_emitter.minVelocity, _emitter.maxVelocity);
            const float directionLength = glm::length(randomDirection);
            const Vector3 normalizedDirection = (directionLength > 0.0001f)
                ? (randomDirection / directionLength)
                : Vector3(0.0f, 1.0f, 0.0f);
            const float speed = RandomRange(_emitter.speedMin, _emitter.speedMax);
            const Vector3 startScale = RandomVector3(_emitter.startScaleMin, _emitter.startScaleMax);

            if (_emitter.localSpace && _emitter.entity != nullptr)
            {
                particleTransform->SetParent(_emitter.entity);
                particleTransform->position = randomOffset;
                particleTransform->rotation = Vector3(0.0f);
                particleTransform->scale = startScale;
            }
            else
            {
                particleTransform->position = _emitterTransform.GetGlobalPosition() + randomOffset;
                particleTransform->rotation = Vector3(0.0f);
                particleTransform->scale = startScale;
            }

            particleModel->modelId = _emitter.cachedModelId;
            particleModel->color = _emitter.startColor;

            particleMaterial->materialId = _emitter.cachedMaterialId;
            particleMaterial->color = _emitter.startColor;

            particle->velocity = normalizedDirection * speed;
            particle->gravity = _emitter.gravity;
            particle->drag = std::max(_emitter.drag, 0.0f);
            particle->age = 0.0f;
            particle->lifetime = std::max(_emitter.particleLifetime, 0.001f);
            particle->startScale = startScale;
            particle->endScale = _emitter.endScale;
            particle->startColor = _emitter.startColor;
            particle->endColor = _emitter.endColor;

            _emitter.particles.push_back(particleEntity);
        }

        void EmitBurst(ParticleEmitter& _emitter, Transform& _emitterTransform, Scene& _scene)
        {
            const int particlesToSpawn = std::max(_emitter.burstCount, 0);
            for (int i = 0; i < particlesToSpawn; ++i)
                SpawnParticle(_emitter, _emitterTransform, _scene);
        }

        void EmitContinuous(ParticleEmitter& _emitter, Transform& _emitterTransform, Scene& _scene, const float _deltaTime)
        {
            _emitter.spawnAccumulator += _deltaTime;
            const float interval = std::max(_emitter.spawnInterval, 0.001f);
            const int particlesPerStep = std::max(_emitter.spawnCount, 1);

            while (_emitter.spawnAccumulator >= interval)
            {
                _emitter.spawnAccumulator -= interval;
                for (int i = 0; i < particlesPerStep; ++i)
                    SpawnParticle(_emitter, _emitterTransform, _scene);
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

    void ParticleEmitterSystem::Create()
    {
        m_name = "Canis::ParticleEmitterSystem";
    }

    void ParticleEmitterSystem::Update(entt::registry& _registry, float _deltaTime)
    {
        auto particleView = _registry.view<SimpleParticle, Transform, Model, Material>();
        for (auto [entityHandle, particle, transform, model, material] : particleView.each())
        {
            if (particle.entity == nullptr || !particle.entity->active)
                continue;

            particle.age += _deltaTime;
            particle.velocity.y += particle.gravity * _deltaTime;
            particle.velocity *= std::max(0.0f, 1.0f - (particle.drag * _deltaTime));
            transform.position += particle.velocity * _deltaTime;

            const float t = Clamp01(particle.age / std::max(particle.lifetime, 0.001f));
            transform.scale = glm::mix(particle.startScale, particle.endScale, t);
            const Color color = glm::mix(particle.startColor, particle.endColor, t);
            model.color = color;
            material.color = color;

            if (particle.age >= particle.lifetime)
                particle.entity->Destroy();
        }

        auto emitterView = _registry.view<Canis::ParticleEmitter, Transform>();
        for (auto [entityHandle, emitter, transform] : emitterView.each())
        {
            if (emitter.entity == nullptr || !emitter.entity->active)
                continue;

            CleanupDeadParticles(emitter);

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
                        EmitBurst(emitter, transform, GetScene());
                        emitter.hasTriggeredBurst = true;

                        if (emitter.looping)
                        {
                            emitter.elapsed = 0.0f;
                        }
                        else
                        {
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
                    EmitContinuous(emitter, transform, GetScene(), _deltaTime);

                    if (!emitter.looping && emitter.elapsed >= std::max(emitter.emissionDuration, 0.0f))
                        emitter.Stop();
                }
            }

            if (emitter.pendingDestroy && emitter.particles.empty())
                emitter.entity->Destroy();
        }
    }
}
