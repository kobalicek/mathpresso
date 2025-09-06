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

static const uint32_t prime_table[] = {
  19, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593
};

// MathPresso - HashUtils
// ======================

uint32_t HashUtils::hash_string(const char* data, size_t size) {
  if (size == 0)
    return 0;

  uint32_t hash_code = uint8_t(*data++);
  if (--size == 0)
    return hash_code;

  do {
    hash_code = HashUtils::hash_char(hash_code, uint8_t(*data++));
  } while (--size);

  return hash_code;
}

uint32_t HashUtils::closest_prime(uint32_t x) {
  uint32_t p, i = 0;

  do {
    if ((p = prime_table[i]) > x)
      break;
  } while (++i < MATHPRESSO_ARRAY_SIZE(prime_table));

  return p;
}

// MathPresso - HashBase
// =====================

void HashBase::_rehash(uint32_t new_count) {
  Arena& arena = _arena;

  HashNode** old_data = _data;
  HashNode** new_data = static_cast<HashNode**>(arena.alloc_reusable_zeroed(static_cast<size_t>(new_count + kExtraCount) * sizeof(void*)));

  if (new_data == nullptr) {
    return;
  }

  uint32_t i;
  uint32_t old_count = _buckets_count;

  for (i = 0; i < old_count; i++) {
    HashNode* node = old_data[i];
    while (node != nullptr) {
      HashNode* next = node->_next;
      uint32_t hash_mod = node->_hash_code % new_count;

      node->_next = new_data[hash_mod];
      new_data[hash_mod] = node;

      node = next;
    }
  }

  // Move extra entries.
  for (i = 0; i < kExtraCount; i++) {
    new_data[i + new_count] = old_data[i + old_count];
  }

  // 90% is the maximum occupancy, can't overflow since the maximum capacity
  // is limited to the last prime number stored in the `prime_table[]` array.
  _buckets_count = new_count;
  _buckets_grow = new_count * 9 / 10;

  _data = new_data;
  if (old_data != _embedded) {
    arena.free_reusable(old_data, static_cast<size_t>(old_count + kExtraCount) * sizeof(void*));
  }
}

void HashBase::_merge_to_invisible_slot(HashBase& other) {
  uint32_t i, count = other._buckets_count + kExtraCount;
  HashNode** data = other._data;

  HashNode* first;
  HashNode* last;

  // Find the `first` node.
  for (i = 0; i < count; i++) {
    first = data[i];
    if (first != nullptr)
      break;
  }

  if (first != nullptr) {
    // Initialize `first` and `last`.
    last = first;
    while (last->_next != nullptr)
      last = last->_next;
    data[i] = nullptr;

    // Iterate over the rest and append so `first` stay the same and `last`
    // is updated to the last node added.
    while (++i < count) {
      HashNode* node = data[i];
      if (node != nullptr) {
        last->_next = node;
        last = node;
        while (last->_next != nullptr)
          last = last->_next;
        data[i] = nullptr;
      }
    }

    // Link with ours.
    if (last != nullptr) {
      i = _buckets_count + kExtraFirst;
      last->_next = _data[i];
      _data[i] = first;
    }
  }
}

HashNode* HashBase::_put(HashNode* node) {
  uint32_t hash_mod = node->_hash_code % _buckets_count;
  HashNode* next = _data[hash_mod];

  node->_next = next;
  _data[hash_mod] = node;

  if (++_size >= _buckets_grow && next != nullptr) {
    uint32_t new_capacity = HashUtils::closest_prime(_buckets_count + kExtraCount);
    if (new_capacity != _buckets_count)
      _rehash(new_capacity);
  }

  return node;
}

HashNode* HashBase::_del(HashNode* node) {
  uint32_t hash_mod = node->_hash_code % _buckets_count;

  HashNode** prev = &_data[hash_mod];
  HashNode* p = *prev;

  while (p != nullptr) {
    if (p == node) {
      *prev = p->_next;
      return node;
    }

    prev = &p->_next;
    p = *prev;
  }

  return nullptr;
}

} // {mathpresso}
