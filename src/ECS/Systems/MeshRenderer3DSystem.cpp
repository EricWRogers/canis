#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>

#include <Canis/AssetManager.hpp>
#include <Canis/Entity.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Shader.hpp>
#include <Canis/Time.hpp>
#include <Canis/Window.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <tuple>

namespace Canis
{
    namespace
    {
        constexpr int kMaxPointLights = 8;
        constexpr int kDirectionalShadowMapSize = 2048;

        static const float kSkyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        struct DirectionalLightState
        {
            bool enabled = false;
            Vector3 direction = Vector3(-0.4f, -1.0f, -0.25f);
            Vector3 color = Vector3(1.0f, 0.98f, 0.95f);
            float intensity = 1.0f;
        };

        struct PointLightState
        {
            Vector3 position = Vector3(0.0f);
            Vector3 color = Vector3(1.0f);
            float intensity = 1.0f;
            float range = 12.0f;
        };

        struct TransparentModelEntry
        {
            entt::entity entityHandle = entt::null;
            float distanceSquared = 0.0f;
        };

        struct StaticModelBatchKey
        {
            i32 modelId = -1;
            i32 materialId = -1;
            i32 nodeIndex = -1;
            bool applyNodeTransform = true;
            float modelColorR = 1.0f;
            float modelColorG = 1.0f;
            float modelColorB = 1.0f;
            float modelColorA = 1.0f;
            float materialColorR = 1.0f;
            float materialColorG = 1.0f;
            float materialColorB = 1.0f;
            float materialColorA = 1.0f;

            bool operator<(const StaticModelBatchKey &_other) const
            {
                return std::tie(
                    modelId,
                    materialId,
                    nodeIndex,
                    applyNodeTransform,
                    modelColorR,
                    modelColorG,
                    modelColorB,
                    modelColorA,
                    materialColorR,
                    materialColorG,
                    materialColorB,
                    materialColorA) <
                    std::tie(
                        _other.modelId,
                        _other.materialId,
                        _other.nodeIndex,
                        _other.applyNodeTransform,
                        _other.modelColorR,
                        _other.modelColorG,
                        _other.modelColorB,
                        _other.modelColorA,
                        _other.materialColorR,
                        _other.materialColorG,
                        _other.materialColorB,
                        _other.materialColorA);
            }
        };

        struct StaticModelBatch
        {
            StaticModelBatchKey key = {};
            std::vector<Matrix4> modelMatrices = {};
            std::vector<TransparentModelEntry> sourceEntries = {};
            float distanceSquared = 0.0f;
        };

        bool UsesTransparentColor(const Color &_color)
        {
            return _color.a < 0.999f;
        }

        bool MaterialAssetUsesTransparency(MaterialAsset *_materialAsset)
        {
            return _materialAsset != nullptr &&
                (((
                    _materialAsset->info & MATERIAL_HAS_COLOR) != 0u) &&
                    UsesTransparentColor(_materialAsset->color));
        }

        bool EntityUsesTransparency(entt::registry &_registry, entt::entity _entityHandle)
        {
            Model *model = _registry.try_get<Model>(_entityHandle);
            if (model != nullptr && UsesTransparentColor(model->color))
                return true;

            Material *material = _registry.try_get<Material>(_entityHandle);
            if (material == nullptr)
                return false;

            if (UsesTransparentColor(material->color))
                return true;

            if (material->materialId >= 0 && MaterialAssetUsesTransparency(AssetManager::GetMaterial(material->materialId)))
                return true;

            for (const i32 slotMaterialId : material->materialIds)
            {
                if (slotMaterialId >= 0 && MaterialAssetUsesTransparency(AssetManager::GetMaterial(slotMaterialId)))
                    return true;
            }

            return false;
        }

        bool HasMaterialFields(const MaterialFields &_fields)
        {
            return !_fields.GetIntUniforms().empty() ||
                !_fields.GetFloatUniforms().empty() ||
                !_fields.GetVec2Uniforms().empty() ||
                !_fields.GetVec3Uniforms().empty() ||
                !_fields.GetVec4Uniforms().empty() ||
                !_fields.GetColorUniforms().empty() ||
                !_fields.GetTextureUniforms().empty();
        }

