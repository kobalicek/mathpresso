// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mphash_p.h"

namespace mathpresso {

// ============================================================================
// [Helpers]
// ============================================================================

static const uint32_t mpPrimeTable[] = {
  19, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593
};

// ============================================================================
// [mathpresso::HashUtils]
// ============================================================================

uint32_t HashUtils::hashString(const char* data, size_t size) {
  if (size == 0)
    return 0;

  uint32_t hashCode = uint8_t(*data++);
  if (--size == 0)
    return hashCode;

  do {
    hashCode = HashUtils::hashChar(hashCode, uint8_t(*data++));
  } while (--size);

  return hashCode;
}

uint32_t HashUtils::closestPrime(uint32_t x) {
  uint32_t p, i = 0;

  do {
    if ((p = mpPrimeTable[i]) > x)
      break;
  } while (++i < MATHPRESSO_ARRAY_SIZE(mpPrimeTable));

  return p;
}

// ============================================================================
// [mathpresso::HashBase - Reset / Rehash]
// ============================================================================

void HashBase::_rehash(uint32_t newCount) {
  ZoneAllocator* allocator = _allocator;

  HashNode** oldData = _data;
  HashNode** newData = static_cast<HashNode**>(
    allocator->allocZeroed(
      static_cast<size_t>(newCount + kExtraCount) * sizeof(void*)));

  if (newData == NULL)
    return;

  uint32_t i;
  uint32_t oldCount = _bucketsCount;

  for (i = 0; i < oldCount; i++) {
    HashNode* node = oldData[i];
    while (node != NULL) {
      HashNode* next = node->_next;
      uint32_t hMod = node->_hashCode % newCount;

      node->_next = newData[hMod];
      newData[hMod] = node;

      node = next;
    }
  }

  // Move extra entries.
  for (i = 0; i < kExtraCount; i++) {
    newData[i + newCount] = oldData[i + oldCount];
  }

  // 90% is the maximum occupancy, can't overflow since the maximum capacity
  // is limited to the last prime number stored in the `mpPrimeTable[]` array.
  _bucketsCount = newCount;
  _bucketsGrow = newCount * 9 / 10;

  _data = newData;
  if (oldData != _embedded)
    allocator->release(oldData,
      static_cast<size_t>(oldCount + kExtraCount) * sizeof(void*));
}

void HashBase::_mergeToInvisibleSlot(HashBase& other) {
  uint32_t i, count = other._bucketsCount + kExtraCount;
  HashNode** data = other._data;

  HashNode* first;
  HashNode* last;

  // Find the `first` node.
  for (i = 0; i < count; i++) {
    first = data[i];
    if (first != NULL)
      break;
  }

  if (first != NULL) {
    // Initialize `first` and `last`.
    last = first;
    while (last->_next != NULL)
      last = last->_next;
    data[i] = NULL;

    // Iterate over the rest and append so `first` stay the same and `last`
    // is updated to the last node added.
    while (++i < count) {
      HashNode* node = data[i];
      if (node != NULL) {
        last->_next = node;
        last = node;
        while (last->_next != NULL)
          last = last->_next;
        data[i] = NULL;
      }
    }

    // Link with ours.
    if (last != NULL) {
      i = _bucketsCount + kExtraFirst;
      last->_next = _data[i];
      _data[i] = first;
    }
  }
}

// ============================================================================
// [mathpresso::HashBase - Ops]
// ============================================================================

HashNode* HashBase::_put(HashNode* node) {
  uint32_t hMod = node->_hashCode % _bucketsCount;
  HashNode* next = _data[hMod];

  node->_next = next;
  _data[hMod] = node;

  if (++_size >= _bucketsGrow && next != NULL) {
    uint32_t newCapacity = HashUtils::closestPrime(_bucketsCount + kExtraCount);
    if (newCapacity != _bucketsCount)
      _rehash(newCapacity);
  }

  return node;
}

HashNode* HashBase::_del(HashNode* node) {
  uint32_t hMod = node->_hashCode % _bucketsCount;

  HashNode** pPrev = &_data[hMod];
  HashNode* p = *pPrev;

  while (p != NULL) {
    if (p == node) {
      *pPrev = p->_next;
      return node;
    }

    pPrev = &p->_next;
    p = *pPrev;
  }

  return NULL;
}

} // mathpresso namespace
