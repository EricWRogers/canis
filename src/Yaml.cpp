#include <Canis/Yaml.hpp>
#include <Canis/AssetManager.hpp>

#include <algorithm>
#include <cctype>

std::string YAMLEncodeTexture(const Canis::TextureHandle &_textureHandle)
{
    return Canis::AssetManager::GetTexture(_textureHandle.id)->GetPath();
}

YAML::Node YAMLEncodeAudioAssetHandle(const Canis::AudioAssetHandle &_audioAssetHandle)
{
    YAML::Node node(YAML::NodeType::Map);

    if (_audioAssetHandle.uuid != Canis::UUID(0))
        node["uuid"] = static_cast<uint64_t>(_audioAssetHandle.uuid);

    std::string path = _audioAssetHandle.path;
    if (path.empty() && _audioAssetHandle.uuid != Canis::UUID(0))
    {
        path = Canis::AssetManager::GetPath(_audioAssetHandle.uuid);
        if (path == "Path was not found in AssetLibrary")
            path.clear();
    }

    if (!path.empty())
        node["path"] = path;

    return node;
}

YAML::Node YAMLEncodeSceneAssetHandle(const Canis::SceneAssetHandle &_sceneAssetHandle)
{
    YAML::Node node;

    Canis::UUID uuid = _sceneAssetHandle.uuid;
    if (uuid == Canis::UUID(0) && !_sceneAssetHandle.path.empty())
    {
        if (Canis::MetaFileAsset *meta = Canis::AssetManager::GetMetaFile(_sceneAssetHandle.path))
            uuid = meta->uuid;
    }

    if (uuid != Canis::UUID(0))
        node = static_cast<uint64_t>(uuid);
    else if (!_sceneAssetHandle.path.empty())
        node = _sceneAssetHandle.path;

    return node;
}

YAML::Node YAMLEncodeShaderGraphAssetHandle(const Canis::ShaderGraphAssetHandle &_shaderGraphAssetHandle)
{
    YAML::Node node(YAML::NodeType::Map);

    if (_shaderGraphAssetHandle.uuid != Canis::UUID(0))
        node["uuid"] = static_cast<uint64_t>(_shaderGraphAssetHandle.uuid);

    std::string path = _shaderGraphAssetHandle.path;
    if (path.empty() && _shaderGraphAssetHandle.uuid != Canis::UUID(0))
    {
        path = Canis::AssetManager::GetPath(_shaderGraphAssetHandle.uuid);
        if (path == "Path was not found in AssetLibrary")
            path.clear();
    }

    if (!path.empty())
        node["path"] = path;

    return node;
}

YAML::Node YAMLEncodeAnimationClipAssetHandle(const Canis::AnimationClipAssetHandle &_animationClipAssetHandle)
{
    YAML::Node node(YAML::NodeType::Map);

    if (_animationClipAssetHandle.uuid != Canis::UUID(0))
        node["uuid"] = static_cast<uint64_t>(_animationClipAssetHandle.uuid);

    std::string path = _animationClipAssetHandle.path;
    if (path.empty() && _animationClipAssetHandle.uuid != Canis::UUID(0))
    {
        path = Canis::AssetManager::GetPath(_animationClipAssetHandle.uuid);
        if (path == "Path was not found in AssetLibrary")
            path.clear();
    }

    if (!path.empty())
        node["path"] = path;

    return node;
}

YAML::Node YAMLEncodeAnimatorControllerAssetHandle(const Canis::AnimatorControllerAssetHandle &_animatorControllerAssetHandle)
{
    YAML::Node node(YAML::NodeType::Map);

    if (_animatorControllerAssetHandle.uuid != Canis::UUID(0))
        node["uuid"] = static_cast<uint64_t>(_animatorControllerAssetHandle.uuid);

    std::string path = _animatorControllerAssetHandle.path;
    if (path.empty() && _animatorControllerAssetHandle.uuid != Canis::UUID(0))
    {
        path = Canis::AssetManager::GetPath(_animatorControllerAssetHandle.uuid);
        if (path == "Path was not found in AssetLibrary")
            path.clear();
    }

    if (!path.empty())
        node["path"] = path;

    return node;
}

