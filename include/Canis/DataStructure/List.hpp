#pragma once
#include <string.h>

namespace Canis {
    namespace List {
        extern void Init(void* _refList, unsigned int _capacity, size_t _elementSize);
        extern void Free(void* _refList);
        extern void Add(void* _refList, const void* _value);
        extern unsigned int GetCount(void* _refList);
        extern int Find(void* _refList, void* _value);
        extern void* End(void* _refList);
        extern void Grow(void* _refList);
    }
}