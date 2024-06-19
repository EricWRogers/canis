#pragma once

#include <glm/glm.hpp>
#include <map>
#include <vector>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include "Animation.hpp"
#include "Bone.hpp"

#include <Canis/Entity.hpp>

namespace LearnOpenGL
{
	struct AnimatorComponent
	{
		glm::mat4 bones[100];
		Animation *currectAnimation;
		float currentTime = 0.0f;
		bool loop = true;
		std::function<void(Canis::Entity _entity)> OnEndOfAnimation = nullptr;
	};

	static Bone* FindBone(Animation& _animation, const std::string& name)
	{
		auto iter = std::find_if(_animation.bones.begin(), _animation.bones.end(),
			[&](Bone& bone)
			{
				return bone.GetBoneName() == name;
			}
		);
		if (iter == _animation.bones.end()) return nullptr;
		else return &(*iter);
	}

	static void CalculateBoneTransform(AnimatorComponent &_animator, const AssimpNodeData *_node, glm::mat4 _parent)
	{
		std::string nodeName = _node->name;
		glm::mat4 nodeTransform = _node->transformation;

		Bone *Bone = FindBone(*_animator.currectAnimation ,nodeName);

		if (Bone)
		{
			Bone->Update(_animator.currentTime);
			nodeTransform = Bone->GetLocalTransform();
		}

		glm::mat4 globalTransformation = _parent * nodeTransform;

		auto boneInfoMap = _animator.currectAnimation->boneInfoMap;
		if (boneInfoMap.find(nodeName) != boneInfoMap.end())
		{
			int index = boneInfoMap[nodeName].id;
			glm::mat4 offset = boneInfoMap[nodeName].offset;
			_animator.bones[index] = globalTransformation * offset;
		}

		for (int i = 0; i < _node->childrenCount; i++)
			CalculateBoneTransform(_animator, &_node->children[i], globalTransformation);
	}

	static void UpdateBones(AnimatorComponent &_animator, float _dt)
	{
		if (_animator.currectAnimation)
		{
			_animator.currentTime += _animator.currectAnimation->ticksPerSecond * _dt;
			_animator.currentTime = fmod(_animator.currentTime, _animator.currectAnimation->duration);
			CalculateBoneTransform(_animator, &_animator.currectAnimation->rootNode, glm::mat4(1.0f));
		}
	}

	static void PlayAnimation(AnimatorComponent &_animator, Animation *animation)
	{
		_animator.currectAnimation = animation;
		_animator.currentTime = 0.0f;
	}
}