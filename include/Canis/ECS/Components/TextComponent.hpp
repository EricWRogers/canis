#pragma once
#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/Data/Bit.hpp>

namespace Canis
{
struct TextComponent
{
	int assetId = 0;
	std::string *text = nullptr; // FIX : memory leak
	unsigned int alignment = 0; // 0 is left align | 1 is right align | 2 is center align
	unsigned int _status = BIT::ONE; // this will make the RenderTextSystem recalculate rect size &| alignment
};

// _info
// bit one if 1 alignment should be recalculated

namespace Text {
	enum TextAlignment {
		LEFT   = 0u,
		RIGHT  = 1u,
		CENTER = 2u
	};

	inline void Set(TextComponent &_textComponent, RectTransformComponent &_rectComponent, std::string _text) {
		(*_textComponent.text) = _text;
		_rectComponent.originOffset = glm::vec2(0.0f);
		_textComponent._status = _textComponent._status | BIT::ONE; // the alignment should be recalculated
	}
}
} // end of Canis namespace