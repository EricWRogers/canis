#pragma once
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <Canis/UUID.hpp>
#include <Canis/Math.hpp>
#include <Canis/AssetHandle.hpp>
#include <Canis/External/GetNameOfType.hpp>

#include <map>
#include <variant>
#include <vector>
#include <type_traits>
#include <functional>
#include <string>

extern std::string YAMLEncodeTexture(const Canis::TextureHandle &_textureHandle);
extern Canis::TextureHandle YAMLDecodeTexture(std::string &_path);
extern YAML::Node YAMLEncodeAudioAssetHandle(const Canis::AudioAssetHandle &_audioAssetHandle);
extern Canis::AudioAssetHandle YAMLDecodeAudioAssetHandle(const YAML::Node &_node);
extern YAML::Node YAMLEncodeSceneAssetHandle(const Canis::SceneAssetHandle &_sceneAssetHandle);
extern Canis::SceneAssetHandle YAMLDecodeSceneAssetHandle(const YAML::Node &_node);
extern YAML::Node YAMLEncodeShaderGraphAssetHandle(const Canis::ShaderGraphAssetHandle &_shaderGraphAssetHandle);
extern Canis::ShaderGraphAssetHandle YAMLDecodeShaderGraphAssetHandle(const YAML::Node &_node);
extern YAML::Node YAMLEncodeAnimationClipAssetHandle(const Canis::AnimationClipAssetHandle &_animationClipAssetHandle);
extern Canis::AnimationClipAssetHandle YAMLDecodeAnimationClipAssetHandle(const YAML::Node &_node);
extern YAML::Node YAMLEncodeAnimatorControllerAssetHandle(const Canis::AnimatorControllerAssetHandle &_animatorControllerAssetHandle);
extern Canis::AnimatorControllerAssetHandle YAMLDecodeAnimatorControllerAssetHandle(const YAML::Node &_node);

namespace YAML
{
    Emitter &operator<<(Emitter &_out, const Canis::Vector2 &_vector);

	Emitter &operator<<(Emitter &_out, const Canis::Vector3 &_vector);

	Emitter &operator<<(Emitter &_out, const Canis::Vector4 &_vector);

    template <>
    struct convert<Canis::Vector2>
    {
        static Node encode(const Canis::Vector2 &rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node &node, Canis::Vector2 &rhs)
        {
            if (!node.IsSequence() || node.size() != 2)
                return false;

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            return true;
        }
    };

    template <>
    struct convert<Canis::Vector3>
    {
        static Node encode(const Canis::Vector3 &rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node &node, Canis::Vector3 &rhs)
        {
            if (!node.IsSequence() || node.size() != 3)
                return false;

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            return true;
        }
    };

    template <>
    struct convert<Canis::Vector4>
    {
        static Node encode(const Canis::Vector4 &rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node &node, Canis::Vector4 &rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
                return false;

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();
            return true;
        }
    };

    template <>
    struct convert<Canis::AudioAssetHandle>
    {
        static Node encode(const Canis::AudioAssetHandle &_audioAssetHandle)
        {
            return YAMLEncodeAudioAssetHandle(_audioAssetHandle);
        }

        static bool decode(const Node &_node, Canis::AudioAssetHandle &_audioAssetHandle)
        {
            _audioAssetHandle = YAMLDecodeAudioAssetHandle(_node);
            return true;
        }
    };

    template <>
    struct convert<Canis::AnimatorControllerAssetHandle>
    {
        static Node encode(const Canis::AnimatorControllerAssetHandle &_animatorControllerAssetHandle)
        {
            return YAMLEncodeAnimatorControllerAssetHandle(_animatorControllerAssetHandle);
        }

        static bool decode(const Node &_node, Canis::AnimatorControllerAssetHandle &_animatorControllerAssetHandle)
        {
            _animatorControllerAssetHandle = YAMLDecodeAnimatorControllerAssetHandle(_node);
            return true;
        }
    };

    template <>
    struct convert<Canis::SceneAssetHandle>
    {
        static Node encode(const Canis::SceneAssetHandle &_sceneAssetHandle)
        {
            return YAMLEncodeSceneAssetHandle(_sceneAssetHandle);
        }

        static bool decode(const Node &_node, Canis::SceneAssetHandle &_sceneAssetHandle)
        {
            _sceneAssetHandle = YAMLDecodeSceneAssetHandle(_node);
            return true;
        }
    };

    template <>
    struct convert<Canis::ShaderGraphAssetHandle>
    {
        static Node encode(const Canis::ShaderGraphAssetHandle &_shaderGraphAssetHandle)
        {
            return YAMLEncodeShaderGraphAssetHandle(_shaderGraphAssetHandle);
        }

        static bool decode(const Node &_node, Canis::ShaderGraphAssetHandle &_shaderGraphAssetHandle)
        {
            _shaderGraphAssetHandle = YAMLDecodeShaderGraphAssetHandle(_node);
            return true;
        }
    };

    template <>
    struct convert<Canis::AnimationClipAssetHandle>
    {
        static Node encode(const Canis::AnimationClipAssetHandle &_animationClipAssetHandle)
        {
            return YAMLEncodeAnimationClipAssetHandle(_animationClipAssetHandle);
        }

        static bool decode(const Node &_node, Canis::AnimationClipAssetHandle &_animationClipAssetHandle)
        {
            _animationClipAssetHandle = YAMLDecodeAnimationClipAssetHandle(_node);
            return true;
        }
    };

    template <>
	struct convert<Canis::TextureHandle>
	{
		static Node encode(const Canis::TextureHandle &_textureHandle)
		{
			Node node;
			node = YAMLEncodeTexture(_textureHandle);
			return node;
		}

		static bool decode(const Node &_node, Canis::TextureHandle &_textureHandle)
		{
			std::string path = _node.as<std::string>("");
			_textureHandle = YAMLDecodeTexture(path);
			return true;
		}
	};

    template <>
    struct convert<Canis::UUID>
    {
        static Node encode(const Canis::UUID &_uuid)
        {
            Node node;
            node = std::to_string(_uuid);
            return node;
        }
        static bool decode(const Node &_node, Canis::UUID &_uuid)
        {
            _uuid = (Canis::UUID)_node.as<uint64_t>(0);
            return true;
        }
    };

    template <>
    struct convert<Canis::Mask>
    {
        static Node encode(const Canis::Mask &_mask)
        {
            Node node;
            node = static_cast<u32>(_mask);
            return node;
        }

        static bool decode(const Node &_node, Canis::Mask &_mask)
        {
            _mask = _node.as<u32>(0u);
            return true;
        }
    };
}
