#pragma once
#include <glm/glm.hpp>
#include <string>

namespace Canis
{
	enum RectAnchor
	{
		TOPLEFT = 0,
		TOPCENTER = 1,
		TOPRIGHT = 2,
		CENTERLEFT = 3,
		CENTER = 4,
		CENTERRIGHT = 5,
		BOTTOMLEFT = 6,
		BOTTOMCENTER = 7,
		BOTTOMRIGHT = 8
	};

	glm::vec2 static GetAnchor(const RectAnchor &_anchor, const float &_windowWidth, const float &_windowHeight)
	{
		switch (_anchor)
		{
		case RectAnchor::TOPLEFT:
		{
			return glm::vec2(0.0f, _windowHeight);
		}
		case RectAnchor::TOPCENTER:
		{
			return glm::vec2(_windowWidth / 2.0f, _windowHeight);
		}
		case RectAnchor::TOPRIGHT:
		{
			return glm::vec2(_windowWidth, _windowHeight);
		}
		case RectAnchor::CENTERLEFT:
		{
			return glm::vec2(0.0f, _windowHeight / 2.0f);
		}
		case RectAnchor::CENTER:
		{
			return glm::vec2(_windowWidth / 2.0f, _windowHeight / 2.0f);
		}
		case RectAnchor::CENTERRIGHT:
		{
			return glm::vec2(_windowWidth, _windowHeight / 2.0f);
		}
		case RectAnchor::BOTTOMLEFT:
		{
			return glm::vec2(0.0f, 0.0f);
		}
		case RectAnchor::BOTTOMCENTER:
		{
			return glm::vec2(_windowWidth / 2.0f, 0.0f);
		}
		case RectAnchor::BOTTOMRIGHT:
		{
			return glm::vec2(_windowWidth, 0.0f);
		}
		default:
		{
			return glm::vec2(0.0f);
		}
		}
	}
	struct RectTransformComponent
	{
		bool active = true;
		int anchor = RectAnchor::BOTTOMLEFT;
		glm::vec2 position = glm::vec2(0.0f);
		glm::vec2 size = glm::vec2(1.0f);
		glm::vec2 originOffset = glm::vec2(0.0f);
		float rotation = 0.0f;
		float scale = 1.0f;
		float depth = 1.0f;
	};
} // end of Canis namespace