#pragma once

#include <cstdint>
#include <cfloat>

typedef std::int8_t  i8;
typedef std::int16_t i16;
typedef std::int32_t i32;
typedef std::int64_t i64;

typedef std::uint8_t   u8; // 255
typedef std::uint16_t u16; // 65,535
typedef std::uint32_t u32; // 4,294,967,295
typedef std::uint64_t u64; // 18,446,744,073,709,551,615

#define u8_max		(255)
#define u16_max	    (65535)
#define u32_max	    (4294967295U)
#define u64_max     (18446744073709551615)

typedef float f32;
typedef double d64;

#define f32_max     (FLT_MAX)

namespace Canis
{
    struct Mask
    {
        static constexpr int BitCount = 32;

        u32 value = 0u;

        constexpr Mask() = default;
        constexpr Mask(u32 _value) : value(_value) {}

        constexpr operator u32() const { return value; }
        constexpr Mask& operator=(u32 _value)
        {
            value = _value;
            return *this;
        }

        constexpr bool operator==(const Mask& _other) const = default;

        constexpr bool HasBit(int _bitIndex) const
        {
            return _bitIndex >= 0
                && _bitIndex < BitCount
                && (value & (u32(1u) << _bitIndex)) != 0u;
        }

        constexpr void SetBit(int _bitIndex, bool _enabled)
        {
            if (_bitIndex < 0 || _bitIndex >= BitCount)
                return;

            const u32 bitValue = u32(1u) << _bitIndex;
            if (_enabled)
                value |= bitValue;
            else
                value &= ~bitValue;
        }

        constexpr void ToggleBit(int _bitIndex)
        {
            if (_bitIndex < 0 || _bitIndex >= BitCount)
                return;

            value ^= u32(1u) << _bitIndex;
        }
    };
}
