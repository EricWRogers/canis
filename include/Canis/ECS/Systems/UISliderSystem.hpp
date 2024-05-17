#pragma once

#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene;
    class UISliderSystem : public System
    {
    protected:
    public:
        UISliderSystem() : System() { m_name = type_name<UISliderSystem>(); }

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeUISliderSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace