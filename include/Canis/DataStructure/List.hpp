#pragma once
#include <string.h>

namespace Canis {
    namespace List {
        extern void Init(void*& _list, unsigned int _capacity, size_t _elementSize);
        extern void Add(void*& _list, const void* _value);
        extern unsigned int Length(void*& _list);
        extern void Grow(void*& _list);
    }
}