// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPHASH_P_H
#define _MATHPRESSO_MPHASH_P_H

// [Dependencies]
#include "./mathpresso_p.h"

namespace mathpresso {

// MathPresso - HashUtils
// ======================

namespace HashUtils {
  // \internal
  static MATHPRESSO_INLINE uint32_t hash_pointer(const void* kPtr) {
    uintptr_t p = (uintptr_t)kPtr;
    return static_cast<uint32_t>(
      ((p >> 3) ^ (p >> 7) ^ (p >> 12) ^ (p >> 20) ^ (p >> 27)) & 0xFFFFFFFFu);
  }

  // \internal
  static MATHPRESSO_INLINE uint32_t hash_char(uint32_t hash, uint32_t c) {
    return hash * 65599 + c;
  }

  // \internal
  //
  // Get a hash of the given string `data` of size `size`. This function doesn't
  // require `key` to be nullptr terminated.
  MATHPRESSO_NOAPI uint32_t hash_string(const char* data, size_t size);

  // \internal
  //
  // Get a prime number that is close to `x`, but always greater than or equal to `x`.
  MATHPRESSO_NOAPI uint32_t closest_prime(uint32_t x);
};

// MathPresso - HashNode
// =====================

struct HashNode {
  MATHPRESSO_INLINE HashNode(uint32_t hash_code = 0)
    : _next(nullptr),
      _hash_code(hash_code) {}

  //! Next node in the chain, nullptr if last node.
  HashNode* _next;
  //! Hash code.
  uint32_t _hash_code;
};

// MathPresso - HashBase
// =====================

struct HashBase {
  MATHPRESSO_NONCOPYABLE(HashBase)

  enum {
    kExtraFirst = 0,
    kExtraCount = 1
  };

  // Members
  // -------

  Arena& _arena;

  uint32_t _size;
  uint32_t _buckets_count;
  uint32_t _buckets_grow;

  HashNode** _data;
  HashNode* _embedded[1 + kExtraCount];

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE HashBase(Arena& arena) : _arena(arena) {
    _size = 0;

    _buckets_count = 1;
    _buckets_grow = 1;

    _data = _embedded;
    for (uint32_t i = 0; i <= kExtraCount; i++)
      _embedded[i] = nullptr;
  }

  MATHPRESSO_INLINE ~HashBase() {
    if (_data != _embedded) {
      _arena.free_reusable(_data, static_cast<size_t>(_buckets_count + kExtraCount) * sizeof(void*));
    }
  }

  // Accessors
  // ---------

  MATHPRESSO_INLINE Arena& arena() const { return _arena; }

  // Operations
  // ----------

  MATHPRESSO_NOAPI void _rehash(uint32_t new_count);
  MATHPRESSO_NOAPI void _merge_to_invisible_slot(HashBase& other);

  MATHPRESSO_NOAPI HashNode* _put(HashNode* node);
  MATHPRESSO_NOAPI HashNode* _del(HashNode* node);
};

// MathPresso - Hash<Key, Node>
// ============================

//! \internal
//!
//! Low level hash table container used by MathPresso, with some "special"
//! features.
//!
//! Notes:
//!
//! 1. This hash table allows duplicates to be inserted (the API is so low
//!    level that it's up to you if you allow it or not, as you should first
//!    `get()` the node and then modify it or insert a new node by using `put()`,
//!    depending on the intention).
//!
//! 2. This hash table also contains "invisible" nodes that are not used by
//!    the basic hash functions, but can be used by functions having "invisible"
//!    in their name. These are used by the parser to merge symbols from the
//!    current scope that is being closed into the root local scope.
//!
//! Hash is currently used by AST to keep references of global and local
//! symbols and by AST to IR translator to associate IR specific data with AST.
template<typename Key, typename Node>
struct Hash : public HashBase {
  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE Hash(Arena& arena)
    : HashBase(arena) {}

  // Operations
  // ----------

  template<typename ReleaseHandler>
  void reset(ReleaseHandler& handler) {
    HashNode** data = _data;
    uint32_t count = _buckets_count + kExtraCount;

    for (uint32_t i = 0; i < count; i++) {
      HashNode* node = data[i];

      while (node != nullptr) {
        HashNode* next = node->_next;
        handler.release(static_cast<Node*>(node));
        node = next;
      }
    }

    if (data != _embedded) {
      _arena.free_reusable(data, static_cast<size_t>(count + kExtraCount) * sizeof(void*));
    }

    _buckets_count = 1;
    _buckets_grow = 1;

    _size = 0;
    _data = _embedded;

    for (uint32_t i = 0; i <= kExtraCount; i++)
      _embedded[i] = nullptr;
  }

