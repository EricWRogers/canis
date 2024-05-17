#pragma once
#include <cstdint>

namespace Canis {
    class UUID
    {
		public:
			UUID();
			UUID(uint64_t _uuid);
			UUID(const UUID&) = default;

			operator uint64_t() const { return ID; }

			uint64_t ID;
    };
}

namespace std {
	template <typename T> struct hash;

	template<>
	struct hash<Canis::UUID>
	{
		size_t operator()(const Canis::UUID& _uuid) const
		{
			return (uint64_t)_uuid;
		}
	};

}