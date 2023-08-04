#ifndef FREE_LIST_H
#define FREE_LIST_H

#include "linked_list.h"

template <class T>
class FreeList {
public:
    int count;

    FreeList() : count{0}, capacity{1024} {
        data = new T[1024];
    }

    FreeList(int capacity) : count{0}, capacity{capacity} {
        data = new T[capacity];
    }

    ~FreeList() {
        delete[] data;
    }

    int allocate(T&& value) {
        int i = count;
        data[i] = std::move(value);
        count++;
        return i;
    }

    void deallocate(int handle) {
        data[handle] = std::move(data[count - 1]);
        count--;
    }

    T& operator [](int index) {
        return data[index];
    }

private:
    T *data;
    int capacity;
};

#endif