  MATHPRESSO_INLINE void merge_to_invisible_slot(Hash<Key, Node>& other) {
    _merge_to_invisible_slot(other);
  }

  MATHPRESSO_INLINE Node* get(const Key& key, uint32_t hash_code) const {
    uint32_t hash_mod = hash_code % _buckets_count;
    Node* node = static_cast<Node*>(_data[hash_mod]);

    while (node != nullptr) {
      if (node->eq(key))
        return node;
      node = static_cast<Node*>(node->_next);
    }

    return nullptr;
  }

  MATHPRESSO_INLINE Node* put(Node* node) { return static_cast<Node*>(_put(node)); }
  MATHPRESSO_INLINE Node* del(Node* node) { return static_cast<Node*>(_del(node)); }
};

// MathPresso - HashIterator<Key, Node>
// ====================================

template<typename Key, typename Node>
struct HashIterator {
  MATHPRESSO_INLINE HashIterator(const Hash<Key, Node>& hash) { init(hash); }

  MATHPRESSO_INLINE bool init(const Hash<Key, Node>& hash) {
    Node** buckets = reinterpret_cast<Node**>(hash._data);
    Node* node = buckets[0];

    uint32_t index = 0;
    uint32_t count = hash._buckets_count;

    while (node == nullptr && ++index < count)
      node = buckets[index];

    _node = node;
    _buckets = buckets;

    _index = index;
    _count = count;

    return node != nullptr;
  }

  MATHPRESSO_INLINE bool next() {
    // Can't be called after it reached the end.
    MATHPRESSO_ASSERT(has());

    Node* node = static_cast<Node*>(_node->_next);
    if (node == nullptr) {
      uint32_t index = _index;
      uint32_t count = _count;

      while (++index < count) {
        node = _buckets[index];
        if (node != nullptr) break;
      }
      _index = index;
    }

    _node = node;
    return node != nullptr;
  }

  MATHPRESSO_INLINE bool has() const { return _node != nullptr; }
  MATHPRESSO_INLINE Node* get() const { return _node; }

  Node* _node;
  Node** _buckets;

  uint32_t _index;
  uint32_t _count;
};

// MathPresso - Map<Key, Value>
// ============================

//! \internal
template<typename Key, typename Value>
struct Map {
  MATHPRESSO_NONCOPYABLE(Map)

  struct Node : public HashNode {
    MATHPRESSO_INLINE Node(const Key& key, const Value& value, uint32_t hash_code)
      : HashNode(hash_code),
        _key(key),
        _value(value) {}
    MATHPRESSO_INLINE bool eq(const Key& key) { return _key == key; }

    Key _key;
    Value _value;
  };

  struct ReleaseHandler {
    Arena& _arena;

    MATHPRESSO_INLINE explicit ReleaseHandler(Arena& arena) : _arena(arena) {}
    MATHPRESSO_INLINE void release(Node* node) { _arena.free_reusable(node, sizeof(Node)); }
  };

  // Members
  // -------

  Hash<Key, Node> _hash;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE Map(Arena& arena)
    : _hash(arena) {}

  MATHPRESSO_INLINE ~Map() {
    ReleaseHandler release_handler(_hash.arena());
    _hash.reset(release_handler);
  }

  // Operations
  // ----------

  MATHPRESSO_INLINE Value get(const Key& key) const {
    uint32_t hash_code = HashUtils::hash_pointer(key);
    uint32_t hash_mod = hash_code % _hash._buckets_count;
    Node* node = static_cast<Node*>(_hash._data[hash_mod]);

    while (node != nullptr) {
      if (node->eq(key)) {
        return node->_value;
      }
      node = static_cast<Node*>(node->_next);
    }

    return nullptr;
  }

  MATHPRESSO_INLINE Error put(const Key& key, const Value& value) {
    Node* node = static_cast<Node*>(_hash._arena.alloc_oneshot(sizeof(Node)));
    if (node == nullptr) {
      return MATHPRESSO_TRACE_ERROR(kErrorNoMemory);
    }

    uint32_t hash_code = HashUtils::hash_pointer(key);
    _hash.put(new(node) Node(key, value, hash_code));

    return kErrorOk;
  }
};

} // {mathpresso}

// [Guard]
#endif // _MATHPRESSO_MPHASH_P_H
