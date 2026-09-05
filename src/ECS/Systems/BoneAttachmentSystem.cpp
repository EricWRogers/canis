#include <Canis/ECS/Systems/BoneAttachmentSystem.hpp>

#include <algorithm>
#include <cmath>

#include <Canis/AssetManager.hpp>
#include <Canis/Entity.hpp>

namespace Canis
{
    void BoneAttachmentSystem::Update(entt::registry& _registry, float)
    {
        auto view = _registry.view<BoneAttachment, Transform>();
        for (const entt::entity handle : view)
        {
            BoneAttachment& attachment = view.get<BoneAttachment>(handle);
            Transform& transform = view.get<Transform>(handle);
            Entity* target = attachment.target;
            if (target == nullptr || !target->active ||
                !target->HasComponents<Transform, Model, ModelAnimation>())
            {
                transform.ClearLocalMatrixPrefix();
                continue;
            }

            const Model& modelComponent = target->GetComponent<Model>();
            ModelAsset* model = AssetManager::GetModel(modelComponent.modelId);
            if (model == nullptr)
            {
                transform.ClearLocalMatrixPrefix();
                continue;
            }
            if (attachment.cachedModelId != modelComponent.modelId ||
                attachment.cachedBoneName != attachment.boneName)
            {
                attachment.cachedModelId = modelComponent.modelId;
                attachment.cachedBoneName = attachment.boneName;
                attachment.cachedNodeIndex = -1;
                for (i32 index = 0; index < model->GetNodeCount(); ++index)
                    if (model->GetNodeName(index) == attachment.boneName)
                    { attachment.cachedNodeIndex = index; break; }
            }

            const ModelAnimation& animation = target->GetComponent<ModelAnimation>();
            const i32 index = attachment.cachedNodeIndex;
            if (index < 0 || index >= static_cast<i32>(animation.pose.globalNodeMatrices.size()))
            {
                transform.ClearLocalMatrixPrefix();
                continue;
            }
            if (attachment.parentToTarget && transform.parent != target)
                transform.SetParent(target);

            const Matrix4& bone = animation.pose.globalNodeMatrices[index];
            Vector3 x(bone[0]), y(bone[1]), z(bone[2]);
            x /= std::max(glm::length(x), 0.000001f);
            y /= std::max(glm::length(y), 0.000001f);
            z /= std::max(glm::length(z), 0.000001f);
            glm::mat3 rotationMatrix(1.0f);
            rotationMatrix[0] = x; rotationMatrix[1] = y; rotationMatrix[2] = z;

            Matrix4 socket(1.0f);
            if (attachment.followPosition)
                socket = glm::translate(socket, Vector3(bone[3]));
            if (attachment.followRotation)
                socket *= glm::mat4_cast(glm::normalize(glm::quat_cast(rotationMatrix)));
            if (!attachment.parentToTarget)
            {
                socket = target->GetComponent<Transform>().GetModelMatrix() * socket;
                if (transform.parent != nullptr && transform.parent->HasComponent<Transform>())
                    socket = glm::inverse(transform.parent->GetComponent<Transform>().GetModelMatrix()) * socket;
            }
            transform.SetLocalMatrixPrefix(socket);
        }
    }
}
