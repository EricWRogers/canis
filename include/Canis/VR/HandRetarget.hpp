#pragma once
#include <Canis/Asset.hpp>
#include <Canis/VR/HandTracking.hpp>
#include <string_view>

namespace Canis::VR
{
    // Adapter for the generated 21-bone glove rig. XR joints point along -Z;
    // Blender bones point along local +Y. Preserve the glove's bone lengths
    // while applying each joint's full orientation (curl, spread and twist).
    inline bool RetargetGlove(const ModelAsset& model, const HandSkeleton& hand,
        std::vector<Matrix4>& localMatrices)
    {
        localMatrices.clear();
        if (!hand.active) return false;
        const auto valid = [](const Pose& p) {
            return p.valid && std::isfinite(p.position.x) && std::isfinite(p.position.y) && std::isfinite(p.position.z) &&
                std::isfinite(p.orientation.x) && std::isfinite(p.orientation.y) && std::isfinite(p.orientation.z) &&
                std::isfinite(p.orientation.w) && glm::length(p.orientation) > .001f;
        };
        for (const auto& joint : hand.joints) if (!valid(joint.pose)) return false;
        const auto wristInverse = glm::inverse(glm::normalize(hand[HandJoint::Wrist].pose.orientation));
        const auto boneToJoint = glm::angleAxis(-PI/2, Vector3(1,0,0));
        struct Mapping { std::string_view name; HandJoint joint; };
        constexpr Mapping mapping[] = {
            {"wrist", HandJoint::Wrist},
            // The glove has an extra thumb helper before its three phalanges.
            // Keep that helper in bind pose; map the three moving bones below.
            {"thumb_proximal", HandJoint::ThumbMetacarpal},
            {"thumb_intermediate", HandJoint::ThumbProximal},
            {"thumb_distal", HandJoint::ThumbDistal},
            {"index_metacarpal", HandJoint::IndexMetacarpal}, {"index_proximal", HandJoint::IndexProximal},
            {"index_intermediate", HandJoint::IndexIntermediate}, {"index_distal", HandJoint::IndexDistal},
            {"middle_metacarpal", HandJoint::MiddleMetacarpal}, {"middle_proximal", HandJoint::MiddleProximal},
            {"middle_intermediate", HandJoint::MiddleIntermediate}, {"middle_distal", HandJoint::MiddleDistal},
            {"ring_metacarpal", HandJoint::RingMetacarpal}, {"ring_proximal", HandJoint::RingProximal},
            {"ring_intermediate", HandJoint::RingIntermediate}, {"ring_distal", HandJoint::RingDistal},
            {"little_metacarpal", HandJoint::LittleMetacarpal}, {"little_proximal", HandJoint::LittleProximal},
            {"little_intermediate", HandJoint::LittleIntermediate}, {"little_distal", HandJoint::LittleDistal}
        };
        const int count = model.GetNodeCount();
        std::vector<Matrix4> globals(count, Matrix4(1));
        std::vector<unsigned char> visited(count, 0);
        localMatrices.resize(count, Matrix4(1));
        unsigned mapped = 0;
        auto visit = [&](auto&& self, int index) -> bool {
            if (index < 0 || index >= count || visited[index] == 1) return false;
            if (visited[index] == 2) return true;
            visited[index] = 1;
            const auto* node = model.GetNode(index);
            if (!node || (node->parent >= 0 && !self(self, node->parent))) return false;
            const Matrix4 parent = node->parent < 0 ? Matrix4(1) : globals[node->parent];
            auto rotation = glm::normalize(Quaternion(node->rotation.w, node->rotation.x, node->rotation.y, node->rotation.z));
            Matrix4 local = node->hasMatrix ? node->localMatrix :
                glm::translate(Matrix4(1), node->translation) * glm::mat4_cast(rotation) * glm::scale(Matrix4(1), node->scale);
            for (const auto& entry : mapping) if (entry.name == node->name)
            {
                const auto desired = wristInverse * glm::normalize(hand[entry.joint].pose.orientation) * boneToJoint;
                const auto parentRotation = glm::normalize(glm::quat_cast(glm::mat3(parent)));
                rotation = glm::normalize(glm::inverse(parentRotation) * desired);
                local = glm::translate(Matrix4(1), node->translation) * glm::mat4_cast(rotation) * glm::scale(Matrix4(1), node->scale);
                ++mapped;
                break;
            }
            localMatrices[index] = local;
            globals[index] = parent * local;
            visited[index] = 2;
            return true;
        };
        for (int i = 0; i < count; ++i) if (!visit(visit, i)) { localMatrices.clear(); return false; }
        if (mapped != std::size(mapping)) { localMatrices.clear(); return false; }
        return true;
    }
}
