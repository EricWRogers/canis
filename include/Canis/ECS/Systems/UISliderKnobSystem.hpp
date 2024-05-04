#pragma once
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene;
    class UISliderKnobSystem : public System
    {
    private:
    public:
        UISliderKnobSystem() : System() {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeUISliderKnobSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace