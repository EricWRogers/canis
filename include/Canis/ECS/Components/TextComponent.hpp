#pragma once

namespace Canis
{
struct TextComponent
{
	int assetId = 0;
	std::string *text = nullptr; // FIX : memory leak
	unsigned int align = 0; // 0 is left align 1 is right align
};
} // end of Canis namespace