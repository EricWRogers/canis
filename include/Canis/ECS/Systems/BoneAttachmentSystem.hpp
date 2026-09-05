#pragma once

#include <Canis/System.hpp>

namespace Canis
{
    class BoneAttachmentSystem : public System
    {
    public:
        BoneAttachmentSystem() { m_name = type_name<BoneAttachmentSystem>(); }
        void Update(entt::registry& _registry, float _deltaTime) override;
        bool UpdateAfterScripts() const override { return true; }
        bool UpdateInEditor() const override { return true; }
    };
}
