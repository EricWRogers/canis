#include <Canis/DataStructure/List.hpp>
#include <cstdlib>
#include <iostream>

namespace Canis {
namespace List {
    void Init(void* _refList, unsigned int _capacity, size_t _elementSize) {
        // unsigned int capacity
        // unsinged int count
        // size_t elementSize
        // begin
        void* data = malloc(
            sizeof(unsigned int)
            + sizeof(unsigned int)
            + sizeof(size_t)
            + (_capacity * _elementSize)
        );

        unsigned int* capacity = (unsigned int*)data;
        unsigned int* count = capacity + 1;
        size_t* elementSize = (size_t*)(count + 1);

        std::cout << "capacity : " << capacity << std::endl;
        std::cout << "count : " << count << std::endl;
        std::cout << "elementSize : " << elementSize << std::endl;
        std::cout << "_refList : " << elementSize + 1 << std::endl;
        std::cout << "sizeof : " << sizeof(size_t) << std::endl;

        *capacity = _capacity;
        *count = 0;
        *elementSize = _elementSize;

        *((void**)_refList) = elementSize + 1;
    }

    void Free(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;
        unsigned int* capacity = count - 1;

        free((void*)capacity);
        *((void**)_refList) = nullptr;
    }

    void Add(void* _refList, const void* _value) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;
        unsigned int* capacity = count - 1;

        if (*count >= *capacity)
        {
            //Grow(_refList);

            //elementSize = ((size_t*)(*((void**)_refList))) - 1;
            //unsigned int* count = ((unsigned int*)elementSize) - 1;
            //unsigned int* capacity = count - 1;

            *capacity = *capacity * 2;

            void* data = capacity;

            std::cout << "data : " << data << std::endl;

            data = realloc(
                (void*)capacity,
                sizeof(unsigned int)
                + sizeof(unsigned int)
                + sizeof(size_t)
                + (*capacity * *elementSize)
            );

            std::cout << "data : " << data << std::endl;

            capacity = (unsigned int*)data;
            count = capacity + 1;
            elementSize = (size_t*)(count + 1);
            *((void**)_refList) = elementSize + 1;
        }
        
        memcpy(
            (void*)((char*)(*(void**)_refList) + ((*count) * (*elementSize))),
            _value,
            *elementSize
        );

        *count = *count + 1;
    }

    void BubbleSort(void* _refList, bool (*_compareFunc)(void*, void*)) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;
        unsigned int* capacity = count - 1;

        void* begin = *((void**)_refList);
        void* end = (*((unsigned char**)_refList)) + (*count * (*elementSize));
        bool swapped = true;
        char* temp = new char[*elementSize];
        void* current = nullptr;
        void* next = nullptr;

        while(swapped) {
            swapped = false;
            current = begin;
            while(current != end) {
                next = ((unsigned char*)current) + (*elementSize);
                if (next != end) {
                    if (_compareFunc(current, next)) {
                        swapped = true;
                        memcpy( temp, current, *elementSize);
                        memcpy( current, next,*elementSize);
                        memcpy( next, temp, *elementSize);
                    }
                }

                current = next;
            }
        }

        delete[] temp;
    }

    void SelectionSort(void* _refList, bool (*_compareFunc)(void*, void*)) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;
        unsigned int* capacity = count - 1;

        char* temp = new char[*elementSize];
        void* current = nullptr;
        void* next = nullptr;

        int i, j, minIdx;
        for (i = 0; i < *count - 1; i++) {
            minIdx = i;
            for (j = i + 1; j < *count; j++) {
                next = ((char*)(*(void**)_refList) + (j * (*elementSize)));
                current = ((char*)(*(void**)_refList) + (minIdx * (*elementSize)));
                if (_compareFunc(current, next)) {
                    minIdx = j;
                }
            }

            if (minIdx != i) {
                current = ((char*)(*(void**)_refList) + (i * (*elementSize)));
                next = ((char*)(*(void**)_refList) + (minIdx * (*elementSize)));
                memcpy( temp, current, *elementSize);
                memcpy( current, next, *elementSize);
                memcpy( next, temp, *elementSize);
            }
        }

        delete[] temp;
    }

    unsigned int GetCount(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;

        return *count;
    }

    int Find(void* _refList, void* _value) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;

        for (int i = 0; i < *count; i++)
            if (memcmp((*((unsigned char**)_refList)) + (i * (*elementSize)),_value,*elementSize) == 0)
                return i;
        
        return -1;
    }

    void* End(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;

        return (*((unsigned char**)_refList)) + (*count * (*elementSize));
    }

    void Grow(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;
        unsigned int* capacity = count - 1;

        std::cout << "capacity " << *capacity << std::endl; 

        *capacity = *capacity * 2;

        std::cout << "capacity " << *capacity << std::endl;

        void* data = capacity;

        std::cout << "data : " << data << std::endl;

        data = realloc(
            (void*)capacity,
            sizeof(unsigned int)
            + sizeof(unsigned int)
            + sizeof(size_t)
            + (*capacity * *elementSize)
        );

        std::cout << "data : " << data << std::endl;

        capacity = (unsigned int*)data;
        count = capacity + 1;
        elementSize = (size_t*)(count + 1);
        *((void**)_refList) = elementSize + 1;

        std::cout << "capacity " << *capacity << std::endl;
    }
}  // End of List
    bool DoubleAscending(void* a, void *b) {
        return *(double*)a > *(double*)b;
    }

    bool DoubleDescending(void* a, void *b) {
        return *(double*)a < *(double*)b;
    }

    bool FloatAscending(void* a, void *b) {
        return *(float*)a > *(float*)b;
    }

    bool FloatDescending(void* a, void *b) {
        return *(float*)a < *(float*)b;
    }

    bool IntAscending(void* a, void *b) {
        return *(int*)a > *(int*)b;
    }

    bool IntDescending(void* a, void *b) {
        return *(int*)a < *(int*)b;
    }
} // End of Canis