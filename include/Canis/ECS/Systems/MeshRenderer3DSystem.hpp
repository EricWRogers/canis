#pragma once
#include <Canis/System.hpp>
#include <Canis/Math.hpp>

namespace Canis
{
    class Shader;

    class MeshRenderer3DSystem : public System
    {
    public:
        MeshRenderer3DSystem() : System() { m_name = type_name<MeshRenderer3DSystem>(); }

        void Create() override;
        void Ready() override;
        void Update(entt::registry &_registry, float _deltaTime) override;
        void OnDestroy() override;

    private:
        static constexpr int kMaxPointLights = 8;
        static constexpr int kDirectionalShadowMapSize = 2048;

        Shader *m_shader = nullptr;
        Shader *m_skyboxShader = nullptr;
        Shader *m_shadowShader = nullptr;
        Shader *m_colliderDebugShader = nullptr;
        unsigned int m_skyboxVao = 0;
        unsigned int m_skyboxVbo = 0;
        unsigned int m_colliderDebugVao = 0;
        unsigned int m_colliderDebugVbo = 0;
        unsigned int m_debugGizmoVao = 0;
        unsigned int m_debugGizmoVbo = 0;
        unsigned int m_shadowFramebuffer = 0;
        unsigned int m_shadowDepthTexture = 0;
        Matrix4 m_shadowLightSpaceMatrix = Matrix4(1.0f);

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
