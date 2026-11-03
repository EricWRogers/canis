#pragma once
namespace Canis
{
    class Scene;
    // Populate components and references before calling their Create methods.
    class ComponentCreateScope
    {
    public:
        explicit ComponentCreateScope(Scene& scene, bool callCreate = true);
        ~ComponentCreateScope() noexcept(false);
        void Finish();
        ComponentCreateScope(const ComponentCreateScope&) = delete;
        ComponentCreateScope& operator=(const ComponentCreateScope&) = delete;
    private:
        Scene* m_scene;
        bool m_callCreate;
        int m_exceptions;
    };
}
