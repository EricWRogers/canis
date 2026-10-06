#pragma once
#include <Canis/System.hpp>
#include <Canis/Math.hpp>
#include <memory>

namespace Canis
{
    class Shader;
    struct MeshRendererCache;

    class MeshRenderer3DSystem : public System
    {
    public:
        MeshRenderer3DSystem() : System() { m_name = type_name<MeshRenderer3DSystem>(); }

        void InvalidateStaticShadowCache() { m_staticShadowValid=false; }
        bool HasStaticShadowCache() const { return m_staticShadowValid; }
        void Create() override;
        void Ready() override;
        void Update(entt::registry &_registry, float _deltaTime) override;
        void OnDestroy() override;

        // Share the light-space map across eye renders, refreshing on every stereo frame.
        void BeginStereoFrame(const Vector3& shadowCenter)
        {
            m_stereoFrame = true;
            m_stereoShadowReady = false;
            m_stereoPrepared = false;
            m_stereoShadowCenter = shadowCenter;
        }
        void EndStereoFrame() { m_stereoFrame = m_stereoShadowReady = m_stereoPrepared = false; m_stereoPass = 0; }
        // 0: normal, 1: opaque geometry into two indexed viewports, 2: per-eye remainder.
        void SetStereoPass(int pass) { m_stereoPass = pass; }
        void SetStereoViews(const Matrix4& left, const Matrix4& right,
                            const Matrix4& leftProjection, const Matrix4& rightProjection,
                            const Vector3& leftEye, const Vector3& rightEye) {
            m_stereoViews[0]=left; m_stereoViews[1]=right;
            m_stereoProjections[0]=leftProjection; m_stereoProjections[1]=rightProjection;
            m_stereoEyes[0]=leftEye; m_stereoEyes[1]=rightEye;
        }

    private:
        int m_stereoPass = 0;
        Matrix4 m_stereoViews[2]{Matrix4(1),Matrix4(1)}, m_stereoProjections[2]{Matrix4(1),Matrix4(1)};
        Vector3 m_stereoEyes[2]{};
        bool m_stereoFrame = false;
        bool m_stereoPrepared = false;
        bool m_stereoShadowReady = false;
        Vector3 m_stereoShadowCenter{0.0f};
        std::shared_ptr<MeshRendererCache> m_batchCache;
        const Matrix4& RenderMatrix(entt::entity handle, const Transform& transform);
        static constexpr int kMaxPointLights = 8;
        static constexpr int kDirectionalShadowMapSize = 2048;

        Shader *m_shader = nullptr;
        Shader *m_trailShader = nullptr;
        unsigned int m_trailVao = 0, m_trailVbo = 0;
        void DrawTrails(entt::registry& registry, const Matrix4& projection, const Matrix4& view, Vector3 cameraPosition);
        Shader *m_skyboxShader = nullptr;
        Shader *m_shadowShader = nullptr;
        Shader *m_colliderDebugShader = nullptr;
        unsigned int m_skyboxVao = 0;
        unsigned int m_skyboxVbo = 0;
        unsigned int m_colliderDebugVao = 0;
        unsigned int m_colliderDebugVbo = 0;
        unsigned int m_debugGizmoVao = 0;
        unsigned int m_debugGizmoVbo = 0;
        unsigned int m_materialSampler = 0;
        unsigned int m_softDepthTexture = 0;
        int m_softDepthWidth = 0, m_softDepthHeight = 0;
        float m_effectTime = 0;
        unsigned int m_shadowFramebuffer = 0;
        unsigned int m_shadowDepthTexture = 0;
        Matrix4 m_shadowLightSpaceMatrix = Matrix4(1.0f);
        unsigned int m_staticShadowTexture=0, m_staticShadowFramebuffer=0;
        bool m_staticShadowValid=false;
        Matrix4 m_staticShadowMatrix{1};
        int m_staticShadowProgram=0;
        struct StaticShadowCaster {
            const void* asset=nullptr;
            uint64_t revision=0;
            int node=-1;
            bool applyNode=true;
            Matrix4 matrix{1};
            bool operator==(const StaticShadowCaster&) const = default;
        };
        std::vector<StaticShadowCaster> m_staticShadowCasters;


        void CreateSkyboxGeometry();
        void DrawSkybox(const Matrix4 &_projection, const Matrix4 &_view);
        void CreateColliderDebugGeometry();
        void DrawColliderDebugLines(entt::registry &_registry, const Matrix4 &_projection, const Matrix4 &_view);
        void CreateDebugGizmoGeometry();
        void DrawDebugGizmoLines(const Matrix4 &_projection, const Matrix4 &_view);
        void CreateShadowMap();
        void DestroyShadowMap();
        void RenderDirectionalShadowMap(entt::registry &_registry, const Matrix4 &_projection, const Matrix4 &_view, const Vector3 &_cameraPosition, float _cameraFarClip, const Vector3 &_directionalLightDirection, bool _useDirectionalLight);
    };
} // end of Canis namespace
