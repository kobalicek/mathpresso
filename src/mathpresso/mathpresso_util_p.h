// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _MATHPRESSO_UTIL_P_H
#define _MATHPRESSO_UTIL_P_H

#include "mathpresso.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <new>

#if defined(_MSC_VER)
#include <windows.h>
#endif // _MSC_VER

namespace mathpresso {

typedef unsigned int uint;

// ============================================================================
// [MP_INVALID_INDEX]
// ============================================================================

#define MP_INVALID_INDEX ((size_t)-1)

// ============================================================================
// [MP_DISABLE_COPY]
// ============================================================================

#define MP_DISABLE_COPY(__type__) \
private: \
  inline __type__(const __type__& other); \
  inline __type__& operator=(const __type__& other);

// ============================================================================
// [mathpresso::Assert]
// ============================================================================

//! \internal
//!
//! MathPresso assertion handler.
MATHPRESSO_NOAPI void mpAssertionFailure(const char* expression, int line);

#define MP_ASSERT(exp) do { if (!(exp)) ::mathpresso::mpAssertionFailure(#exp, __LINE__); } while (0)
#define MP_ASSERT_NOT_REACHED() do { ::mathpresso::mpAssertionFailure("Not reached", __LINE__); } while (0)

// ============================================================================
// [mathpresso::MPAtomic]
// ============================================================================

struct MATHPRESSO_NOAPI MPAtomic
{
  inline void init(size_t val) { _val = val; }
  inline size_t get() const { return _val; }

  inline void inc() {
#if defined(_MSC_VER)
#if (defined(__x86_64__) || defined(_WIN64) || defined(_M_IA64) || defined(_M_X64))
    InterlockedIncrement64((LONGLONG volatile *)&_val);
#else
    InterlockedIncrement((LONG volatile *)&_val);
#endif
#elif defined(__clang__) || defined(__GNUC__)
    __sync_fetch_and_add(&_val, 1);
#else
#error "mathpresso::MPAtomic - Unsupported compiler."
#endif
  }

  inline bool dec() {
#if defined(_MSC_VER)
#if (defined(__x86_64__) || defined(_WIN64) || defined(_M_IA64) || defined(_M_X64))
    return InterlockedDecrement64((LONGLONG volatile *)&_val) == (LONGLONG)0;
#else
    return InterlockedDecrement((LONG volatile *)&_val) == (LONG)0;
#endif
#elif defined(__clang__) || defined(__GNUC__)
    return __sync_fetch_and_sub(&_val, 1) == 1;
#else
#error "mathpresso::MPAtomic - Unsupported compiler."
#endif
  }

  volatile size_t _val;
};

// ============================================================================
// [mathpresso::mpIsXXX]
// ============================================================================

static inline bool mpIsSpace(uint uc) { return uc <= ' ' && (uc == ' ' || (uc <= 0x0D && uc >= 0x09)); }
static inline bool mpIsDigit(uint uc) { return uc >= '0' && uc <= '9'; }
static inline bool mpIsAlpha(uint uc) { return (uc | 0x20) >= 'a' && (uc | 0x20) <= 'z'; }
static inline bool mpIsAlnum(uint uc) { return mpIsAlpha(uc) || mpIsDigit(uc); }

// ============================================================================
// [mathpresso::mpConvertToDouble]
// ============================================================================

MATHPRESSO_NOAPI double mpConvertToDouble(const char* str, size_t length, bool* ok);

// ============================================================================
// [mathpresso::Vector<T>]
// ============================================================================

//! Template used to store and manage array of POD data (for example pointers).
//!
//! This template has these adventages over other std::vector<> like templates:
//! - Non-copyable (designed to be non-copyable, we want it).
//! - No copy-on-write (some implementations of STL can use it).
//! - Optimized for working only with POD types.
template <typename T, unsigned int N = 0>
struct MATHPRESSO_NOAPI Vector {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create new instance of Vector template. Data won't be allocated.
  inline Vector()
    : _capacity(N),
      _length(0) {
    _data = getEmbeddedData();
  }
  //! Destroy the `Vector` and free all data.
  inline ~Vector() {
    if (_data != getEmbeddedData())
      ::free(_data);
  }

