#pragma once

namespace Canis
{
struct TextComponent
{
	int assetId = 0;
	std::string *text = nullptr; // FIX : memory leak
};
} // end of Canis namespace