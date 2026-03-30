#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>

#include <Canis/AssetManager.hpp>
#include <Canis/Entity.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Shader.hpp>
#include <Canis/Window.hpp>
#include <algorithm>

namespace Canis
{
    namespace
    {
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
        CreateSkyboxGeometry();
    }

    void MeshRenderer3DSystem::Ready() {}

    void MeshRenderer3DSystem::OnDestroy()
    {
        if (m_skyboxVbo != 0)
            glDeleteBuffers(1, &m_skyboxVbo);
        if (m_skyboxVao != 0)
            glDeleteVertexArrays(1, &m_skyboxVao);

        m_skyboxVbo = 0;
        m_skyboxVao = 0;
        m_skyboxShader = nullptr;
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

    void MeshRenderer3DSystem::DrawSkybox(const Matrix4 &_projection, const Matrix4 &_view)
    {
        if (m_skyboxShader == nullptr || m_skyboxVao == 0)
            return;

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

        Matrix4 projection = Matrix4(1.0f);
        Matrix4 view = Matrix4(1.0f);
        Vector3 cameraPosition = Vector3(0.0f, 0.0f, 0.0f);

        if (scene->HasEditorCamera3DOverride())
        {
            projection = scene->GetEditorCamera3DProjection();
            view = scene->GetEditorCamera3DView();
            const Matrix4 invView = glm::inverse(view);
            cameraPosition = Vector3(invView[3][0], invView[3][1], invView[3][2]);
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
                return;

            const float aspect = (window->GetScreenHeight() > 0)
                ? (static_cast<float>(window->GetScreenWidth()) / static_cast<float>(window->GetScreenHeight()))
                : 1.0f;
            projection = glm::perspective(DEG2RAD * camera->fovDegrees, aspect, camera->nearClip, camera->farClip);

            const Vector3 eye = cameraTransform->GetGlobalPosition();
            const Vector3 target = eye + cameraTransform->GetForward();
            const Vector3 up = cameraTransform->GetUp();
            view = glm::lookAt(eye, target, up);
            cameraPosition = eye;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        DrawSkybox(projection, view);

        bool useDirectionalLight = true;
        Vector3 directionalLightDirection = Vector3(-0.4f, -1.0f, -0.25f);
        Vector3 directionalLightColor = Vector3(1.0f, 0.98f, 0.95f);
        float directionalLightIntensity = 1.0f;

        auto directionalLightView = _registry.view<DirectionalLight>();
        for (const entt::entity entityHandle : directionalLightView)
        {
            DirectionalLight &light = directionalLightView.get<DirectionalLight>(entityHandle);
            Entity *entity = light.entity;
            if (entity == nullptr || !entity->active)
                continue;

            useDirectionalLight = light.enabled;
            directionalLightDirection = light.direction;
            const float directionLength = glm::length(directionalLightDirection);
            if (directionLength > 0.0001f)
                directionalLightDirection /= directionLength;
            else
                directionalLightDirection = Vector3(0.0f, -1.0f, 0.0f);

            directionalLightColor = Vector3(light.color.r, light.color.g, light.color.b);
            directionalLightIntensity = light.intensity;
            break;
        }

        bool usePointLight = false;
        Vector3 pointLightPosition = Vector3(2.0f, 2.5f, 2.0f);
        Vector3 pointLightColor = Vector3(1.0f, 0.95f, 0.85f);
        float pointLightIntensity = 1.2f;
        float pointLightRange = 12.0f;

        auto pointLightView = _registry.view<PointLight, Transform>();
        for (const entt::entity entityHandle : pointLightView)
        {
            PointLight &light = pointLightView.get<PointLight>(entityHandle);
            Transform &lightTransform = pointLightView.get<Transform>(entityHandle);
            Entity *entity = light.entity;
            if (entity == nullptr)
                entity = lightTransform.entity;

            if (entity == nullptr || !entity->active)
                continue;

            usePointLight = light.enabled;
            pointLightPosition = lightTransform.GetGlobalPosition();
            pointLightColor = Vector3(light.color.r, light.color.g, light.color.b);
            pointLightIntensity = light.intensity;
            pointLightRange = light.range;
            break;
        }

        Shader *currentShader = nullptr;

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

            MaterialAsset *materialAsset = nullptr;
            Material *material = _registry.try_get<Material>(entityHandle);
            if (material != nullptr && material->materialId >= 0)
                materialAsset = AssetManager::GetMaterial(material->materialId);

            Shader *activeShader = m_shader;
            if (materialAsset != nullptr && materialAsset->shaderId >= 0)
            {
                if (ShaderAsset *shaderAsset = AssetManager::Get<ShaderAsset>(materialAsset->shaderId))
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

                currentShader->SetBool("useDirectionalLight", useDirectionalLight);
                currentShader->SetVec3("directionalLightDirection", directionalLightDirection);
                currentShader->SetVec3("directionalLightColor", directionalLightColor);
                currentShader->SetFloat("directionalLightIntensity", directionalLightIntensity);

                currentShader->SetBool("usePointLight", usePointLight);
                currentShader->SetVec3("pointLightPosition", pointLightPosition);
                currentShader->SetVec3("pointLightColor", pointLightColor);
                currentShader->SetFloat("pointLightIntensity", pointLightIntensity);
                currentShader->SetFloat("pointLightRange", pointLightRange);
            }

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

                materialAsset->materialFields.Use(*currentShader);
            }

            if (material != nullptr)
                baseColor *= material->color;

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
                slotMaterialOverrides.empty() ? nullptr : &slotMaterialOverrides);
        }

        if (currentShader != nullptr)
            currentShader->UnUse();

        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
    }
} // end of Canis namespace