  // --------------------------------------------------------------------------
  // [Data]
  // --------------------------------------------------------------------------

  //! Get vector capacity.
  inline size_t getCapacity() const { return _capacity; }
  //! Get vector length.
  inline size_t getLength() const { return _length; }

  //! Get vector data.
  inline T* getData() { return _data; }
  //! \overload
  inline const T* getData() const { return _data; }

  // --------------------------------------------------------------------------
  // [Manipulation]
  // --------------------------------------------------------------------------

  //! Clear vector data, but not free internal buffer.
  void clear();
  //! Clear vector data and free internal buffer.
  void free();

  //! Prepend `item` to the vector.
  bool prepend(const T& item);
  //! Insert an `item` at the `index`.
  bool insert(size_t index, const T& item);
  //! Append `item` to the vector.
  bool append(const T& item);

  //! Return index of `val` or `MP_INVALID_INDEX` if not found.
  size_t indexOf(const T& val) const;

  //! Remove element at index `i`.
  void removeAt(size_t i);

  //! Swap this vector with `other`.
  template<unsigned int M>
  bool swap(Vector<T, M>& other);

  //! Get item at position `i`.
  inline T& operator[](size_t i) {
    MP_ASSERT(i < _length);
    return _data[i];
  }
  //! Get item at position `i`.
  inline const T& operator[](size_t i) const {
    MP_ASSERT(i < _length);
    return _data[i];
  }

  //! Append the item and return address so it can be initialized.
  T* newItem() {
    if (_length == _capacity && !_grow()) return NULL;
    return _data + (_length++);
  }

  // --------------------------------------------------------------------------
  // [Private]
  // --------------------------------------------------------------------------

private:
  //! Called to grow internal array.
  bool _grow();

  //! Realloc internal array to fit `to` items.
  bool _realloc(size_t to);

  inline T* getEmbeddedData() const {
    return (T*)(_embedded + sizeof(T*));
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Capacity of buffer (maximum items that can fit to current array).
  size_t _capacity;
  //! Length of buffer (count of items in array).
  size_t _length;

  // This is hack, but if N is zero some compilers can complain about zero
  // sized array[]. So I adjusted the array to be always sizeof(void*), so
  // first 4-8 bytes are shared with _data.
  union {
    //! Data.
    T* _data;
    //! Embedded data (see the template `N` parameter).
    char _embedded[sizeof(T*) + N * sizeof(T)];
  };

private:
  // DISABLE COPY.
  inline Vector<T, N>(const Vector<T, N>& other);
  inline Vector<T, N>& operator=(const Vector<T, N>& other);
};

template <typename T, unsigned int N>
void Vector<T, N>::clear() {
  _length = 0;
}

template<typename T, unsigned int N>
void Vector<T, N>::free() {
  if (_data != getEmbeddedData()) ::free(_data);

  _capacity = N;
  _length = 0;

  _data = getEmbeddedData();
}

template<typename T, unsigned int N>
bool Vector<T, N>::prepend(const T& item) {
  if (_length == _capacity && !_grow()) return false;

  memmove(_data + 1, _data, sizeof(T) * _length);
  memcpy(_data, &item, sizeof(T));

  _length++;
  return true;
}

template<typename T, unsigned int N>
bool Vector<T, N>::insert(size_t index, const T& item) {
  MP_ASSERT(index <= _length);
  if (_length == _capacity && !_grow()) return false;

  T* dst = _data + index;
  memmove(dst + 1, dst, _length - index);
  memcpy(dst, &item, sizeof(T));

  _length++;
  return true;
}

template<typename T, unsigned int N>
bool Vector<T, N>::append(const T& item) {
  if (_length == _capacity && !_grow()) return false;
  memcpy(_data + _length, &item, sizeof(T));

  _length++;
  return true;
}

template<typename T, unsigned int N>
size_t Vector<T, N>::indexOf(const T& val) const {
  size_t i = 0, len = _length;
  for (i = 0; i < len; i++) { if (_data[i] == val) return i; }
  return MP_INVALID_INDEX;
}

template<typename T, unsigned int N>
void Vector<T, N>::removeAt(size_t i) {
  MP_ASSERT(i < _length);

  T* dst = _data + i;
  _length--;
  memmove(dst, dst + 1, _length - i);
}

template<typename T, unsigned int N>
template<unsigned int M>
bool Vector<T, N>::swap(Vector<T, M>& other) {
  if (_data == getEmbeddedData() && _length)
    _realloc(_length);

  if (other._data == other.getEmbeddedData() && other._length)
    other._realloc(other._length);

  size_t _tmp_capacity = _capacity;
  size_t _tmp_length = _length;
  T* _tmp_data = _data;

  if (other._data != other.getEmbeddedData()) {
    _capacity = other._capacity;
    _length = other._length;
    _data = other._data;
  }
  else {
    _capacity = N;
    _length = 0;
    _data = getEmbeddedData();
  }

  if (_tmp_data != getEmbeddedData()) {
    other._capacity = _tmp_capacity;
    other._length = _tmp_length;
    other._data = _tmp_data;
  }
  else {
    other._capacity = M;
    other._length = 0;
    other._data = other.getEmbeddedData();
  }

  return true;
}

template<typename T, unsigned int N>
bool Vector<T, N>::_grow() {
  return _realloc(_capacity < 16 ? 16 : _capacity * 2);
}

template<typename T, unsigned int N>
bool Vector<T, N>::_realloc(size_t to) {
  MP_ASSERT(to >= _length);

  T* p = reinterpret_cast<T*>(::malloc(to * sizeof(T)));
  if (!p) return false;

  memcpy(p, _data, _length * sizeof(T));
  if (_data != getEmbeddedData()) ::free(_data);

  _data = p;
  _capacity = to;
  return true;
}

template<typename T, unsigned int N>
static void mpDeleteAll(Vector<T, N>& vector) {
  size_t i, len = vector.getLength();
  for (i = 0; i < len; i++)
    delete vector[i];
  vector.clear();
}

// ============================================================================
// [Hash<T>]
// ============================================================================

MATHPRESSO_NOAPI int mpGetPrime(int x);
MATHPRESSO_NOAPI unsigned int mpGetHash(const char* _key, size_t klen);

template<typename T>
struct MATHPRESSO_NOAPI Hash {
  struct Node {
    Node* next;

    char* key;
    unsigned int klen;
    unsigned int hash;

    T value;
  };

  Hash() : _elements(0), _buckets(1), _grow(1), _shrink(0), _data(_embedded) { _data[0] = NULL; }
  ~Hash() { clear(); }

  void clear();
  bool copyFrom(const Hash<T>& other);
  bool mergeWith(const Hash<T>& other);

  bool put(const char* key, size_t klen, const T& value);
  T* get(const char* key, size_t klen);
  bool remove(const char* key, size_t klen);
  bool contains(const char* key, size_t klen) const;

  void grow();
  void shrink();
  void rehash(int count);

  static const char* dataToKey(const T* data) {
    const Node* node = reinterpret_cast<const Node*>(
      ((const char*)data) - MATHPRESSO_OFFSET(Node, value)
    );
    return node->key;
  }

  int _elements;
  int _buckets;

  int _grow;
  int _shrink;