        DirectionalLightState GatherDirectionalLight(entt::registry &_registry)
        {
            DirectionalLightState state = {};

            auto directionalLightView = _registry.view<DirectionalLight>();
            for (const entt::entity entityHandle : directionalLightView)
            {
                DirectionalLight &light = directionalLightView.get<DirectionalLight>(entityHandle);
                Entity *entity = light.entity;
                if (entity == nullptr || !entity->active)
                    continue;

                state.enabled = light.enabled;
                state.direction = light.direction;
                const float directionLength = glm::length(state.direction);
                if (directionLength > 0.0001f)
                    state.direction /= directionLength;
                else
                    state.direction = Vector3(0.0f, -1.0f, 0.0f);
                state.color = Vector3(light.color.r, light.color.g, light.color.b);
                state.intensity = light.intensity;
                break;
            }

            return state;
        }

        std::vector<PointLightState> GatherPointLights(entt::registry &_registry)
        {
            std::vector<PointLightState> lights = {};
            lights.reserve(kMaxPointLights);

            auto pointLightView = _registry.view<PointLight, Transform>();
            for (const entt::entity entityHandle : pointLightView)
            {
                if (lights.size() >= kMaxPointLights)
                    break;

                PointLight &light = pointLightView.get<PointLight>(entityHandle);
                Transform &lightTransform = pointLightView.get<Transform>(entityHandle);
                Entity *entity = light.entity;
                if (entity == nullptr)
                    entity = lightTransform.entity;

                if (entity == nullptr || !entity->active || !light.enabled)
                    continue;

                PointLightState state = {};
                state.position = lightTransform.GetGlobalPosition();
                state.color = Vector3(light.color.r, light.color.g, light.color.b);
                state.intensity = light.intensity;
                state.range = light.range;
                lights.push_back(state);
            }

            return lights;
        }
    } // namespace

    void MeshRenderer3DSystem::Create()
    {
        int id = AssetManager::LoadShader("assets/shaders/model3d");
        Shader *shader = AssetManager::Get<ShaderAsset>(id)->GetShader();

        if (!shader->IsLinked())
        {
            shader->AddAttribute("vertexPosition");
            shader->AddAttribute("vertexNormal");
            shader->AddAttribute("vertexUV");
            shader->Link();
        }

        m_shader = shader;

        int skyboxShaderId = AssetManager::LoadShader("assets/shaders/skybox");
        Shader *skyboxShader = AssetManager::Get<ShaderAsset>(skyboxShaderId)->GetShader();
        if (!skyboxShader->IsLinked())
        {
            skyboxShader->AddAttribute("aPos");
            skyboxShader->Link();
        }

        m_skyboxShader = skyboxShader;

        int shadowShaderId = AssetManager::LoadShader("assets/shaders/model3d_shadow");
        Shader *shadowShader = AssetManager::Get<ShaderAsset>(shadowShaderId)->GetShader();
        if (!shadowShader->IsLinked())
        {
            shadowShader->AddAttribute("vertexPosition");
            shadowShader->AddAttribute("vertexNormal");
            shadowShader->AddAttribute("vertexUV");
            shadowShader->Link();
        }

        m_shadowShader = shadowShader;
        CreateShadowMap();
        CreateSkyboxGeometry();
    }

    void MeshRenderer3DSystem::Ready() {}

    void MeshRenderer3DSystem::OnDestroy()
    {
        if (m_skyboxVbo != 0)
            glDeleteBuffers(1, &m_skyboxVbo);
        if (m_skyboxVao != 0)
            glDeleteVertexArrays(1, &m_skyboxVao);

        DestroyShadowMap();
        m_skyboxVbo = 0;
        m_skyboxVao = 0;
        m_skyboxShader = nullptr;
        m_shadowShader = nullptr;
        m_shader = nullptr;
    }

