#pragma once
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene;
    class UISliderKnobSystem;

    struct KnobListener {
        std::string name = "";
        void* data = nullptr;
        std::function<void(Entity _entity, float _value, void* _data)> func = nullptr;
        
        int _id = 0;
        UISliderKnobSystem* _system;

        ~KnobListener();        
    };

    class UISliderKnobSystem : public System
    {
    private:
        std::vector<KnobListener*> m_knobListeners = {};
        int nextId = 0;
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

        void RemoveKnobListener(int _id);
    };

    extern bool DecodeUISliderKnobSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace