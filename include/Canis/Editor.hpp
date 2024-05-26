#pragma once
#include <Canis/Window.hpp>
#include <Canis/Scene.hpp>

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
        void Draw(Scene* _scene);

        EditorMode GetMode() { return m_mode; }
    private:
        void DrawInspectorPanel();
        void DrawSystemPanel();
        void DrawHierarchyPanel();
        void DrawScenePanel();

        SceneManager& GetSceneManager();

        Scene *m_scene;
        int m_index = 0;
        EditorMode m_mode = EditorMode::EDIT;
    };
}