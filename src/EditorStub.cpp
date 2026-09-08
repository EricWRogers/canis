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

    void Editor::RebuildPrefabInstance(Canis::Entity* _entity)
    {
        (void)_entity;
    }

    void Editor::RebuildAllPrefabInstances()
    {
    }

    void Editor::ApplyPrefabInstanceOverrides(Canis::Entity* _entity)
    {
        (void)_entity;
    }

    void Editor::RebuildTerrainEntity(Canis::Entity& _entity)
    {
        (void)_entity;
    }

    bool Editor::BakeBlockoutEntity(Canis::Entity* _entity)
    {
        (void)_entity;
        return false;
    }

    void Editor::NotifyAnimationPropertyEdited(
        Canis::Entity& _entity,
        const std::string& _componentName,
        const std::string& _propertyName,
        AnimationValueType _type,
        AnimationInterpolation _interpolation,
        const AnimationValue& _value)
    {
        (void)_entity;
        (void)_componentName;
        (void)_propertyName;
        (void)_type;
        (void)_interpolation;
        (void)_value;
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

    void Editor::InputAnimationClipAsset(const std::string& _name, Canis::AnimationClipAssetHandle& _variable)
    {
        (void)_name;
        (void)_variable;
    }

    void Editor::InputAnimationClipAsset(const std::string& _name, const char* _idSuffix, Canis::AnimationClipAssetHandle& _variable)
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

    void Editor::InputAudioAsset(const std::string& _name, Canis::AudioAssetHandle& _variable)
    {
        InputAudioAsset(_name, nullptr, _variable);
    }

    void Editor::InputAudioAsset(const std::string& _name, const char* _idSuffix, Canis::AudioAssetHandle& _variable)
    {
        (void)_name;
        (void)_idSuffix;
        (void)_variable;
    }

    void Editor::InputAnimatorControllerAsset(const std::string& _name, Canis::AnimatorControllerAssetHandle& _variable)
    {
        InputAnimatorControllerAsset(_name, nullptr, _variable);
    }

    void Editor::InputAnimatorControllerAsset(const std::string& _name, const char* _idSuffix, Canis::AnimatorControllerAssetHandle& _variable)
    {
        (void)_name;
        (void)_idSuffix;
        (void)_variable;
    }

    std::string Editor::ResolveRememberedShaderGraphPath() const
    {
        return {};
    }

    void Editor::RememberLastShaderGraphAssetPath(const std::string& _path)
    {
        (void)_path;
    }

    bool Editor::InputTerrainAsset(const std::string& _name, Canis::TerrainAssetHandle& _variable)
    {
        (void)_name;
        (void)_variable;
        return false;
    }

    bool Editor::InputTerrainAsset(const std::string& _name, const char* _idSuffix, Canis::TerrainAssetHandle& _variable)
    {
        (void)_name;
        (void)_idSuffix;
        (void)_variable;
        return false;
    }
}
