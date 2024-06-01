#pragma once
#include <Canis/Window.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Time.hpp>

namespace Canis
{
    class SceneManager;

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
        EditorMode m_mode = EditorMode::EDIT;
    };
}