YAML::Node YAMLEncodeTerrainAssetHandle(const Canis::TerrainAssetHandle &_terrainAssetHandle)
{
    YAML::Node node(YAML::NodeType::Map);

    if (_terrainAssetHandle.uuid != Canis::UUID(0))
        node["uuid"] = static_cast<uint64_t>(_terrainAssetHandle.uuid);

    std::string path = _terrainAssetHandle.path;
    if (path.empty() && _terrainAssetHandle.uuid != Canis::UUID(0))
    {
        path = Canis::AssetManager::GetPath(_terrainAssetHandle.uuid);
        if (path == "Path was not found in AssetLibrary")
            path.clear();
    }

    if (!path.empty())
        node["path"] = path;

    return node;
}

Canis::TextureHandle YAMLDecodeTexture(std::string &_path)
{
    return Canis::AssetManager::GetTextureHandle(_path);
}

Canis::AudioAssetHandle YAMLDecodeAudioAssetHandle(const YAML::Node &_node)
{
    Canis::AudioAssetHandle handle = {};

    auto resolvePathFromUUID = [](const Canis::UUID _uuid) -> std::string
    {
        if (_uuid == Canis::UUID(0))
            return "";

        const std::string resolvedPath = Canis::AssetManager::GetPath(_uuid);
        return resolvedPath == "Path was not found in AssetLibrary" ? "" : resolvedPath;
    };

    auto syncUUIDFromPath = [](Canis::AudioAssetHandle &_handle) -> void
    {
        if (_handle.uuid != Canis::UUID(0) || _handle.path.empty())
            return;

        if (Canis::MetaFileAsset *meta = Canis::AssetManager::GetMetaFile(_handle.path))
            _handle.uuid = meta->uuid;
    };

    if (!_node)
        return handle;

    if (_node.IsMap())
    {
        handle.uuid = _node["uuid"].as<uint64_t>(0);
        handle.path = resolvePathFromUUID(handle.uuid);

        if (handle.path.empty())
            handle.path = _node["path"].as<std::string>("");

        syncUUIDFromPath(handle);
        return handle;
    }

    if (_node.IsScalar())
    {
        const std::string raw = _node.as<std::string>("");
        const bool isNumeric = !raw.empty() &&
            std::all_of(raw.begin(), raw.end(), [](unsigned char _c) { return std::isdigit(_c) != 0; });

        if (isNumeric)
        {
            handle.uuid = static_cast<Canis::UUID>(std::stoull(raw));
            handle.path = resolvePathFromUUID(handle.uuid);
        }
        else
        {
            handle.path = raw;
            syncUUIDFromPath(handle);
        }
    }

    return handle;
}

Canis::SceneAssetHandle YAMLDecodeSceneAssetHandle(const YAML::Node &_node)
{
    Canis::SceneAssetHandle handle = {};

    auto resolvePathFromUUID = [](const Canis::UUID _uuid) -> std::string
    {
        if (_uuid == Canis::UUID(0))
            return "";

        const std::string resolvedPath = Canis::AssetManager::GetPath(_uuid);
        return resolvedPath == "Path was not found in AssetLibrary" ? "" : resolvedPath;
    };

    auto syncUUIDFromPath = [](Canis::SceneAssetHandle &_handle) -> void
    {
        if (_handle.uuid != Canis::UUID(0) || _handle.path.empty())
            return;

        if (Canis::MetaFileAsset *meta = Canis::AssetManager::GetMetaFile(_handle.path))
            _handle.uuid = meta->uuid;
    };

    if (!_node)
        return handle;

    if (_node.IsMap())
    {
        handle.uuid = _node["uuid"].as<uint64_t>(0);
        handle.path = resolvePathFromUUID(handle.uuid);

        if (handle.path.empty())
            handle.path = _node["path"].as<std::string>("");

        syncUUIDFromPath(handle);
        return handle;
    }

    if (_node.IsScalar())
    {
        const std::string raw = _node.as<std::string>("");
        const bool isNumeric = !raw.empty() &&
            std::all_of(raw.begin(), raw.end(), [](unsigned char _c) { return std::isdigit(_c) != 0; });

        if (isNumeric)
        {
            handle.uuid = static_cast<Canis::UUID>(std::stoull(raw));
            handle.path = resolvePathFromUUID(handle.uuid);
        }
        else
        {
            handle.path = raw;
            syncUUIDFromPath(handle);
        }
    }

    return handle;
}

