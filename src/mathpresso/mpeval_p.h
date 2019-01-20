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

// ============================================================================
// [mathpresso::Evaluation]
// ============================================================================

//! DP-FP binary representation and utilities.
union DoubleBits {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  static MATHPRESSO_INLINE DoubleBits fromDouble(double val) { DoubleBits u; u.d = val; return u; }
  static MATHPRESSO_INLINE DoubleBits fromUInt64(uint64_t val) { DoubleBits u; u.u = val; return u; }

  MATHPRESSO_INLINE void setNan() { hi = 0x7FF80000u; lo = 0x00000000u; }
  MATHPRESSO_INLINE void setInf() { hi = 0x7FF00000u; lo = 0x00000000u; }

  MATHPRESSO_INLINE bool isNan() const { return (hi & 0x7FF00000u) == 0x7FF00000u && ((hi & 0x000FFFFFu) | lo) != 0x00000000u; }
  MATHPRESSO_INLINE bool isInf() const { return (hi & 0x7FFFFFFFu) == 0x7FF00000u && lo == 0x00000000u; }
  MATHPRESSO_INLINE bool isFinite() const { return (hi & 0x7FF00000u) != 0x7FF00000u; }

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
// condition should be removed by C++ compiler.
template<typename T> MATHPRESSO_INLINE T mpMin(T a, T b) { return (a != a) | (a < b) ? a : b; }
template<typename T> MATHPRESSO_INLINE T mpMax(T a, T b) { return (a != a) | (a > b) ? a : b; }

static MATHPRESSO_INLINE double mpGetNan() { static const DoubleBits value = { 0x7FF8000000000000u }; return value.d; }
static MATHPRESSO_INLINE double mpGetInf() { static const DoubleBits value = { 0x7FF0000000000000u }; return value.d; }
static MATHPRESSO_INLINE bool mpIsNan(double x) { return DoubleBits::fromDouble(x).isNan(); }
static MATHPRESSO_INLINE bool mpIsInf(double x) { return DoubleBits::fromDouble(x).isInf(); }
static MATHPRESSO_INLINE bool mpIsFinite(double x) { return DoubleBits::fromDouble(x).isFinite(); }

static MATHPRESSO_INLINE double mpRound(double x) {
  double y = ::floor(x);
  return y + (x - y >= 0.5 ? double(1.0) : double(0.0));
}

static MATHPRESSO_INLINE double mpRoundEven(double x) { return ::rint(x); }
static MATHPRESSO_INLINE double mpTrunc(double x) { return ::trunc(x); }
static MATHPRESSO_INLINE double mpFloor(double x) { return ::floor(x); }
static MATHPRESSO_INLINE double mpCeil(double x) { return ::ceil(x); }

static MATHPRESSO_INLINE double mpSignBit(double x) { return DoubleBits::fromDouble(x).hi >= 0x80000000u; }
static MATHPRESSO_INLINE double mpCopySign(double x, double y) {
  DoubleBits bits = DoubleBits::fromDouble(x);
  bits.hi &= 0x7FFFFFFFu;
  bits.hi |= DoubleBits::fromDouble(y).hi & 0x80000000u;
  return bits.d;
}

static MATHPRESSO_INLINE double mpAvg(double x, double y) { return (x + y) * 0.5; }
static MATHPRESSO_INLINE double mpMod(double x, double y) { return fmod(x, y); }
static MATHPRESSO_INLINE double mpAbs(double x) { return ::fabs(x); }
static MATHPRESSO_INLINE double mpExp(double x) { return ::exp(x); }
static MATHPRESSO_INLINE double mpPow(double x, double y) { return ::pow(x, y); }

static MATHPRESSO_INLINE double mpLog(double x) { return ::log(x); }
static MATHPRESSO_INLINE double mpLog2(double x) { return ::log2(x); }
static MATHPRESSO_INLINE double mpLog10(double x) { return ::log10(x); }

static MATHPRESSO_INLINE double mpSqrt(double x) { return ::sqrt(x); }
static MATHPRESSO_INLINE double mpFrac(double x) { return x - mpFloor(x); }
static MATHPRESSO_INLINE double mpRecip(double x) { return 1.0 / x; }

static MATHPRESSO_INLINE double mpSin(double x) { return ::sin(x); }
static MATHPRESSO_INLINE double mpCos(double x) { return ::cos(x); }
static MATHPRESSO_INLINE double mpTan(double x) { return ::tan(x); }

static MATHPRESSO_INLINE double mpSinh(double x) { return ::sinh(x); }
static MATHPRESSO_INLINE double mpCosh(double x) { return ::cosh(x); }
static MATHPRESSO_INLINE double mpTanh(double x) { return ::tanh(x); }

static MATHPRESSO_INLINE double mpAsin(double x) { return ::asin(x); }
static MATHPRESSO_INLINE double mpAcos(double x) { return ::acos(x); }
static MATHPRESSO_INLINE double mpAtan(double x) { return ::atan(x); }

static MATHPRESSO_INLINE double mpAtan2(double y, double x) { return ::atan2(y, x); }
static MATHPRESSO_INLINE double mpHypot(double x, double y) { return ::hypot(x, y); }

} // mathpresso namespace

#endif // _MATHPRESSO_MPEVAL_P_H
