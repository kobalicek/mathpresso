// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPEVAL_P_H
#define _MATHPRESSO_MPEVAL_P_H

// [Dependencies]
#include "./mathpresso_p.h"

namespace mathpresso {

// MathPresso - Evaluation
// =======================

//! DP-FP binary representation and utilities.
union DoubleBits {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  static MATHPRESSO_INLINE DoubleBits from_double(double val) { DoubleBits u; u.d = val; return u; }
  static MATHPRESSO_INLINE DoubleBits from_uint64(uint64_t val) { DoubleBits u; u.u = val; return u; }

  MATHPRESSO_INLINE void set_nan() { hi = 0x7FF80000u; lo = 0x00000000u; }
  MATHPRESSO_INLINE void set_inf() { hi = 0x7FF00000u; lo = 0x00000000u; }

  MATHPRESSO_INLINE bool is_nan() const { return (hi & 0x7FF00000u) == 0x7FF00000u && ((hi & 0x000FFFFFu) | lo) != 0x00000000u; }
  MATHPRESSO_INLINE bool is_inf() const { return (hi & 0x7FFFFFFFu) == 0x7FF00000u && lo == 0x00000000u; }
  MATHPRESSO_INLINE bool is_finite() const { return (hi & 0x7FF00000u) != 0x7FF00000u; }

  //! Value as uint64_t.
  uint64_t u;
  //! Value as `double`.
  double d;

#if MATHPRESSO_ARCH_LE
  struct { uint32_t lo; uint32_t hi; };
#else
  struct { uint32_t hi; uint32_t lo; };
#endif
};

// The `a != a` condition is used to handle NaN values properly. If one of `a`
// and `b` is NaN the result should be NaN. When `T` isn't a floating point the
// condition should be removed by the C++ compiler.
template<typename T> MATHPRESSO_INLINE T mp_min(T a, T b) { return ((a != a) | (a < b)) ? a : b; }
template<typename T> MATHPRESSO_INLINE T mp_max(T a, T b) { return ((a != a) | (a > b)) ? a : b; }

static MATHPRESSO_INLINE double mp_get_nan() { static const DoubleBits value = { 0x7FF8000000000000u }; return value.d; }
static MATHPRESSO_INLINE double mp_get_inf() { static const DoubleBits value = { 0x7FF0000000000000u }; return value.d; }
static MATHPRESSO_INLINE double mp_is_nan(double x) { return DoubleBits::from_double(x).is_nan(); }
static MATHPRESSO_INLINE double mp_is_inf(double x) { return DoubleBits::from_double(x).is_inf(); }
static MATHPRESSO_INLINE double mp_is_finite(double x) { return DoubleBits::from_double(x).is_finite(); }

static MATHPRESSO_INLINE double mp_round(double x) {
  double y = ::floor(x);
  return y + (x - y >= 0.5 ? double(1.0) : double(0.0));
}

static MATHPRESSO_INLINE double mp_round_even(double x) { return ::rint(x); }
static MATHPRESSO_INLINE double mp_trunc(double x) { return ::trunc(x); }
static MATHPRESSO_INLINE double mp_floor(double x) { return ::floor(x); }
static MATHPRESSO_INLINE double mp_ceil(double x) { return ::ceil(x); }

static MATHPRESSO_INLINE double mp_sign_bit(double x) { return DoubleBits::from_double(x).hi >= 0x80000000u; }
static MATHPRESSO_INLINE double mp_copy_sign(double x, double y) {
  DoubleBits bits = DoubleBits::from_double(x);
  bits.hi &= 0x7FFFFFFFu;
  bits.hi |= DoubleBits::from_double(y).hi & 0x80000000u;
  return bits.d;
}

static MATHPRESSO_INLINE double mp_avg(double x, double y) { return (x + y) * 0.5; }
static MATHPRESSO_INLINE double mp_mod(double x, double y) { return fmod(x, y); }
static MATHPRESSO_INLINE double mp_abs(double x) { return ::fabs(x); }
static MATHPRESSO_INLINE double mp_exp(double x) { return ::exp(x); }
static MATHPRESSO_INLINE double mp_pow(double x, double y) { return ::pow(x, y); }

static MATHPRESSO_INLINE double mp_log(double x) { return ::log(x); }
static MATHPRESSO_INLINE double mp_log2(double x) { return ::log2(x); }
static MATHPRESSO_INLINE double mp_log10(double x) { return ::log10(x); }

static MATHPRESSO_INLINE double mp_sqrt(double x) { return ::sqrt(x); }
static MATHPRESSO_INLINE double mp_frac(double x) { return x - mp_floor(x); }
static MATHPRESSO_INLINE double mp_recip(double x) { return 1.0 / x; }

static MATHPRESSO_INLINE double mp_sin(double x) { return ::sin(x); }
static MATHPRESSO_INLINE double mp_cos(double x) { return ::cos(x); }
static MATHPRESSO_INLINE double mp_tan(double x) { return ::tan(x); }

static MATHPRESSO_INLINE double mp_sinh(double x) { return ::sinh(x); }
static MATHPRESSO_INLINE double mp_cosh(double x) { return ::cosh(x); }
static MATHPRESSO_INLINE double mp_tanh(double x) { return ::tanh(x); }

static MATHPRESSO_INLINE double mp_asin(double x) { return ::asin(x); }
static MATHPRESSO_INLINE double mp_acos(double x) { return ::acos(x); }
static MATHPRESSO_INLINE double mp_atan(double x) { return ::atan(x); }

static MATHPRESSO_INLINE double mp_atan2(double y, double x) { return ::atan2(y, x); }
static MATHPRESSO_INLINE double mp_hypot(double x, double y) { return ::hypot(x, y); }

} // {mathpresso}

#endif // _MATHPRESSO_MPEVAL_P_H