Canis::ShaderGraphAssetHandle YAMLDecodeShaderGraphAssetHandle(const YAML::Node &_node)
{
    Canis::ShaderGraphAssetHandle handle = {};

    auto resolvePathFromUUID = [](const Canis::UUID _uuid) -> std::string
    {
        if (_uuid == Canis::UUID(0))
            return "";

        const std::string resolvedPath = Canis::AssetManager::GetPath(_uuid);
        return resolvedPath == "Path was not found in AssetLibrary" ? "" : resolvedPath;
    };

    auto syncUUIDFromPath = [](Canis::ShaderGraphAssetHandle &_handle) -> void
    {
        if (_handle.uuid != Canis::UUID(0) || _handle.path.empty())
            return;

        if (Canis::MetaFileAsset *meta = Canis::AssetManager::GetMetaFile(_handle.path))
            _handle.uuid = meta->uuid;
    };

    if (!_node)
        return handle;

    if (_node.IsMap())
    {
        handle.uuid = _node["uuid"].as<uint64_t>(0);
        handle.path = resolvePathFromUUID(handle.uuid);

        if (handle.path.empty())
            handle.path = _node["path"].as<std::string>("");

        syncUUIDFromPath(handle);
        return handle;
    }

    if (_node.IsScalar())
    {
        const std::string raw = _node.as<std::string>("");
        const bool isNumeric = !raw.empty() &&
            std::all_of(raw.begin(), raw.end(), [](unsigned char _c) { return std::isdigit(_c) != 0; });

        if (isNumeric)
        {
            handle.uuid = static_cast<Canis::UUID>(std::stoull(raw));
            handle.path = resolvePathFromUUID(handle.uuid);
        }
        else
        {
            handle.path = raw;
            syncUUIDFromPath(handle);
        }
    }

    return handle;
}

Canis::AnimationClipAssetHandle YAMLDecodeAnimationClipAssetHandle(const YAML::Node &_node)
{
    Canis::AnimationClipAssetHandle handle = {};

    auto resolvePathFromUUID = [](const Canis::UUID _uuid) -> std::string
    {
        if (_uuid == Canis::UUID(0))
            return "";

        const std::string resolvedPath = Canis::AssetManager::GetPath(_uuid);
        return resolvedPath == "Path was not found in AssetLibrary" ? "" : resolvedPath;
    };

    auto syncUUIDFromPath = [](Canis::AnimationClipAssetHandle &_handle) -> void
    {
        if (_handle.uuid != Canis::UUID(0) || _handle.path.empty())
            return;

        if (Canis::MetaFileAsset *meta = Canis::AssetManager::GetMetaFile(_handle.path))
            _handle.uuid = meta->uuid;
    };

    if (!_node)
        return handle;

    if (_node.IsMap())
    {
        handle.uuid = _node["uuid"].as<uint64_t>(0);
        handle.path = resolvePathFromUUID(handle.uuid);

        if (handle.path.empty())
            handle.path = _node["path"].as<std::string>("");

        syncUUIDFromPath(handle);
        return handle;
    }

    if (_node.IsScalar())
    {
        const std::string raw = _node.as<std::string>("");
        const bool isNumeric = !raw.empty() &&
            std::all_of(raw.begin(), raw.end(), [](unsigned char _c) { return std::isdigit(_c) != 0; });

        if (isNumeric)
        {
            handle.uuid = static_cast<Canis::UUID>(std::stoull(raw));
            handle.path = resolvePathFromUUID(handle.uuid);
        }
        else
        {
            handle.path = raw;
            syncUUIDFromPath(handle);
        }
    }

    return handle;
}

