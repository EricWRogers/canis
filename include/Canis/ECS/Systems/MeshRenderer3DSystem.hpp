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
        Shader *m_shader = nullptr;
        Shader *m_skyboxShader = nullptr;
        unsigned int m_skyboxVao = 0;
        unsigned int m_skyboxVbo = 0;

        void CreateSkyboxGeometry();
        void DrawSkybox(const Matrix4 &_projection, const Matrix4 &_view);
    };
} // end of Canis namespace