  Node** _data;
  Node* _embedded[1];

private:
  // DISABLE COPY.
  MP_DISABLE_COPY(Hash<T>)
};

template<typename T>
void Hash<T>::clear() {
  int i;

  for (i = 0; i < _buckets; i++) {
    Node* node = _data[i];
    while (node) {
      Node* next = node->next;
      node->value.~T();
      ::free(node);
      node = next;
    }
  }

  _elements = 0;
  _buckets = 1;

  _grow = 1;
  _shrink = 0;

  if (_data != _embedded)
    free(_data);

  _data = _embedded;
  _data[0] = NULL;
}

template<typename T>
bool Hash<T>::copyFrom(const Hash<T>& other) {
  clear();
  return mergeWith(other);
}

template<typename T>
bool Hash<T>::mergeWith(const Hash<T>& other) {
  if (this == &other)
    return true;

  for (int i = 0; i < other._buckets; i++) {
    Node* node = other._data[i];
    while (node) {
      if (!put(node->key, node->klen, node->value)) return false;
      node = node->next;
    }
  }

  return true;
}

template<typename T>
bool Hash<T>::put(const char* key, size_t klen, const T& value) {
  unsigned int hash = mpGetHash(key, klen);
  unsigned int hmod = hash % _buckets;

  Node* node = _data[hmod];
  while (node) {
    if (node->klen == klen && memcmp(node->key, key, klen) == 0) {
      // Merge with existing.
      node->value = value;
      return true;
    }
    node = node->next;
  }

  // Add a new record.
  node = reinterpret_cast<Node*>(::malloc(sizeof(Node) + klen + 1));
  if (!node) return false;

  node->key = (char*)node + sizeof(Node);
  memcpy(node->key, key, klen);
  node->key[klen] = '\0';
  node->klen = static_cast<unsigned int >(klen);
  node->hash = hash;
  new(&node->value) T(value);

  node->next = _data[hmod];
  _data[hmod] = node;

  if (++_elements >= _grow) grow();
  return true;
}

template<typename T>
T* Hash<T>::get(const char* key, size_t klen) {
  unsigned int hash = mpGetHash(key, klen);
  unsigned int hmod = hash % _buckets;

  Node* node = _data[hmod];
  while (node) {
    if (node->klen == klen && memcmp(node->key, key, klen) == 0)
      return &node->value;
    node = node->next;
  }

  return NULL;
}

template<typename T>
bool Hash<T>::remove(const char* key, size_t klen) {
  unsigned int hash = mpGetHash(key, klen);
  unsigned int hmod = hash % _buckets;

  Node* node = _data[hmod];
  Node* prev = NULL;

  while (node) {
    Node* next = node->next;
    if (node->klen == klen && memcmp(node->key, key, klen) == 0) {
      // Remove.
      if (prev)
        prev->next = next;
      else
        _data[hmod] = next;

      node->value.~T();
      ::free(node);

      if (--_elements < _shrink) shrink();
      return true;
    }

    prev = node;
    node = next;
  }

  return false;
}

template<typename T>
bool Hash<T>::contains(const char* key, size_t klen) const {
  unsigned int hash = mpGetHash(key, klen);
  unsigned int hmod = hash % _buckets;

  Node* node = _data[hmod];
  while (node) {
    if (node->klen == klen && memcmp(node->key, key, klen) == 0)
      return true;
    node = node->next;
  }

  return false;
}

template<typename T>
void Hash<T>::grow() {
  int i = mpGetPrime(_grow);
  if (i != _buckets) rehash(i);
}

template<typename T>
void Hash<T>::shrink() {
  int i = mpGetPrime(_shrink);
  if (i != _buckets) rehash(i);
}

template<typename T>
void Hash<T>::rehash(int count) {
  int oldBuckets = _buckets;
  int newBuckets = count;

  Node** oldData = _data;
  Node** newData = reinterpret_cast<Node**>(::malloc(newBuckets * sizeof(void*)));

  if (newData == NULL) return;
  memset(newData, 0, newBuckets * sizeof(void*));

  for (int i = 0; i < oldBuckets; i++) {
    Node* node = _data[i];
    while (node) {
      Node* next = node->next;
      unsigned int hmod = node->hash % newBuckets;

      node->next = newData[hmod];
      newData[hmod] = node;

      node = next;
    }
  }

  _data = newData;
  _buckets = newBuckets;
  if (oldData != _embedded) ::free(oldData);

  _grow   = (int)( ((double)_buckets * 0.85) );
  _shrink = (int)( ((double)_buckets * 0.15) );
}

} // mathpresso namespace

#endif // _MATHPRESSO_UTIL_P_H
