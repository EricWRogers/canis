#include <Canis/DataStructure/List.hpp>
#include <cstdlib>
#include <iostream>

namespace Canis {
namespace List {
    void Init(void*& _list, unsigned int _capacity, size_t _elementSize) {
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
        std::cout << "_list : " << elementSize + 1 << std::endl;
        std::cout << "sizeof : " << sizeof(size_t) << std::endl;

        *capacity = _capacity;
        *count = 0;
        *elementSize = _elementSize;

        _list = elementSize + 1;
    }

    void Add(void*& _list, const void* _value) {
        size_t* elementSize = ((size_t*)_list) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;
        unsigned int* capacity = count - 1;

        /*std::cout << "capacity : " << capacity << std::endl;
        std::cout << "count : " << count << std::endl;
        std::cout << "elementSize : " << elementSize << std::endl;
        std::cout << "_list : " << _list << std::endl;
        std::cout << "sizeof : " << sizeof(size_t) << std::endl;
        std::cout << "here" << std::endl;
        std::cout << "count : " << (*count) << std::endl;*/

        if (*count >= *capacity)
        {
            // resize
        }
        
        memcpy(
            (void*)((char*)_list + ((*count) * (*elementSize))),
            _value,
            *elementSize
        );

        *count = *count + 1;
    }

    unsigned int Length(void*& _list) {
        size_t* elementSize = ((size_t*)_list) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;

        return *count;
    }

    void Grow(void*& _list) {
        size_t* elementSize = ((size_t*)_list) - 1;
        unsigned int* count = ((unsigned int*)elementSize) - 1;
        unsigned int* capacity = count - 1;

        *capacity = *capacity * 2;

        void* data = malloc(
            sizeof(unsigned int)
            + sizeof(unsigned int)
            + sizeof(size_t)
            + (*capacity * *elementSize)
        );

        // copy data

        // return new data
    }
}
}