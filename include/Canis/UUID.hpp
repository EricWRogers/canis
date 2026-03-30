#pragma once
#include <cstdint>
#include <unordered_map>

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
    template<>
    struct hash<Canis::UUID> {
        std::size_t operator()(const Canis::UUID& _uuid) const {
            return static_cast<uint64_t>(_uuid);
        }
    };
}
