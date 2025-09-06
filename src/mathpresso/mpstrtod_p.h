// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPSTRTOD_P_H
#define _MATHPRESSO_MPSTRTOD_P_H

// [Dependencies]
#include "./mathpresso_p.h"

#if defined(_WIN32)
  #define MATHPRESSO_STRTOD_MSLOCALE
  #include <locale.h>
  #include <stdlib.h>
#elif !defined(__OpenBSD__)
  #define MATHPRESSO_STRTOD_XLOCALE
  #include <locale.h>
  #include <stdlib.h>
  // xlocale.h is not available on Linux anymore, it uses <locale.h>.
  #if defined(__APPLE__) || defined(__FreeBSD__)
    #include <xlocale.h>
  #endif
#endif

namespace mathpresso {

// MathPresso - String to Double
// =============================

struct StrToD {
  MATHPRESSO_NONCOPYABLE(StrToD)

#if defined(MATHPRESSO_STRTOD_MSLOCALE)
  MATHPRESSO_INLINE StrToD() { handle = _create_locale(LC_ALL, "C"); }
  MATHPRESSO_INLINE ~StrToD() { _free_locale(handle); }

  MATHPRESSO_INLINE bool isOk() const { return handle != nullptr; }
  MATHPRESSO_INLINE double conv(const char* s, char** end) const { return _strtod_l(s, end, handle); }

  _locale_t handle;
#elif defined(MATHPRESSO_STRTOD_XLOCALE)
  MATHPRESSO_INLINE StrToD() { handle = newlocale(LC_ALL_MASK, "C", nullptr); }
  MATHPRESSO_INLINE ~StrToD() { freelocale(handle); }

  MATHPRESSO_INLINE bool isOk() const { return handle != nullptr; }
  MATHPRESSO_INLINE double conv(const char* s, char** end) const { return strtod_l(s, end, handle); }

  locale_t handle;
#else
  // Time bomb!
  MATHPRESSO_INLINE StrToD() {}
  MATHPRESSO_INLINE ~StrToD() {}

  MATHPRESSO_INLINE bool isOk() const { return true; }
  MATHPRESSO_INLINE double conv(const char* s, char** end) const { return strtod(s, end); }
#endif
};

} // {mathpresso}

// [Guard]
#endif // _MATHPRESSO_MPSTRTOD_P_H
