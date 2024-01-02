#pragma once
#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/Data/Bit.hpp>

namespace Canis
{
struct TextComponent
{
	int assetId = 0;
	std::string text = {}; // FIX : memory leak
	unsigned int alignment = 0; // 0 is left align | 1 is right align | 2 is center align
	unsigned int _status = BIT::ONE; // this will make the RenderTextSystem recalculate rect size &| alignment
};

// _info
// bit one if 1 alignment should be recalculated

namespace Text {
	
	const unsigned int LEFT   = 0u;
	const unsigned int RIGHT  = 1u;
	const unsigned int CENTER = 2u;

	inline void Set(TextComponent &_textComponent, RectTransformComponent &_rectComponent, std::string _text) {
		_textComponent.text = _text;
		_rectComponent.originOffset = glm::vec2(0.0f);
		_textComponent._status = _textComponent._status | BIT::ONE; // the alignment should be recalculated
	}
}

inline void RemoveTrialingZeros(std::string &_word)
{
	while(_word.size() > 0)
	{
		if (_word[_word.size() - 1] == '0')
			_word.pop_back();
		else
			return;
	}
}
} // end of Canis namespace