    void MeshRenderer3DSystem::CreateSkyboxGeometry()
    {
        if (m_skyboxVao == 0)
            glGenVertexArrays(1, &m_skyboxVao);

        if (m_skyboxVbo == 0)
            glGenBuffers(1, &m_skyboxVbo);

        glBindVertexArray(m_skyboxVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kSkyboxVertices), kSkyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void MeshRenderer3DSystem::CreateShadowMap()
    {
        if (m_shadowFramebuffer != 0 && m_shadowDepthTexture != 0)
            return;

        if (m_shadowDepthTexture != 0)
        {
            glDeleteTextures(1, &m_shadowDepthTexture);
            m_shadowDepthTexture = 0;
        }

        if (m_shadowFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_shadowFramebuffer);
            m_shadowFramebuffer = 0;
        }

        glGenFramebuffers(1, &m_shadowFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);

        glGenTextures(1, &m_shadowDepthTexture);
        glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            kDirectionalShadowMapSize,
            kDirectionalShadowMapSize,
            0,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_INT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowDepthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            Debug::Warning("Directional shadow framebuffer incomplete.");

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void MeshRenderer3DSystem::DestroyShadowMap()
    {
        if (m_shadowDepthTexture != 0)
        {
            glDeleteTextures(1, &m_shadowDepthTexture);
            m_shadowDepthTexture = 0;
        }

        if (m_shadowFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_shadowFramebuffer);
            m_shadowFramebuffer = 0;
        }
    }

    void MeshRenderer3DSystem::RenderDirectionalShadowMap(
        entt::registry &_registry,
        const Matrix4 &_projection,
        const Matrix4 &_view,
        const Vector3 &_cameraPosition,
        float _cameraFarClip,
        const Vector3 &_directionalLightDirection,
        bool _useDirectionalLight)
    {
        (void)_projection;
        (void)_view;

        if (m_shadowShader == nullptr || m_shadowFramebuffer == 0 || m_shadowDepthTexture == 0 || !_useDirectionalLight)
        {
            m_shadowLightSpaceMatrix = Matrix4(1.0f);
            return;
        }

        if (!m_shadowShader->IsLinked())
        {
            m_shadowShader->AddAttribute("vertexPosition");
            m_shadowShader->AddAttribute("vertexNormal");
            m_shadowShader->AddAttribute("vertexUV");
            m_shadowShader->Link();
        }

        const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
        const Vector3 lightUp = (std::abs(glm::dot(_directionalLightDirection, worldUp)) > 0.95f)
            ? Vector3(0.0f, 0.0f, 1.0f)
            : worldUp;
        const float shadowExtent = std::clamp(_cameraFarClip * 0.1f, 20.0f, 120.0f);
        const float shadowDistance = std::max(50.0f, shadowExtent * 2.0f);
        const Vector3 lightPosition = _cameraPosition - (_directionalLightDirection * shadowDistance);

        const Matrix4 lightView = glm::lookAt(lightPosition, _cameraPosition, lightUp);
        const Matrix4 lightProjection = glm::ortho(
            -shadowExtent,
            shadowExtent,
            -shadowExtent,
            shadowExtent,
            0.1f,
            shadowDistance * 2.0f);
        m_shadowLightSpaceMatrix = lightProjection * lightView;

        GLint previousViewport[4] = { 0, 0, 0, 0 };
        GLint previousFramebuffer = 0;
        glGetIntegerv(GL_VIEWPORT, previousViewport);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);

