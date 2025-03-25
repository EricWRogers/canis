#pragma once
#include <glm/glm.hpp>
#include <string>
#include <Canis/Yaml.hpp>
#include <Canis/Entity.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>

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

	static const char *RectAnchorLabels[] = {
		"Top Left", "Top Center", "Top Right",
		"Center Left", "Center", "Center Right",
		"Bottom Left", "Bottom Center", "Bottom Right"};

	enum ScaleWithScreen
	{
		NONE = 0,
		WIDTH = 1,
		HEIGHT = 2,
		WIDTHANDHEIGHT = 3
	};

	static const char *ScaleWithScreenLabels[] = {
		"None", "Width", "Height", "Width And Height"};

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

	struct RectTransform
	{
		bool active = true;
		int anchor = RectAnchor::BOTTOMLEFT;
		glm::vec2 position = glm::vec2(0.0f);
		glm::vec2 size = glm::vec2(1.0f);
		bool inheritWidth = false;
		bool inheritHeight = false;
		glm::vec2 originOffset = glm::vec2(0.0f);
		float rotation = 0.0f;
		float scale = 1.0f;
		float depth = 1.0f;
		int scaleWithScreen = ScaleWithScreen::NONE;
		glm::vec2 rotationOriginOffset = glm::vec2(0.0f);
		Canis::Entity parent;
		std::vector<Canis::Entity> children;

		static void RegisterProperties()
		{
			REGISTER_PROPERTY(Canis::RectTransform, active, bool);
			REGISTER_PROPERTY(Canis::RectTransform, anchor, int);
			REGISTER_PROPERTY(Canis::RectTransform, position, glm::vec2);
			REGISTER_PROPERTY(Canis::RectTransform, size, glm::vec2);
			REGISTER_PROPERTY(Canis::RectTransform, inheritWidth, bool);
			REGISTER_PROPERTY(Canis::RectTransform, inheritHeight, bool);
			REGISTER_PROPERTY(Canis::RectTransform, originOffset, glm::vec2);
			REGISTER_PROPERTY(Canis::RectTransform, rotation, float);
			REGISTER_PROPERTY(Canis::RectTransform, scale, float);
			REGISTER_PROPERTY(Canis::RectTransform, depth, float);
			REGISTER_PROPERTY(Canis::RectTransform, scaleWithScreen, int);
			REGISTER_PROPERTY(Canis::RectTransform, rotationOriginOffset, glm::vec2);
			REGISTER_PROPERTY(Canis::RectTransform, parent, Canis::Entity);
			REGISTER_PROPERTY_VECTOR(Canis::RectTransform, children, std::vector<Canis::Entity>);
		}

		bool GetGlobalActive()
		{
			if (active == false)
				return false;

			Canis::Entity currentParent = parent;
			while (currentParent)
			{
				RectTransform &rtc = currentParent.GetComponent<RectTransform>();
				if (rtc.active)
				{
					currentParent = rtc.parent;
				}
				else
				{
					return false;
				}
			}

			return active;
		}

		float GetGlobalDepth()
		{
			float depthOffset = 0.0f;
			Canis::Entity currentParent = parent;

			while (currentParent)
			{
				RectTransform &rtc = currentParent.GetComponent<RectTransform>();
				depthOffset += rtc.depth;
				if (rtc.active)
				{
					currentParent = rtc.parent;
				}
			}

			return depth + depthOffset;
		}

		glm::vec2 GetGlobalPosition(int _canvasWidth, int _canvasHeight)
		{
			glm::vec2 offset = glm::vec2(0.0f);

			if (parent)
				offset = parent.GetComponent<RectTransform>().GetGlobalPosition(_canvasWidth, _canvasHeight);

			if (inheritWidth)
			{
				if (parent)
				{
					size.x = parent.GetComponent<RectTransform>().size.x;
				}
				else
				{
					size.x = _canvasWidth;
				}
			}

			if (inheritHeight)
			{
				if (parent)
				{
					size.y = parent.GetComponent<RectTransform>().size.y;
				}
				else
				{
					size.y = _canvasHeight;
				}
			}

			offset += GetGlobalArchor(_canvasWidth, _canvasHeight);
			return position + offset;
		}

		glm::vec2 GetGlobalArchor(int _canvasWidth, int _canvasHeight)
		{
			if (parent)
			{
				RectTransform &parentRect = parent.GetComponent<RectTransform>();
				return GetAnchor((Canis::RectAnchor)anchor,
								 parentRect.size.x * parentRect.scale,
								 parentRect.size.y * parentRect.scale);
			}
			else
			{
				return GetAnchor((Canis::RectAnchor)anchor,
								 (float)_canvasWidth,
								 (float)_canvasHeight);
			}
		}
	};

	namespace Text
	{
		inline void Set(TextComponent &_textComponent, RectTransform &_rectComponent, const std::string &_text)
		{
			_textComponent.text = _text;
			_rectComponent.originOffset = glm::vec2(0.0f);
			_textComponent._status = _textComponent._status | BIT::ONE; // the alignment should be recalculated
		}
	}
} // end of Canis namespace