#pragma once
#include <string.h>

namespace Canis {
    namespace List {
        extern void Init(void* _refList, unsigned int _capacity, size_t _elementSize);
        extern void Free(void* _refList);
        extern void Add(void* _refList, const void* _value);
        extern void BubbleSort(void* _refList, bool (*_compareFunc)(void*, void*));
        extern void SelectionSort(void* _refList, bool (*_compareFunc)(void*, void*));
        extern void MergeSort(void* _refList, bool (*_compareFunc)(void*, void*));
        extern unsigned int GetCount(void* _refList);
        extern int Find(void* _refList, void* _value);
        extern void* End(void* _refList);
        extern void Grow(void* _refList);

        extern void _MergeSort(void* _refList, int _begin, int _end, bool (*_compareFunc)(void*, void*));
        extern void _Merge(void* _refList, int _begin, int _mid, int _right, bool (*_compareFunc)(void*, void*));
    } // End of List

    // compare functions
    extern bool DoubleAscending(void* a, void *b);
    extern bool DoubleDescending(void* a, void *b);
    extern bool FloatAscending(void* a, void *b);
    extern bool FloatDescending(void* a, void *b);
    extern bool IntAscending(void* a, void *b);
    extern bool IntDescending(void* a, void *b);
} // End of Canis