#pragma once
#include <Canis/UUID.hpp>

namespace Canis
{
    struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(UUID _id) { ID = _id; }
	};
}