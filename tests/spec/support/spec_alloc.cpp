// Global operator new and delete replacements that count allocations per thread (see spec_alloc.h).

#include "spec_alloc.h"

#include <cstdlib>
#include <new>

namespace {

thread_local std::size_t allocations = 0;

void* allocate(std::size_t size) {
    ++allocations;
    return std::malloc(size == 0 ? 1 : size);
}

void* allocateAligned(std::size_t size, std::size_t alignment) {
    ++allocations;
    if (alignment < sizeof(void*))
        alignment = sizeof(void*);
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size == 0 ? 1 : size) != 0)
        return nullptr;
    return memory;
}

} // namespace

namespace spec {

std::size_t allocationsOnThisThread() { return allocations; }

AllocationCounter::AllocationCounter() : start_(allocations) {}

std::size_t AllocationCounter::count() const { return allocations - start_; }

} // namespace spec

void* operator new(std::size_t size) {
    if (void* memory = allocate(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (void* memory = allocate(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }

void* operator new(std::size_t size, std::align_val_t alignment) {
    if (void* memory = allocateAligned(size, static_cast<std::size_t>(alignment)))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    if (void* memory = allocateAligned(size, static_cast<std::size_t>(alignment)))
        return memory;
    throw std::bad_alloc();
}

void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete(void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept { std::free(memory); }
