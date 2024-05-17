#pragma once
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene;
    class UISliderKnobSystem : public System
    {
    private:
    public:
        UISliderKnobSystem() : System() { m_name = type_name<UISliderKnobSystem>(); }

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeUISliderKnobSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace