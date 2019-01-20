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

// ============================================================================
// [mathpresso::HashUtils]
// ============================================================================

namespace HashUtils {
  // \internal
  static MATHPRESSO_INLINE uint32_t hashPointer(const void* kPtr) {
    uintptr_t p = (uintptr_t)kPtr;
    return static_cast<uint32_t>(
      ((p >> 3) ^ (p >> 7) ^ (p >> 12) ^ (p >> 20) ^ (p >> 27)) & 0xFFFFFFFFu);
  }

  // \internal
  static MATHPRESSO_INLINE uint32_t hashChar(uint32_t hash, uint32_t c) {
    return hash * 65599 + c;
  }

  // \internal
  //
  // Get a hash of the given string `data` of size `size`. This function doesn't
  // require `key` to be NULL terminated.
  MATHPRESSO_NOAPI uint32_t hashString(const char* data, size_t size);

  // \internal
  //
  // Get a prime number that is close to `x`, but always greater than or equal to `x`.
  MATHPRESSO_NOAPI uint32_t closestPrime(uint32_t x);
};

// ============================================================================
// [mathpresso::HashNode]
// ============================================================================

struct HashNode {
  MATHPRESSO_INLINE HashNode(uint32_t hashCode = 0) : _next(NULL), _hashCode(hashCode) {}

  //! Next node in the chain, NULL if last node.
  HashNode* _next;
  //! Hash code.
  uint32_t _hashCode;
};

// ============================================================================
// [mathpresso::HashBase]
// ============================================================================

struct HashBase {
  MATHPRESSO_NONCOPYABLE(HashBase)

  enum {
    kExtraFirst = 0,
    kExtraCount = 1
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE HashBase(ZoneAllocator* allocator) {
    _allocator = allocator;
    _size = 0;

    _bucketsCount = 1;
    _bucketsGrow = 1;

    _data = _embedded;
    for (uint32_t i = 0; i <= kExtraCount; i++)
      _embedded[i] = NULL;
  }

  MATHPRESSO_INLINE ~HashBase() {
    if (_data != _embedded)
      _allocator->release(_data, static_cast<size_t>(_bucketsCount + kExtraCount) * sizeof(void*));
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE ZoneAllocator* allocator() const { return _allocator; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  MATHPRESSO_NOAPI void _rehash(uint32_t newCount);
  MATHPRESSO_NOAPI void _mergeToInvisibleSlot(HashBase& other);

  MATHPRESSO_NOAPI HashNode* _put(HashNode* node);
  MATHPRESSO_NOAPI HashNode* _del(HashNode* node);

  // --------------------------------------------------------------------------
  // [Reset / Rehash]
  // --------------------------------------------------------------------------

  ZoneAllocator* _allocator;

  uint32_t _size;
  uint32_t _bucketsCount;
  uint32_t _bucketsGrow;

  HashNode** _data;
  HashNode* _embedded[1 + kExtraCount];
};

// ============================================================================
// [mathpresso::Hash<Key, Node>]
// ============================================================================

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
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE Hash(ZoneAllocator* allocator)
    : HashBase(allocator) {}

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  template<typename ReleaseHandler>
  void reset(ReleaseHandler& handler) {
    HashNode** data = _data;
    uint32_t count = _bucketsCount + kExtraCount;

    for (uint32_t i = 0; i < count; i++) {
      HashNode* node = data[i];

      while (node != NULL) {
        HashNode* next = node->_next;
        handler.release(static_cast<Node*>(node));
        node = next;
      }
    }

    if (data != _embedded)
      _allocator->release(data, static_cast<size_t>(count + kExtraCount) * sizeof(void*));

    _bucketsCount = 1;
    _bucketsGrow = 1;

    _size = 0;
    _data = _embedded;

    for (uint32_t i = 0; i <= kExtraCount; i++)
      _embedded[i] = NULL;
  }

  MATHPRESSO_INLINE void mergeToInvisibleSlot(Hash<Key, Node>& other) {
    _mergeToInvisibleSlot(other);
  }

  MATHPRESSO_INLINE Node* get(const Key& key, uint32_t hashCode) const {
    uint32_t hMod = hashCode % _bucketsCount;
    Node* node = static_cast<Node*>(_data[hMod]);

    while (node != NULL) {
      if (node->eq(key))
        return node;
      node = static_cast<Node*>(node->_next);
    }

    return NULL;
  }

  MATHPRESSO_INLINE Node* put(Node* node) { return static_cast<Node*>(_put(node)); }
  MATHPRESSO_INLINE Node* del(Node* node) { return static_cast<Node*>(_del(node)); }
};

// ============================================================================
// [mathpresso::HashIterator<Key, Node>]
// ============================================================================

template<typename Key, typename Node>
struct HashIterator {
  MATHPRESSO_INLINE HashIterator(const Hash<Key, Node>& hash) { init(hash); }

  MATHPRESSO_INLINE bool init(const Hash<Key, Node>& hash) {
    Node** buckets = reinterpret_cast<Node**>(hash._data);
    Node* node = buckets[0];

    uint32_t index = 0;
    uint32_t count = hash._bucketsCount;

    while (node == NULL && ++index < count)
      node = buckets[index];

    _node = node;
    _buckets = buckets;

    _index = index;
    _count = count;

    return node != NULL;
  }

  MATHPRESSO_INLINE bool next() {
    // Can't be called after it reached the end.
    MATHPRESSO_ASSERT(has());

    Node* node = static_cast<Node*>(_node->_next);
    if (node == NULL) {
      uint32_t index = _index;
      uint32_t count = _count;

      while (++index < count) {
        node = _buckets[index];
        if (node != NULL) break;
      }
      _index = index;
    }

    _node = node;
    return node != NULL;
  }

  MATHPRESSO_INLINE bool has() const { return _node != NULL; }
  MATHPRESSO_INLINE Node* get() const { return _node; }

  Node* _node;
  Node** _buckets;

  uint32_t _index;
  uint32_t _count;
};

// ============================================================================
// [mathpresso::Map<Key, Value>]
// ============================================================================

//! \internal
template<typename Key, typename Value>
struct Map {
  MATHPRESSO_NONCOPYABLE(Map)

  struct Node : public HashNode {
    MATHPRESSO_INLINE Node(const Key& key, const Value& value, uint32_t hashCode)
      : HashNode(hashCode),
        _key(key),
        _value(value) {}
    MATHPRESSO_INLINE bool eq(const Key& key) { return _key == key; }

    Key _key;
    Value _value;
  };

  struct ReleaseHandler {
    MATHPRESSO_INLINE ReleaseHandler(ZoneAllocator* allocator) : _allocator(allocator) {}
    MATHPRESSO_INLINE void release(Node* node) { _allocator->release(node, sizeof(Node)); }

    ZoneAllocator* _allocator;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE Map(ZoneAllocator* allocator)
    : _hash(allocator) {}

  MATHPRESSO_INLINE ~Map() {
    ReleaseHandler releaseHandler(_hash.allocator());
    _hash.reset(releaseHandler);
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE Value get(const Key& key) const {
    uint32_t hashCode = HashUtils::hashPointer(key);
    uint32_t hMod = hashCode % _hash._bucketsCount;
    Node* node = static_cast<Node*>(_hash._data[hMod]);

    while (node != NULL) {
      if (node->eq(key))
        return node->_value;
      node = static_cast<Node*>(node->_next);
    }

    return NULL;
  }

  MATHPRESSO_INLINE Error put(const Key& key, const Value& value) {
    Node* node = static_cast<Node*>(_hash._allocator->alloc(sizeof(Node)));
    if (node == NULL)
      return MATHPRESSO_TRACE_ERROR(kErrorNoMemory);

    uint32_t hashCode = HashUtils::hashPointer(key);
    _hash.put(new(node) Node(key, value, hashCode));

    return kErrorOk;
  }

  Hash<Key, Node> _hash;
};

} // mathpresso namespace

// [Guard]
#endif // _MATHPRESSO_MPHASH_P_H
