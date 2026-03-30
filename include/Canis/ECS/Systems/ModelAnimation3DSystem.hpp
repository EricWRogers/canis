#pragma once
#include <Canis/System.hpp>

namespace Canis
{
    class ModelAnimation3DSystem : public System
    {
    public:
        ModelAnimation3DSystem() : System() { m_name = type_name<ModelAnimation3DSystem>(); }

        void Create() override {}
        void Ready() override;
        void Update(entt::registry &_registry, float _deltaTime) override;
    };
} // end of Canis namespace
