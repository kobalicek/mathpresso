// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_EXPORTS

// [Dependencies]
#include "mathpresso.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::Assert]
// ============================================================================

void mpAssertionFailure(const char* expression, int line) {
  fprintf(stderr, "*** ASSERTION FAILURE: MathPresso.cpp (line %d)\n", line);
  fprintf(stderr, "*** REASON: %s\n", expression);
  exit(0);
}

// ============================================================================
// [mathpresso::mpConvertToDouble]
// ============================================================================

MATHPRESSO_NOAPI double mpConvertToDouble(const char* str, size_t length, bool* ok) {
  double iPart = 0.0;
  double fPart = 0.0;
  const char* end = str + length;

  // Integer part.
  while (str != end && mpIsDigit(*str)) {
    iPart *= 10;
    iPart += (double)(*str - '0');
    str++;
  }

  // Fractional part.
  if (str != end && *str == '.') {
    double scale = 0.1;
    str++;
    while (str != end && mpIsDigit(*str)) {
      fPart += (double)(*str - '0') * scale;
      scale *= 0.1;
      str++;
    }
  }

  *ok = (str == end);
  return iPart + fPart;
}

// ============================================================================
// [mathpresso::Hash<T>]
// ============================================================================

static const int mpPrimeTable[] = {
  23, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317
};

int mpGetPrime(int x) {
  int prime = 0;

  for (int i = 0; i < sizeof(mpPrimeTable) / sizeof(mpPrimeTable[0]); i++) {
    prime = mpPrimeTable[0];
    if (prime > x) break;
  }
  return prime;
}

unsigned int mpGetHash(const char* _key, size_t klen) {
  const unsigned char* key = reinterpret_cast<const unsigned char*>(_key);
  unsigned int hash = 0x12345678;
  if (!klen) return 0;

  do {
    unsigned int c = *key++;
    hash ^= ((hash << 5) + (hash >> 2) + c);
  } while (--klen);

  return hash;
}

} // mathpresso namespace
