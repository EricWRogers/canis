#include <Canis/DataStructure/List.hpp>
#include <cstdlib>
#include <iostream>

// uint32_t capacity
// uint32_t count
// size_t elementSize
// beginning of array

namespace Canis {
namespace List {
    void Init(void* _refList, uint32_t _capacity, size_t _elementSize) {
        void* data = malloc(
            sizeof(uint32_t)
            + sizeof(uint32_t)
            + sizeof(size_t)
            + (_capacity * _elementSize)
        );

        uint32_t* capacity = (uint32_t*)data;
        uint32_t* count = capacity + 1;
        size_t* elementSize = (size_t*)(count + 1);
        *((void**)_refList) = elementSize + 1;

        // init variables
        *capacity = _capacity;
        *count = 0;
        *elementSize = _elementSize;
    }

    void Free(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;
        uint32_t* capacity = count - 1;

        free((void*)capacity);
        *((void**)_refList) = nullptr;
    }

    void Clear(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;

        *count = 0;
    }

    void Add(void* _refList, const void* _value) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;
        uint32_t* capacity = count - 1;

        if (*count >= *capacity)
        {
            *capacity = *capacity * 2;

            void* data = capacity;

            data = realloc(
                (void*)capacity,
                sizeof(uint32_t)
                + sizeof(uint32_t)
                + sizeof(size_t)
                + (*capacity * *elementSize)
            );

            capacity = (uint32_t*)data;
            count = capacity + 1;
            elementSize = (size_t*)(count + 1);
            *((void**)_refList) = elementSize + 1;
        }
        
        memcpy(
            (void*)((char*)(*(void**)_refList) + ((*count) * (*elementSize))),
            _value,
            *elementSize
        );

        (*count)++;
    }

    void BubbleSort(void* _refList, bool (*_compareFunc)(void*, void*)) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;
        uint32_t* capacity = count - 1;

        void* begin = *((void**)_refList);
        void* end = (*((char**)_refList)) + (*count * (*elementSize));
        bool swapped = true;
        char* temp = new char[*elementSize];
        void* current = nullptr;
        void* next = nullptr;

        while(swapped) {
            swapped = false;
            current = begin;
            while(current != end) {
                next = ((char*)current) + (*elementSize);
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
        uint32_t* count = ((uint32_t*)elementSize) - 1;
        uint32_t* capacity = count - 1;

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

    void MergeSort(void* _refList, bool (*_compareFunc)(void*, void*)) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;
        uint32_t* capacity = count - 1;

        if (*count < 2)
            return;
        
        uint32_t begin = 0;
        uint32_t end = *count - 1;
        uint32_t mid = begin + (end - begin) / 2;

        _MergeSort(_refList, begin, mid, _compareFunc);
        _MergeSort(_refList, mid + 1, end, _compareFunc);
        _Merge(_refList, begin, mid, end, _compareFunc);
    }

    void _MergeSort(void* _refList, int _begin, int _end, bool (*_compareFunc)(void*, void*)) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;
        uint32_t* capacity = count - 1;

        if (_begin >= _end)
            return;

        uint32_t mid = _begin + (_end - _begin) / 2;

        _MergeSort(_refList, _begin, mid, _compareFunc);
        _MergeSort(_refList, mid + 1, _end, _compareFunc);
        _Merge(_refList, _begin, mid, _end, _compareFunc);
    }
    
    void _Merge(void* _refList, int _left, int _mid, int _right, bool (*_compareFunc)(void*, void*)) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;

        int const subArrayOneSize = _mid - _left + 1;
        int const subArrayTwoSize = _right - _mid;
    
        // Create temp arrays
        char* leftArray = new char[(subArrayOneSize) * (*elementSize)];
        char* rightArray = new char[(subArrayTwoSize) * (*elementSize)];
    
        // Copy data to temp arrays leftArray[] and rightArray[]
        memcpy( leftArray, ((char*)(*(void**)_refList) + (_left * (*elementSize))), (subArrayOneSize) * (*elementSize));
        memcpy( rightArray, ((char*)(*(void**)_refList) + ((_mid+1) * (*elementSize))), (subArrayTwoSize) * (*elementSize));

        int indexOfSubArrayOne = 0;
        int indexOfSubArrayTwo = 0;
        int indexOfMergedArray = _left;

        // Merge the temp arrays back into array[left..right]
        while (indexOfSubArrayOne < subArrayOneSize && indexOfSubArrayTwo < subArrayTwoSize) {
            if (!_compareFunc(leftArray + (indexOfSubArrayOne * (*elementSize)), rightArray + (indexOfSubArrayTwo * (*elementSize)))) {
                //std::cout << *(double*)(leftArray + (indexOfSubArrayOne * (*elementSize))) << std::endl;
                memcpy(
                    ((char*)(*(void**)_refList) + (indexOfMergedArray * (*elementSize))),
                    leftArray + (indexOfSubArrayOne * (*elementSize)),
                    *elementSize
                );
                indexOfSubArrayOne++;
            }
            else {
                //std::cout << *(double*)(rightArray + (indexOfSubArrayTwo * (*elementSize))) << std::endl;
                memcpy(
                    ((char*)(*(void**)_refList) + (indexOfMergedArray * (*elementSize))),
                    rightArray + (indexOfSubArrayTwo * (*elementSize)),
                    *elementSize
                );
                indexOfSubArrayTwo++;
            }
            indexOfMergedArray++;
        }
    
        while (indexOfSubArrayOne < subArrayOneSize) {
            memcpy(
                ((char*)(*(void**)_refList) + (indexOfMergedArray * (*elementSize))),
                leftArray + (indexOfSubArrayOne * (*elementSize)),
                *elementSize
            );
            indexOfSubArrayOne++;
            indexOfMergedArray++;
        }

        while (indexOfSubArrayTwo < subArrayTwoSize) {
            memcpy(
                ((char*)(*(void**)_refList) + (indexOfMergedArray * (*elementSize))),
                rightArray + (indexOfSubArrayTwo * (*elementSize)),
                *elementSize
            );
            indexOfSubArrayTwo++;
            indexOfMergedArray++;
        }
       
        delete[] leftArray;
        delete[] rightArray;
    }

    uint32_t GetCount(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;

        return *count;
    }

    int Find(void* _refList, void* _value) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;

        for (int i = 0; i < *count; i++)
            if (memcmp((*((char**)_refList)) + (i * (*elementSize)),_value,*elementSize) == 0)
                return i;
        
        return -1;
    }

    void* End(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;

        return (*((char**)_refList)) + (*count * (*elementSize));
    }

    void Grow(void* _refList) {
        size_t* elementSize = ((size_t*)(*((void**)_refList))) - 1;
        uint32_t* count = ((uint32_t*)elementSize) - 1;
        uint32_t* capacity = count - 1;

        std::cout << "capacity " << *capacity << std::endl; 

        *capacity = *capacity * 2;

        std::cout << "capacity " << *capacity << std::endl;

        void* data = capacity;

        std::cout << "data : " << data << std::endl;

        data = realloc(
            (void*)capacity,
            sizeof(uint32_t)
            + sizeof(uint32_t)
            + sizeof(size_t)
            + (*capacity * *elementSize)
        );

        std::cout << "data : " << data << std::endl;

        capacity = (uint32_t*)data;
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