#pragma once

#include <Canis/Entity.hpp>
#include <Canis/System.hpp>

#include <string>
#include <vector>

namespace Canis
{
    struct ParticleEmitter
    {
    public:
        static constexpr const char* ScriptName = "Canis::ParticleEmitter";

        ParticleEmitter() = default;
        explicit ParticleEmitter(Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}
        void Destroy() {}

        bool playOnStart = true;
        bool playing = false;
        bool looping = false;
        bool burstMode = true;
        bool destroyEntityWhenDone = true;
        bool localSpace = false;
        int burstCount = 18;
        int spawnCount = 2;
        float emissionDuration = 0.08f;
        float spawnInterval = 0.03f;
        float particleLifetime = 0.6f;
        float gravity = -3.5f;
        float drag = 1.8f;
        float speedMin = 1.2f;
        float speedMax = 3.8f;
        Vector3 spawnExtents = Vector3(0.15f);
        // Authored in emitter-local axes. Canis forward is negative Z.
        Vector3 minVelocity = Vector3(-1.0f, -0.2f, -1.0f);
        Vector3 maxVelocity = Vector3(1.0f, 1.3f, 1.0f);
        Vector3 startScaleMin = Vector3(0.08f);
        Vector3 startScaleMax = Vector3(0.18f);
        Vector3 endScale = Vector3(0.01f);
        Color startColor = Color(1.0f, 0.45f, 0.35f, 1.0f);
        Color endColor = Color(0.15f, 0.02f, 0.02f, 0.0f);
        std::string modelPath = "assets/defaults/models/cube.glb";
        std::string materialPath = "assets/defaults/materials/default.material";
        bool castShadow = false;

        void Play()
        {
            playing = true;
            initialized = true;
            elapsed = 0.0f;
            spawnAccumulator = 0.0f;
            hasTriggeredBurst = false;
            pendingDestroy = false;
        }

        void Stop()
        {
            playing = false;
            pendingDestroy = destroyEntityWhenDone;
        }

        // Runtime-only registry handles. Individual particles deliberately do
        // not have Canis::Entity wrappers, UUIDs, hierarchy rows, or scene
        // serialization state.
        std::vector<entt::entity> particles = {};
        float elapsed = 0.0f;
        float spawnAccumulator = 0.0f;
        bool initialized = false;
        bool hasTriggeredBurst = false;
        bool pendingDestroy = false;
        bool editorPreviewSelected = false;
        int cachedModelId = -1;
        int cachedMaterialId = -1;
    };

    class ParticleEmitterSystem : public System
    {
    public:
        ParticleEmitterSystem();
        void Create() override;
        void Update(entt::registry& _registry, float _deltaTime) override;
        void UpdateEditorPreview(entt::registry& _registry, float _deltaTime, Entity* _selectedEntity);

    private:
        void UpdateInternal(entt::registry& _registry, float _deltaTime, Entity* _previewEmitter, bool _editorPreview);
    };

    void RegisterParticleEmitterComponent(App& _app);
    void UnRegisterParticleEmitterComponent(App& _app);

    void RegisterParticleEmitterSystem(App& _app);
    void UnRegisterParticleEmitterSystem(App& _app);
}