        const bool depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
        const bool blendEnabled = glIsEnabled(GL_BLEND);
        const bool cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        const bool polygonOffsetEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);

        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
        glViewport(0, 0, kDirectionalShadowMapSize, kDirectionalShadowMapSize);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);

        m_shadowShader->Use();
        m_shadowShader->SetMat4("lightSpaceMatrix", m_shadowLightSpaceMatrix);

        auto modelView = _registry.view<Transform, Model>();
        for (const entt::entity entityHandle : modelView)
        {
            Transform &transform = modelView.get<Transform>(entityHandle);
            Model &modelRenderer = modelView.get<Model>(entityHandle);
            Entity *entity = modelRenderer.entity;
            if (entity == nullptr)
                entity = transform.entity;

            if (entity == nullptr || !entity->active || modelRenderer.modelId < 0)
                continue;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                continue;

            const ModelAsset::Pose3D *pose = nullptr;
            if (ModelAnimation *animation = _registry.try_get<ModelAnimation>(entityHandle))
            {
                if (animation->poseModelId == modelRenderer.modelId)
                    pose = &animation->pose;
            }

            model->Draw(
                *m_shadowShader,
                transform.GetModelMatrix(),
                pose,
                -1,
                Color(1.0f),
                nullptr,
                modelRenderer.nodeIndex,
                modelRenderer.applyNodeTransform);
        }

        m_shadowShader->UnUse();

        if (polygonOffsetEnabled)
            glEnable(GL_POLYGON_OFFSET_FILL);
        else
            glDisable(GL_POLYGON_OFFSET_FILL);

        if (cullFaceEnabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);

        if (blendEnabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);

        if (depthTestEnabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
        glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    }

    void MeshRenderer3DSystem::DrawSkybox(const Matrix4 &_projection, const Matrix4 &_view)
    {
        if (m_skyboxShader == nullptr || m_skyboxVao == 0)
            return;

        if (!m_skyboxShader->IsLinked())
        {
            m_skyboxShader->AddAttribute("aPos");
            m_skyboxShader->Link();
        }

        const UUID skyboxUUID = scene->GetEnvironmentSkyboxUUID();
        if ((uint64_t)skyboxUUID == 0)
            return;

        const std::string skyboxPath = AssetManager::GetPath(skyboxUUID);
        if (skyboxPath == "Path was not found in AssetLibrary")
            return;

        SkyboxAsset *skybox = AssetManager::GetSkybox(skyboxPath);
        if (skybox == nullptr || !skybox->IsLoaded())
            return;

        const Matrix4 skyboxView = Matrix4(glm::mat3(_view));

        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        m_skyboxShader->Use();
        m_skyboxShader->SetMat4("projection", _projection);
        m_skyboxShader->SetMat4("view", skyboxView);
        m_skyboxShader->SetInt("skybox", 0);

        glBindVertexArray(m_skyboxVao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->GetTexture());
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glBindVertexArray(0);
        m_skyboxShader->UnUse();

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    void MeshRenderer3DSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (m_shader == nullptr)
            return;

        if (!m_shader->IsLinked())
        {
            m_shader->AddAttribute("vertexPosition");
            m_shader->AddAttribute("vertexNormal");
            m_shader->AddAttribute("vertexUV");
            m_shader->Link();
        }

        Matrix4 projection = Matrix4(1.0f);
        Matrix4 view = Matrix4(1.0f);
        Vector3 cameraPosition = Vector3(0.0f, 0.0f, 0.0f);
        float cameraNearClip = 0.1f;
        float cameraFarClip = 100.0f;

        if (scene->HasEditorCamera3DOverride())
        {
            projection = scene->GetEditorCamera3DProjection();
            view = scene->GetEditorCamera3DView();
            const Matrix4 invView = glm::inverse(view);
            cameraPosition = Vector3(invView[3][0], invView[3][1], invView[3][2]);
            cameraNearClip = 0.05f;
            cameraFarClip = 2000.0f;
        }
        else
        {
            Camera *camera = nullptr;
            Transform *cameraTransform = nullptr;

            auto cameraView = _registry.view<Camera, Transform>();
            for (const entt::entity entityHandle : cameraView)
            {
                Camera &candidateCamera = cameraView.get<Camera>(entityHandle);
                Transform &candidateTransform = cameraView.get<Transform>(entityHandle);

                Entity *entity = candidateCamera.entity;
                if (entity == nullptr)
                    entity = candidateTransform.entity;

                if (entity == nullptr || !entity->active)
                    continue;

                if (candidateCamera.primary)
                {
                    camera = &candidateCamera;
                    cameraTransform = &candidateTransform;
                    break;
                }

                if (camera == nullptr)
                {
                    camera = &candidateCamera;
                    cameraTransform = &candidateTransform;
                }
            }

            if (camera == nullptr || cameraTransform == nullptr)
            {
                scene->ClearLastRenderCamera();
                return;
            }

            const float aspect = (window->GetScreenHeight() > 0)
                ? (static_cast<float>(window->GetScreenWidth()) / static_cast<float>(window->GetScreenHeight()))
                : 1.0f;
            projection = glm::perspective(DEG2RAD * camera->fovDegrees, aspect, camera->nearClip, camera->farClip);
            cameraNearClip = camera->nearClip;
            cameraFarClip = camera->farClip;

            const Vector3 eye = cameraTransform->GetGlobalPosition();
            const Vector3 target = eye + cameraTransform->GetForward();
            const Vector3 up = cameraTransform->GetUp();
            view = glm::lookAt(eye, target, up);
            cameraPosition = eye;
        }

        scene->SetLastRenderCamera(view, projection, cameraPosition, cameraNearClip, cameraFarClip);

        DirectionalLightState directionalLight = GatherDirectionalLight(_registry);
        std::vector<PointLightState> pointLights = GatherPointLights(_registry);

        RenderDirectionalShadowMap(
            _registry,
            projection,
            view,
            cameraPosition,
            cameraFarClip,
            directionalLight.direction,
            directionalLight.enabled);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        DrawSkybox(projection, view);

        Shader *currentShader = nullptr;

        auto modelView = _registry.view<Transform, Model>();
        std::vector<TransparentModelEntry> opaqueEntities = {};
        std::vector<TransparentModelEntry> transparentEntities = {};
        opaqueEntities.reserve(modelView.size_hint());
        transparentEntities.reserve(modelView.size_hint());

        for (const entt::entity entityHandle : modelView)
        {
            Transform &transform = modelView.get<Transform>(entityHandle);
            Model &modelRenderer = modelView.get<Model>(entityHandle);
            Entity *entity = modelRenderer.entity;
            if (entity == nullptr)
                entity = transform.entity;

            if (entity == nullptr || !entity->active || modelRenderer.modelId < 0)
                continue;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                continue;

            const Vector3 offset = transform.GetGlobalPosition() - cameraPosition;
            const float distanceSquared = glm::dot(offset, offset);

            if (EntityUsesTransparency(_registry, entityHandle))
            {
                transparentEntities.push_back(TransparentModelEntry{
                    .entityHandle = entityHandle,
                    .distanceSquared = distanceSquared
                });
            }
            else
            {
                opaqueEntities.push_back(TransparentModelEntry{
                    .entityHandle = entityHandle,
                    .distanceSquared = distanceSquared
                });
            }
        }

        std::sort(
            opaqueEntities.begin(),
            opaqueEntities.end(),
            [](const TransparentModelEntry &_a, const TransparentModelEntry &_b)
            {
                return _a.distanceSquared < _b.distanceSquared;
            });

        std::sort(
            transparentEntities.begin(),
            transparentEntities.end(),
            [](const TransparentModelEntry &_a, const TransparentModelEntry &_b)
            {
                return _a.distanceSquared > _b.distanceSquared;
            });

        auto useShaderForMaterial = [&](MaterialAsset *_materialAsset) -> Shader*
        {
            Shader *activeShader = m_shader;
            if (_materialAsset != nullptr && _materialAsset->shaderId >= 0)
            {
                if (ShaderAsset *shaderAsset = AssetManager::Get<ShaderAsset>(_materialAsset->shaderId))
                {
                    if (!shaderAsset->GetShader()->IsLinked())
                        shaderAsset->GetShader()->Link();
                    activeShader = shaderAsset->GetShader();
                }
            }

            if (currentShader != activeShader)
            {
                if (currentShader != nullptr)
                    currentShader->UnUse();

                currentShader = activeShader;
                currentShader->Use();
                currentShader->SetMat4("P", projection);
                currentShader->SetMat4("V", view);
                currentShader->SetVec3("cameraPosition", cameraPosition);
                const Color ambientLight = scene->GetEnvironmentAmbientLight();
                currentShader->SetVec3("ambientLightColor", ambientLight.r, ambientLight.g, ambientLight.b);
                currentShader->SetFloat("ambientLightIntensity", scene->GetEnvironmentAmbientLightIntensity());

                currentShader->SetBool("useDirectionalLight", directionalLight.enabled);
                currentShader->SetVec3("directionalLightDirection", directionalLight.direction);
                currentShader->SetVec3("directionalLightColor", directionalLight.color);
                currentShader->SetFloat("directionalLightIntensity", directionalLight.intensity);
                currentShader->SetBool("useDirectionalShadow", directionalLight.enabled && m_shadowDepthTexture != 0);
                currentShader->SetMat4("directionalLightSpaceMatrix", m_shadowLightSpaceMatrix);
                currentShader->SetInt("directionalShadowMap", 4);
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);

                currentShader->SetInt("pointLightCount", static_cast<int>(pointLights.size()));
                for (size_t lightIndex = 0; lightIndex < pointLights.size(); ++lightIndex)
                {
                    const PointLightState &light = pointLights[lightIndex];
                    const std::string indexString = std::to_string(lightIndex);
                    currentShader->SetVec3("pointLightPositions[" + indexString + "]", light.position);
                    currentShader->SetVec3("pointLightColors[" + indexString + "]", light.color);
                    currentShader->SetFloat("pointLightIntensities[" + indexString + "]", light.intensity);
                    currentShader->SetFloat("pointLightRanges[" + indexString + "]", light.range);
                }
            }

            return currentShader;
        };

        std::map<StaticModelBatchKey, StaticModelBatch> staticBatchMap = {};
        std::vector<TransparentModelEntry> remainingOpaqueEntities = {};
        remainingOpaqueEntities.reserve(opaqueEntities.size());

        for (const TransparentModelEntry &entry : opaqueEntities)
        {
            Transform &transform = modelView.get<Transform>(entry.entityHandle);
            Model &modelRenderer = modelView.get<Model>(entry.entityHandle);
            Material *material = _registry.try_get<Material>(entry.entityHandle);
            MaterialAsset *materialAsset = nullptr;
            const i32 materialId = (material != nullptr) ? material->materialId : -1;
            if (materialId >= 0)
                materialAsset = AssetManager::GetMaterial(materialId);

            bool usesInstancedModelShader = true;
            if (materialAsset != nullptr && materialAsset->shaderId >= 0)
            {
                usesInstancedModelShader = false;
                if (ShaderAsset *shaderAsset = AssetManager::Get<ShaderAsset>(materialAsset->shaderId))
                    usesInstancedModelShader = shaderAsset->GetShader() == m_shader;
            }

            const bool canBatch = modelRenderer.staticModel &&
                usesInstancedModelShader &&
                _registry.try_get<ModelAnimation>(entry.entityHandle) == nullptr &&
                (material == nullptr || (material->materialIds.empty() && !HasMaterialFields(material->materialFields))) &&
                (materialAsset == nullptr || !HasMaterialFields(materialAsset->materialFields));

            if (!canBatch)
            {
                remainingOpaqueEntities.push_back(entry);
                continue;
            }

            const Color modelColor = modelRenderer.color;
            const Color materialColor = (material != nullptr) ? material->color : Color(1.0f);
            StaticModelBatchKey key = {};
            key.modelId = modelRenderer.modelId;
            key.materialId = materialId;
            key.nodeIndex = modelRenderer.nodeIndex;
            key.applyNodeTransform = modelRenderer.applyNodeTransform;
            key.modelColorR = modelColor.r;
            key.modelColorG = modelColor.g;
            key.modelColorB = modelColor.b;
            key.modelColorA = modelColor.a;
            key.materialColorR = materialColor.r;
            key.materialColorG = materialColor.g;
            key.materialColorB = materialColor.b;
            key.materialColorA = materialColor.a;

            StaticModelBatch &batch = staticBatchMap[key];
            batch.key = key;
            batch.modelMatrices.push_back(transform.GetModelMatrix());
            batch.sourceEntries.push_back(entry);
            batch.distanceSquared = std::max(batch.distanceSquared, entry.distanceSquared);
        }

        std::vector<StaticModelBatch> staticBatches = {};
        staticBatches.reserve(staticBatchMap.size());
        for (auto &batchPair : staticBatchMap)
        {
            StaticModelBatch &batch = batchPair.second;
            if (batch.modelMatrices.size() > 1u)
            {
                staticBatches.push_back(std::move(batch));
            }
            else
            {
                remainingOpaqueEntities.insert(
                    remainingOpaqueEntities.end(),
                    batch.sourceEntries.begin(),
                    batch.sourceEntries.end());
            }
        }

        std::sort(
            remainingOpaqueEntities.begin(),
            remainingOpaqueEntities.end(),
            [](const TransparentModelEntry &_a, const TransparentModelEntry &_b)
            {
                return _a.distanceSquared < _b.distanceSquared;
            });

        std::sort(
            staticBatches.begin(),
            staticBatches.end(),
            [](const StaticModelBatch &_a, const StaticModelBatch &_b)
            {
                return _a.distanceSquared < _b.distanceSquared;
            });

        auto drawEntity = [&](const entt::entity entityHandle) -> void
        {
            Transform &transform = modelView.get<Transform>(entityHandle);
            Model &modelRenderer = modelView.get<Model>(entityHandle);
            Entity *entity = modelRenderer.entity;
            if (entity == nullptr)
                entity = transform.entity;

            if (entity == nullptr || !entity->active || modelRenderer.modelId < 0)
                return;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                return;

            MaterialAsset *materialAsset = nullptr;
            Material *material = _registry.try_get<Material>(entityHandle);
            if (material != nullptr && material->materialId >= 0)
                materialAsset = AssetManager::GetMaterial(material->materialId);

            useShaderForMaterial(materialAsset);

            const ModelAsset::Pose3D *pose = nullptr;
            if (ModelAnimation *animation = _registry.try_get<ModelAnimation>(entityHandle))
            {
                if (animation->poseModelId == modelRenderer.modelId)
                    pose = &animation->pose;
            }

            Color baseColor = modelRenderer.color;
            i32 overrideTextureId = -1;
            i32 specularTextureId = -1;
            i32 roughnessTextureId = -1;
            i32 metallicTextureId = -1;
            float specularValue = 0.5f;
            float roughnessValue = 0.5f;
            float metallicValue = 0.0f;

            glDisable(GL_CULL_FACE);
            if (materialAsset != nullptr)
            {
                if ((materialAsset->info & MATERIAL_HAS_COLOR) != 0u)
                    baseColor *= materialAsset->color;

                if (materialAsset->albedoId >= 0)
                    overrideTextureId = materialAsset->albedoId;
                if (materialAsset->specularId >= 0)
                    specularTextureId = materialAsset->specularId;
                if (materialAsset->roughnessId >= 0)
                    roughnessTextureId = materialAsset->roughnessId;
                if (materialAsset->metallicId >= 0)
                    metallicTextureId = materialAsset->metallicId;

                specularValue = materialAsset->specularValue;
                roughnessValue = materialAsset->roughnessValue;
                metallicValue = materialAsset->metallicValue;

                if ((materialAsset->info & MATERIAL_BACK_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                }
                else if ((materialAsset->info & MATERIAL_FRONT_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                }

                int nextCustomTextureUnit = 5;
                nextCustomTextureUnit = materialAsset->materialFields.Use(*currentShader, nextCustomTextureUnit);

                if (material != nullptr)
                    nextCustomTextureUnit = material->materialFields.Use(*currentShader, nextCustomTextureUnit);

                (void)nextCustomTextureUnit;
            }
            else if (material != nullptr)
            {
                (void)material->materialFields.Use(*currentShader, 5);
            }

            if (material != nullptr)
                baseColor *= material->color;

            currentShader->SetFloat("TIME", static_cast<float>(Time::TimeSinceLaunch()) / 1000.0f);
            currentShader->SetFloat("specularValue", specularValue);
            currentShader->SetFloat("roughnessValue", roughnessValue);
            currentShader->SetFloat("metallicValue", metallicValue);

            currentShader->SetBool("useSpecularMap", specularTextureId >= 0);
            currentShader->SetBool("useRoughnessMap", roughnessTextureId >= 0);
            currentShader->SetBool("useMetallicMap", metallicTextureId >= 0);
            currentShader->SetInt("specularMap", 1);
            currentShader->SetInt("roughnessMap", 2);
            currentShader->SetInt("metallicMap", 3);

            glActiveTexture(GL_TEXTURE1);
            if (specularTextureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(specularTextureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glActiveTexture(GL_TEXTURE2);
            if (roughnessTextureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(roughnessTextureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glActiveTexture(GL_TEXTURE3);
            if (metallicTextureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(metallicTextureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glActiveTexture(GL_TEXTURE0);

            std::vector<MaterialAsset*> slotMaterialOverrides = {};
            if (material != nullptr && !material->materialIds.empty())
            {
                const i32 slotCount = model->GetMaterialSlotCount();
                if (slotCount > 0)
                {
                    slotMaterialOverrides.resize(static_cast<size_t>(slotCount), nullptr);
                    const size_t copyCount = std::min(slotMaterialOverrides.size(), material->materialIds.size());
                    for (size_t slotIndex = 0; slotIndex < copyCount; ++slotIndex)
                    {
                        const i32 slotMaterialId = material->materialIds[slotIndex];
                        if (slotMaterialId >= 0)
                            slotMaterialOverrides[slotIndex] = AssetManager::GetMaterial(slotMaterialId);
                    }
                }
            }

            model->Draw(
                *currentShader,
                transform.GetModelMatrix(),
                pose,
                overrideTextureId,
                baseColor,
                slotMaterialOverrides.empty() ? nullptr : &slotMaterialOverrides,
                modelRenderer.nodeIndex,
                modelRenderer.applyNodeTransform);
        };

        auto drawStaticBatch = [&](const StaticModelBatch &_batch) -> void
        {
            ModelAsset *model = AssetManager::GetModel(_batch.key.modelId);
            if (model == nullptr)
                return;

            MaterialAsset *materialAsset = nullptr;
            if (_batch.key.materialId >= 0)
                materialAsset = AssetManager::GetMaterial(_batch.key.materialId);

            useShaderForMaterial(materialAsset);

            Color baseColor = Color(
                _batch.key.modelColorR,
                _batch.key.modelColorG,
                _batch.key.modelColorB,
                _batch.key.modelColorA);
            i32 overrideTextureId = -1;
            i32 specularTextureId = -1;
            i32 roughnessTextureId = -1;
            i32 metallicTextureId = -1;
            float specularValue = 0.5f;
            float roughnessValue = 0.5f;
            float metallicValue = 0.0f;

            glDisable(GL_CULL_FACE);
            if (materialAsset != nullptr)
            {
                if ((materialAsset->info & MATERIAL_HAS_COLOR) != 0u)
                    baseColor *= materialAsset->color;

                if (materialAsset->albedoId >= 0)
                    overrideTextureId = materialAsset->albedoId;
                if (materialAsset->specularId >= 0)
                    specularTextureId = materialAsset->specularId;
                if (materialAsset->roughnessId >= 0)
                    roughnessTextureId = materialAsset->roughnessId;
                if (materialAsset->metallicId >= 0)
                    metallicTextureId = materialAsset->metallicId;

                specularValue = materialAsset->specularValue;
                roughnessValue = materialAsset->roughnessValue;
                metallicValue = materialAsset->metallicValue;

                if ((materialAsset->info & MATERIAL_BACK_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                }
                else if ((materialAsset->info & MATERIAL_FRONT_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                }
            }

            baseColor *= Color(
                _batch.key.materialColorR,
                _batch.key.materialColorG,
                _batch.key.materialColorB,
                _batch.key.materialColorA);

            currentShader->SetFloat("TIME", static_cast<float>(Time::TimeSinceLaunch()) / 1000.0f);
            currentShader->SetFloat("specularValue", specularValue);
            currentShader->SetFloat("roughnessValue", roughnessValue);
            currentShader->SetFloat("metallicValue", metallicValue);

            currentShader->SetBool("useSpecularMap", specularTextureId >= 0);
            currentShader->SetBool("useRoughnessMap", roughnessTextureId >= 0);
            currentShader->SetBool("useMetallicMap", metallicTextureId >= 0);
            currentShader->SetInt("specularMap", 1);
            currentShader->SetInt("roughnessMap", 2);
            currentShader->SetInt("metallicMap", 3);

            glActiveTexture(GL_TEXTURE1);
            if (specularTextureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(specularTextureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glActiveTexture(GL_TEXTURE2);
            if (roughnessTextureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(roughnessTextureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glActiveTexture(GL_TEXTURE3);
            if (metallicTextureId >= 0)
            {
                if (TextureAsset *texture = AssetManager::GetTexture(metallicTextureId))
                    glBindTexture(GL_TEXTURE_2D, texture->GetGLTexture().id);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glActiveTexture(GL_TEXTURE0);

            model->DrawInstanced(
                *currentShader,
                _batch.modelMatrices,
                overrideTextureId,
                baseColor,
                _batch.key.nodeIndex,
                _batch.key.applyNodeTransform);
        };

        glDepthMask(GL_TRUE);
        for (const StaticModelBatch &batch : staticBatches)
            drawStaticBatch(batch);
        for (const TransparentModelEntry &entry : remainingOpaqueEntities)
            drawEntity(entry.entityHandle);

        glDepthMask(GL_FALSE);
        for (const TransparentModelEntry &entry : transparentEntities)
            drawEntity(entry.entityHandle);
        glDepthMask(GL_TRUE);

        if (currentShader != nullptr)
        {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            currentShader->UnUse();
        }

        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
    }
} // end of Canis namespace
