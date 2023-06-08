#pragma once

namespace Canis
{
struct TextComponent
{
	int assetId = 0;
	std::string *text = nullptr; // FIX : memory leak
	unsigned int align = 0; // 0 is left align | 1 is right align | 2 is center align
	glm::vec2 _textOffset = glm::vec2(0.0f); // this should only be modified by RenderTextSystem
};

namespace Text {
	enum TextAlignment {
		LEFT   = 0u,
		RIGHT  = 1u,
		CENTER = 2u
	};

	inline void Set(TextComponent &_textComponent, std::string _text) {
		(*_textComponent.text) = _text;
		_textComponent._textOffset = glm::vec2(0.0f);
	}
}
} // end of Canis namespace