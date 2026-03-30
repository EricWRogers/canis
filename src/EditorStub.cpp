#include <Canis/Editor.hpp>

namespace Canis
{
    Editor::~Editor() = default;

    void Editor::Init(Window* _window)
    {
        (void)_window;
        m_mode = EditorMode::HIDDEN;
    }

    void Editor::Draw(Scene* _scene, Window* _window, App* _app, GameCodeObject* _gameSharedLib, float _deltaTime)
    {
        (void)_scene;
        (void)_window;
        (void)_app;
        (void)_gameSharedLib;
        (void)_deltaTime;
    }

    void Editor::BeginGameRender(Window* _window)
    {
        (void)_window;
    }

    void Editor::BeginPlayRender(Window* _window)
    {
        (void)_window;
    }

    void Editor::EndGameRender(Window* _window)
    {
        (void)_window;
    }

    void Editor::RenderGameDebug()
    {
    }

    void Editor::StopPlayMode()
    {
    }

    void Editor::FocusEntity(Canis::Entity* _entity)
    {
        (void)_entity;
    }

    void Editor::InputEntity(const std::string& _name, Canis::Entity*& _variable)
    {
        (void)_name;
        (void)_variable;
    }

    void Editor::InputEntity(const std::string& _name, const char* _idSuffix, Canis::Entity*& _variable)
    {
        (void)_name;
        (void)_idSuffix;
        (void)_variable;
    }

    void Editor::InputAnimationClip(const std::string& _name, Canis::AnimationClip2DID& _variable)
    {
        (void)_name;
        (void)_variable;
    }

    void Editor::InputAnimationClip(const std::string& _name, const char* _idSuffix, Canis::AnimationClip2DID& _variable)
    {
        (void)_name;
        (void)_idSuffix;
        (void)_variable;
    }

    void Editor::InputSceneAsset(const std::string& _name, Canis::SceneAssetHandle& _variable)
    {
        (void)_name;
        (void)_variable;
    }

    void Editor::InputSceneAsset(const std::string& _name, const char* _idSuffix, Canis::SceneAssetHandle& _variable)
    {
        (void)_name;
        (void)_idSuffix;
        (void)_variable;
    }
}
