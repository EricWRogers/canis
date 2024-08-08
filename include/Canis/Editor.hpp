#pragma once
#include <Canis/Scene.hpp>

namespace Canis
{
    class SceneManager;
    class Time;
    class Window;

    enum EditorMode
    {
        EDIT,
        PLAY,
        HIDDEN
    };

    class Editor
    {
    friend class Scene;
    public:
        Editor() = default;
        ~Editor() = default;
        void Init(Window* _window);
        void Draw(Scene* _scene, Window* _window, Time *_time);

        EditorMode GetMode() { return m_mode; }
    private:
        void DrawInspectorPanel();
        void DrawSystemPanel();
        void DrawHierarchyPanel();
        void DrawScenePanel(Window* _window, Time *_time);

        SceneManager& GetSceneManager();

        Scene *m_scene;
        int m_index = 0;
        bool m_forceRefresh = false;
        EditorMode m_mode = EditorMode::EDIT;
    };
}