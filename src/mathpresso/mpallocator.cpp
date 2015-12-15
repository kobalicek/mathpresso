// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_EXPORTS

// [Dependencies]
#include "./mpallocator_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::Allocator - Helpers]
// ============================================================================

template<typename T>
static MATHPRESSO_INLINE T mpAllocatorAlign(T p, uintptr_t alignment) {
  uintptr_t mask = static_cast<uintptr_t>(alignment - 1);
  return (T)(((uintptr_t)p + mask) & ~mask);
}

// ============================================================================
// [mathpresso::Allocator - Clear / Reset]
// ============================================================================

void Allocator::clear() {
  _current = _first;
  ::memset(_slots, 0, MATHPRESSO_ARRAY_SIZE(_slots) * sizeof(Slot*));
}

void Allocator::reset() {
  Chunk* chunk = _first;

  while (chunk != NULL) {
    Chunk* next = chunk->next;
    ::free(chunk);
    chunk = next;
  }

  ::memset(this, 0, sizeof(*this));
}

// ============================================================================
// [mathpresso::Allocator - Alloc / Release]
// ============================================================================

void* Allocator::_alloc(size_t size, size_t& allocatedSize) {
  uint32_t slot;

  // We use our memory pool only if the requested block is of reasonable size.
  if (_getSlotIndex(size, slot, allocatedSize)) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(_slots[slot]);
    Chunk* chunk = _current;

    size = allocatedSize;
    if (ptr != NULL) {
      // Lucky, memory reuse.
      _slots[slot] = reinterpret_cast<Slot*>(ptr)->next;
      return ptr;
    }

    if (chunk != NULL) {
      ptr = chunk->ptr;

      uint8_t* end = (uint8_t*)chunk + kChunkSize;
      size_t remain = (size_t)(end - ptr);

      if (remain >= size) {
        // Allocate from the current chunk.
        chunk->ptr += size;
        return ptr;
      }
      else {
        // Distribute remaining memory to the slots.
        while (remain >= kLoGranularity) {
          size_t curSize = remain < kLoMaxSize ? remain : kLoMaxSize;
          slot = static_cast<uint32_t>(curSize - (kLoGranularity - 1)) / kLoCount;

          reinterpret_cast<Slot*>(ptr)->next = _slots[slot];
          _slots[slot] = reinterpret_cast<Slot*>(ptr);

          ptr += curSize;
        }
      }

      // Advance if there is a next chunk to the current one.
      if (chunk->next != NULL) {
        chunk = chunk->next;
        ptr = mpAllocatorAlign<uint8_t*>(reinterpret_cast<uint8_t*>(chunk) + sizeof(Chunk), kChunkAlignment);

        _current = chunk;
        chunk->ptr = ptr + size;

        return ptr;
      }
    }

    // Allocate a new chunk.
    chunk = static_cast<Chunk*>(::malloc(kChunkSize));
    ptr = mpAllocatorAlign<uint8_t*>(reinterpret_cast<uint8_t*>(chunk) + sizeof(Chunk), kChunkAlignment);

    if (chunk == NULL) {
      allocatedSize = 0;
      return NULL;
    }

    chunk->next = NULL;
    chunk->ptr = ptr + size;

    if (_first == NULL)
      _first = chunk;
    else
      _current->next = chunk;

    _current = chunk;
    return ptr;
  }
  else {
    void* p = ::malloc(size);
    allocatedSize = p != NULL ? size : static_cast<size_t>(0);
    return p;
  }
}

void* Allocator::_allocZeroed(size_t size, size_t& allocatedSize) {
  void* p = alloc(size, allocatedSize);
  if (p != NULL)
    ::memset(p, 0, allocatedSize);
  return p;
}

char* Allocator::_allocString(const char* s, size_t len) {
  size_t dummy;
  char* p = static_cast<char*>(alloc(len + 1, dummy));

  if (p == NULL)
    return NULL;

  // Copy string and NULL terminate as `s` doesn't have to be NULL terminated.
  if (len > 0)
    ::memcpy(p, s, len);
  p[len] = 0;

  return p;
}

bool Allocator::multiAlloc(void** pDst, const size_t* sizes, size_t count) {
  MATHPRESSO_ASSERT(count > 0);

  size_t i = 0;
  do {
    void* p = alloc(sizes[i]);
    if (p == NULL)
      goto _Fail;
    pDst[i] = p;
  } while (++i < count);
  return true;

_Fail:
  while (i > 0) {
    i--;
    release(pDst[i], sizes[i]);
  }
  return false;
}

} // mathpresso namespace
