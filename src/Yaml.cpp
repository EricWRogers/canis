#include <Canis/Yaml.hpp>
#include <Canis/AssetManager.hpp>

std::string YAMLEncodeTexture(const Canis::TextureHandle &_textureHandle)
{
    return Canis::AssetManager::GetTexture(_textureHandle.id)->GetPath();
}

std::string YAMLEncodeSceneAssetHandle(const Canis::SceneAssetHandle &_sceneAssetHandle)
{
    return _sceneAssetHandle.path;
}

Canis::TextureHandle YAMLDecodeTexture(std::string &_path)
{
    return Canis::AssetManager::GetTextureHandle(_path);
}

Canis::SceneAssetHandle YAMLDecodeSceneAssetHandle(const std::string &_path)
{
    return Canis::SceneAssetHandle{ .path = _path };
}

namespace YAML
{
    Emitter &operator<<(Emitter &_out, const Canis::Vector2 &_vector)
    {
        _out << Flow;
        _out << BeginSeq << _vector.x << _vector.y << EndSeq;
        return _out;
    }

    Emitter &operator<<(Emitter &_out, const Canis::Vector3 &_vector)
    {
        _out << Flow;
        _out << BeginSeq << _vector.x << _vector.y << _vector.z << EndSeq;
        return _out;
    }

    Emitter &operator<<(Emitter &_out, const Canis::Vector4 &_vector)
    {
        _out << Flow;
        _out << BeginSeq << _vector.x << _vector.y << _vector.z << _vector.w << EndSeq;
        return _out;
    }
}