Canis::AnimatorControllerAssetHandle YAMLDecodeAnimatorControllerAssetHandle(const YAML::Node &_node)
{
    Canis::AnimatorControllerAssetHandle handle = {};

    auto resolvePathFromUUID = [](const Canis::UUID _uuid) -> std::string
    {
        if (_uuid == Canis::UUID(0))
            return "";

        const std::string resolvedPath = Canis::AssetManager::GetPath(_uuid);
        return resolvedPath == "Path was not found in AssetLibrary" ? "" : resolvedPath;
    };

    auto syncUUIDFromPath = [](Canis::AnimatorControllerAssetHandle &_handle) -> void
    {
        if (_handle.uuid != Canis::UUID(0) || _handle.path.empty())
            return;

        if (Canis::MetaFileAsset *meta = Canis::AssetManager::GetMetaFile(_handle.path))
            _handle.uuid = meta->uuid;
    };

    if (!_node)
        return handle;

    if (_node.IsMap())
    {
        handle.uuid = _node["uuid"].as<uint64_t>(0);
        handle.path = resolvePathFromUUID(handle.uuid);

        if (handle.path.empty())
            handle.path = _node["path"].as<std::string>("");

        syncUUIDFromPath(handle);
        return handle;
    }

    if (_node.IsScalar())
    {
        const std::string raw = _node.as<std::string>("");
        const bool isNumeric = !raw.empty() &&
            std::all_of(raw.begin(), raw.end(), [](unsigned char _c) { return std::isdigit(_c) != 0; });

        if (isNumeric)
        {
            handle.uuid = static_cast<Canis::UUID>(std::stoull(raw));
            handle.path = resolvePathFromUUID(handle.uuid);
        }
        else
        {
            handle.path = raw;
            syncUUIDFromPath(handle);
        }
    }

    return handle;
}

Canis::TerrainAssetHandle YAMLDecodeTerrainAssetHandle(const YAML::Node &_node)
{
    Canis::TerrainAssetHandle handle = {};

    auto resolvePathFromUUID = [](const Canis::UUID _uuid) -> std::string
    {
        if (_uuid == Canis::UUID(0))
            return "";

        const std::string resolvedPath = Canis::AssetManager::GetPath(_uuid);
        return resolvedPath == "Path was not found in AssetLibrary" ? "" : resolvedPath;
    };

    auto syncUUIDFromPath = [](Canis::TerrainAssetHandle &_handle) -> void
    {
        if (_handle.uuid != Canis::UUID(0) || _handle.path.empty())
            return;

        if (Canis::MetaFileAsset *meta = Canis::AssetManager::GetMetaFile(_handle.path))
            _handle.uuid = meta->uuid;
    };

    if (!_node)
        return handle;

    if (_node.IsMap())
    {
        handle.uuid = _node["uuid"].as<uint64_t>(0);
        handle.path = resolvePathFromUUID(handle.uuid);

        if (handle.path.empty())
            handle.path = _node["path"].as<std::string>("");

        syncUUIDFromPath(handle);
        return handle;
    }

    if (_node.IsScalar())
    {
        const std::string raw = _node.as<std::string>("");
        const bool isNumeric = !raw.empty() &&
            std::all_of(raw.begin(), raw.end(), [](unsigned char _c) { return std::isdigit(_c) != 0; });

        if (isNumeric)
        {
            handle.uuid = static_cast<Canis::UUID>(std::stoull(raw));
            handle.path = resolvePathFromUUID(handle.uuid);
        }
        else
        {
            handle.path = raw;
            syncUUIDFromPath(handle);
        }
    }

    return handle;
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
