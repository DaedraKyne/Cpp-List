#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>
#include <cassert>
#include <functional>
#include <algorithm>
#include <type_traits>

// Never use header-wide using directives ("using namespace") in the header!!
// Explanation: https://stackoverflow.com/questions/5849457/using-namespace-in-c-headers
// Basically, when this header is included, the "using namespace" will be included too, possibly causing un-intended clashes in variable/function/etc.. namess
// Note: as long as code doesn't #include a .cpp file, it's fine to use using directives in that source file

//Generic type, allow for stateful Allocator if user desires it
template <typename T, typename Allocator = std::allocator<T>>
class List {
    static_assert(!std::is_void_v<T>, "void type is not allowed");
    static_assert(!std::is_reference_v<T>, "reference type is not allowed");
    //C++ Standard �8.3.2/4: There shall be no references to references, no arrays of references, and no pointers to references.


    //note: elements are value copies of original objects (copy-by-value, not copy-by-reference)
public:
    List() : dataAllocator(), data(nullptr), capacity(0), count(0) {

    }

    List(Allocator const& alloc) : dataAllocator(alloc), data(nullptr), capacity(0), count(0) {
    
    }

    friend void swap(List& first, List& second) noexcept {
        //Purely swap data: since we want the states to remain the same as beforehand, just switched, explicitely use std::swap in case swap(Allocator&, Allocator&) somehow moves memory around
        //Note: since data is a pointer value, it cannot have a "custom swap friend function" - aka, let's just use std::swap everywhere
        std::swap(first.dataAllocator, second.dataAllocator);
        std::swap(first.capacity, second.capacity);
        std::swap(first.count, second.count);
        std::swap(first.data, second.data);
    }

    //Rule of 3
    ~List() { //Destructor
        Clear();
        dataAllocator.deallocate(data, capacity);
    }

    //Copy Constructor - dataAllocator is not copied over as the data allocated is not the same (different memory address, deep copy)
    //TODO: look into std::allocator_traits
    List(const List& other) : dataAllocator(other.dataAllocator), data(CreateDeepCopy(other, dataAllocator)),
                              capacity(other.capacity), count(other.count) {}

    List& operator=(const List& other) { //Copy assignement - keep dataAllocator the same as it was
        if (this != &other) {
            Clear();
            Capacity(other.capacity);
            const auto end = other.data + other.count;
            //double-pointer progression
            for (auto dst = data, src = other.data; src != end; ++src, ++dst) {
                new (dst) T(*src);
            }
            count = other.count;
        }
        return *this;
    } 

    //Rule of 5
    //R-value references (&&) explained: http://thbecker.net/articles/rvalue_references/section_01.html 
    //basic explanation: if a function argument is &&, whatever it references will stop existing at the end of the function
    List(List&& other) noexcept : dataAllocator(std::move(other.dataAllocator)), data(other.data), capacity(other.capacity), count(other.count) {
        //due to using std::move(other.dataAllocator), other.dataAllocator is now unrelated to *this.dataAllocator (and *this.dataAllocated retains all previous info)
        other.data = nullptr;
        other.capacity = 0;
        other.count = 0;
    }
    List& operator=(List&& other) noexcept { 
        List tmp(std::move(other));
        swap(*this, tmp);
        return *this;
        //old " *this "-related data is deleted automatically at function end (through temp destruction)
    }


    //Remove all elements - maintain capacity
    void Clear() {
        for (size_t i = 0; i < count; i++) {
            data[i].~T();
        }
        count = 0;
    }

    //Shrink array capacity down to count
    void ShrinkToFit() {
        if (capacity == count) return;
        //sadly, allocators don't support partial deallocation - need to fully resize
        Resize(count);
    }

    size_t Capacity() const { return capacity; }
    size_t Count() const { return count; }

    //Sets the capacity of the internal array to new_capacity. If new_capacity is smaller than Count, do nothing.
    void Capacity(size_t new_capacity) {
        if (new_capacity <= capacity) return; //no change
        Resize(new_capacity);
    }


    void Print() const {
        std::cout << "(";
        for (size_t i = 0; i < count - 1; i++) {
            std::cout << data[i] << ", ";
        }
        std::cout << data[count - 1] << ")\n";
    }

    //Take in any amount of arguments of any type, then unpack on element construction - allows for creating new data without checking for logic errors (the compiler and the element's constructor will take care of that)
    template<typename... Args>
    void Add(Args&&... args) {
        if (capacity == count) Capacity(std::max(size_t(2) * capacity, size_t(1)));
        new (data + count++) T(std::forward<Args>(args)...);
    }

    T* Find(const T& val) {
        for (size_t i = 0; i < count; i++) {
            if (data[i] == val) return data + i; //TODO: research why &data[i] might get overloaded
        }
        return nullptr;
    }
    const T* Find(const T& val) const {
        for (size_t i = 0; i < count; i++) {
            if (data[i] == val) return data + i; //TODO: research why &data[i] might get overloaded
        }
        return nullptr;
    }

    template <typename Predicate>
    T* FindIf(Predicate&& pred) {
        for (auto ptr = begin(); ptr < end(); ptr++) {
            if (pred(*ptr)) return ptr; //TODO: research why &data[i] might get overloaded
        }
        return nullptr;
    }

    template <typename Predicate>
    const T* FindIf(Predicate&& pred) const {
        for (auto ptr = begin(); ptr < end(); ptr++) {
            if (pred(*ptr)) return ptr; //TODO: research why &data[i] might get overloaded
        }
        return nullptr;
    }

    void RemoveAt(size_t index) {
        if (index < 0 || index >= count) throw std::out_of_range(std::string("Cannot remove element at out_of_range index: ") + std::to_string(index) + std::string("."));

        std::move(data + index + 1, data + count, data + index);
        data[--count].~T();
    }


    //Returns true if a relevant element exists, and removes it.
    size_t Remove(const T& val) {
        //Lazy, simpler way - when changing behaviour, just change it in RemoveIf:
        return RemoveIf([&](const auto& e) { return e == val; } );
    }
    
    template <typename Predicate>
    size_t RemoveIf(Predicate&& pred) {
        //idea: double pointers, picker points to next element to check,
        //                       placer points to next available space

        auto placer = FindIf(pred);
        if (placer == nullptr) return 0;

        for (auto picker = placer + 1; picker < end(); ++picker) {
            if (!pred(*picker)) {
                *placer = std::move(*picker); //*picker now contains irrelevant instantiated data
                ++placer;
            }
        }

        //if constexpr is evaluated at compile time - eg: List<int> won't have the following code when compiled
        if constexpr (!std::is_trivially_destructible<T>::value) { //those that are don't have non-empty destructors to call
            for (auto k = placer; k < end(); ++k) {
                k->~T(); //destructs meaningless data from matched indexes moved to end
            }
        }

        const auto removed = end() - placer;
        count -= removed;

        return removed;
    }

    const T& operator[](size_t index) const { return data[index]; } //read-only
    T& operator[](size_t index) { return data[index]; } //read+(later)write
    const T& Get(size_t index) const {
        if (index >= count || index < 0) throw std::out_of_range(std::string("Cannot get element at out_of_range index: ") + std::to_string(index) + std::string(")"));

        return data[index];
    }
    T& Get(size_t index) {
        if (index >= count || index < 0) throw std::out_of_range(std::string("Cannot get element at out_of_range index: ") + std::to_string(index) + std::string(")"));

        return data[index];
    }


    //can iterate through list without using iterators if internal data is contiguous
    T* begin() { return data; }
    const T* begin() const { return data; } //non-copy version for read-only
    const T* cbegin() const { return data; } //same as above, allows for direct call to cbegin for user who knows they want constant iterators
    
    T* end() { return data + count; }
    const T* end() const { return data + count; }
    const T* cend() const { return data + count; }



private:
    T* data;
    size_t capacity;
    size_t count;

    Allocator dataAllocator; //not actually necessary to maintain same allocator throughout program as allocators can allocate/deallocate any data

    static T* CreateDeepCopy(const List& list, Allocator& alloc) {
        size_t capacity = list.capacity;
        size_t count = list.count;

        assert(count <= capacity && capacity >= 0);

        T* new_data = capacity > 0 ? alloc.allocate(capacity) : nullptr;

        for (size_t i = 0; i < count; i++) {
            new (new_data + i) T(list[i]);
        }

        return new_data;
    }

    void Resize(size_t new_capacity) {
        assert(new_capacity >= count); //asserts get removed in release builds

        T* new_data = dataAllocator.allocate(new_capacity);
        const auto end = data + count;
        for (auto dest = new_data, src = data; src != end; ++src, ++dest) {
            new (dest) T(*src); //move data to new location (also clears data from old location)
            src->~T(); //delete invalid data at old location (does not own any relevant data anymore so can destruct safely)
        }

        dataAllocator.deallocate(data, capacity);

        data = new_data;
        capacity = new_capacity;
    }


};