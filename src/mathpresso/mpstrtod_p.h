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

// `mathpresso_p.h` includes asmjit, so we can use `ASMJIT_OS_...`.
#if ASMJIT_OS_WINDOWS
# define MATHPRESSO_STRTOD_MSLOCALE
# include <locale.h>
#else
# define MATHPRESSO_STRTOD_XLOCALE
# include <locale.h>
# include <xlocale.h>
#endif

namespace mathpresso {

// ============================================================================
// [mathpresso::Parser]
// ============================================================================

struct StrToD {
  MATHPRESSO_NO_COPY(StrToD)

#if defined(MATHPRESSO_STRTOD_MSLOCALE)
  MATHPRESSO_INLINE StrToD() { handle = _create_locale(LC_ALL, "C"); }
  MATHPRESSO_INLINE ~StrToD() { _free_locale(handle); }

  MATHPRESSO_INLINE bool isOk() const { return handle != NULL; }
  MATHPRESSO_INLINE double conv(const char* s, char** end) const { return _strtod_l(s, end, handle); }

  _locale_t handle;
#elif defined(MATHPRESSO_STRTOD_XLOCALE)
  MATHPRESSO_INLINE StrToD() { handle = newlocale(LC_ALL_MASK, "C", NULL); }
  MATHPRESSO_INLINE ~StrToD() { freelocale(handle); }

  MATHPRESSO_INLINE bool isOk() const { return handle != NULL; }
  MATHPRESSO_INLINE double conv(const char* s, char** end) const { return strtod_l(s, end, handle); }

  locale_t handle;
#else
  // Time bomb!
  MATHPRESSO_INLINE bool isOk() const { return true; }
  MATHPRESSO_INLINE double conv(const char* s, char** end) const { return strtod(s, end); }
#endif
};

} // mathpresso namespace

// [Guard]
#endif // _MATHPRESSO_MPSTRTOD_P_H
