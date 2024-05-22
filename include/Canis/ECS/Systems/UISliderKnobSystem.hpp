#pragma once
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene;

    struct KnobListener {
        std::string name = "";
        void* data = nullptr;
        std::function<void(Entity _entity, float _value, void* _data)> func = nullptr;
    };

    class UISliderKnobSystem : public System
    {
    private:
        std::vector<KnobListener> m_knobListeners = {};
    public:
        UISliderKnobSystem() : System() { m_name = type_name<UISliderKnobSystem>(); }

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);

        KnobListener& AddKnobListener(
            std::string _name,
            void* _data,
            std::function<void(Entity _entity, float _value, void* _data)> _func
        );

        void RemoveKnobListener(KnobListener& _listener);
    };

    extern bool DecodeUISliderKnobSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace