// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MATHPRESSO_P_H
#define _MATHPRESSO_MATHPRESSO_P_H

// [Dependencies]
#include "./mathpresso.h"

#include <asmjit/core.h>

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>

#if defined(_MSC_VER)
  #include <windows.h>
  #include <intrin.h>
#endif

// MathPresso Likely & Unlikely
// ============================

#if defined(__GNUC__) || defined(__clang__)
# define MATHPRESSO_LIKELY(exp) __builtin_expect(!!(exp), 1)
# define MATHPRESSO_UNLIKELY(exp) __builtin_expect(!!(exp), 0)
#else
# define MATHPRESSO_LIKELY(exp) exp
# define MATHPRESSO_UNLIKELY(exp) exp
#endif

// MathPresso  Error Handling
// ==========================

//! \internal
#define MATHPRESSO_ASSERT(exp) do { \
  if (!(exp)) \
    ::mathpresso::assertion_failure(__FILE__, __LINE__, #exp); \
  } while (0)

//! \internal
#define MATHPRESSO_ASSERT_NOT_REACHED() do { \
    ::mathpresso::assertion_failure(__FILE__, __LINE__, "Not reached"); \
  } while (0)

//! \internal
#define MATHPRESSO_PROPAGATE(...) \
  do { \
    ::mathpresso::Error _errorValue = __VA_ARGS__; \
    if (MATHPRESSO_UNLIKELY(_errorValue != ::mathpresso::kErrorOk)) \
      return _errorValue; \
  } while (0)

//! \internal
#define MATHPRESSO_PROPAGATE_(exp, cleanup) \
  do { \
    ::mathpresso::Error _errorCode = (exp); \
    if (MATHPRESSO_UNLIKELY(_errorCode != ::mathpresso::kErrorOk)) { \
      cleanup \
      return _errorCode; \
    } \
  } while (0)

//! \internal
#define MATHPRESSO_NULLCHECK(ptr) \
  do { \
    if (MATHPRESSO_UNLIKELY(!(ptr))) \
      return MATHPRESSO_TRACE_ERROR(::mathpresso::kErrorNoMemory); \
  } while (0)

//! \internal
#define MATHPRESSO_NULLCHECK_(ptr, cleanup) \
  do { \
    if (MATHPRESSO_UNLIKELY(!(ptr))) { \
      cleanup \
      return MATHPRESSO_TRACE_ERROR(::mathpresso::kErrorNoMemory); \
    } \
  } while (0)

#define MATHPRESSO_TRACE_ERROR(error) \
  ::mathpresso::make_error(error)

namespace mathpresso {

// Reuse these classes - we depend on asmjit anyway and these are internal.
using asmjit::String;
using asmjit::StringTmp;

using asmjit::Arena;
using asmjit::ArenaVector;

// MathPresso OpType
// =================

//! \internal
//!
//! Operator type.
enum OpType {
  kOpNone = 0,

  kOpNeg,               // -a
  kOpNot,               // !a

  kOpIsNan,             // isnan(a)
  kOpIsInf,             // isinf(a)
  kOpIsFinite,          // isfinite(a)
  kOpSignBit,           // signbit(a)

  kOpTrunc,             // trunc(a)
  kOpFloor,             // floor(a)
  kOpCeil,              // ceil(a)
  kOpRoundEven,         // round_even(a)
  kOpRoundHalfAway,     // round_half_away(a)
  kOpRoundHalfUp,       // round_half_up(a)

  kOpAbs,               // abs(a)
  kOpExp,               // exp(a)
  kOpLog,               // log(a)
  kOpLog2,              // log2(a)
  kOpLog10,             // log10(a)
  kOpSqrt,              // sqrt(a)
  kOpFrac,              // frac(a)
  kOpRecip,             // recip(a)

  kOpSin,               // sin(a)
  kOpCos,               // cos(a)
  kOpTan,               // tan(a)
  kOpSinh,              // sinh(a)
  kOpCosh,              // cosh(a)
  kOpTanh,              // tanh(a)
  kOpAsin,              // asin(a)
  kOpAcos,              // acos(a)
  kOpAtan,              // atan(a)

  kOpAssign,            // a = b
  kOpEq,                // a == b
  kOpNe,                // a != b
  kOpLt,                // a <  b
  kOpLe,                // a <= b
  kOpGt,                // a >  b
  kOpGe,                // a >= b

  kOpAdd,               // a + b
  kOpSub,               // a - b
  kOpMul,               // a * b
  kOpDiv,               // a / b
  kOpMod,               // a % b

  kOpAvg,               // avg(a, b)
  kOpMin,               // min(a, b)
  kOpMax,               // max(a, b)
  kOpPow,               // pow(a, b)
  kOpAtan2,             // atan2(a, b)
  kOpHypot,             // hypot(a, b)
  kOpCopySign,          // copysign(a, b)

  //! \internal
  //!
  //! Count of operators.
  kOpCount
};

// MathPresso OpFlags
// ==================

//! Operator flags.
enum OpFlags {
  //! The operator has one parameter (unary node).
  kOpFlagUnary         = 0x00000001,
  //! The operator has two parameters (binary node).
  kOpFlagBinary        = 0x00000002,
  //! The operator is an intrinsic function.
  kOpFlagIntrinsic     = 0x00000004,
  //! The operator has right-to-left associativity.
  kOpFlagRightToLeft   = 0x00000008,

  //! The operator does an assignment to a variable.
  kOpFlagAssign        = 0x00000010,

  //! The operator performs an arithmetic operation.
  kOpFlagArithmetic    = 0x00000100,
  //! The operator performs a conditional operation.
  kOpFlagCondition     = 0x00000200,
  //! The operator performs a floating-point rounding.
  kOpFlagRounding      = 0x00000400,
  //! The operator calculates a trigonometric function.
  kOpFlagTrigonometric = 0x00000800,

  kOpFlagNopIfLZero    = 0x10000000,
  kOpFlagNopIfRZero    = 0x20000000,
  kOpFlagNopIfLOne     = 0x40000000,
  kOpFlagNopIfROne     = 0x80000000,

  kOpFlagNopIfZero     = kOpFlagNopIfLZero | kOpFlagNopIfRZero,
  kOpFlagNopIfOne      = kOpFlagNopIfLOne  | kOpFlagNopIfROne
};

// MathPresso Internal Consts
// ==========================

enum InternalConsts {
  kInvalidSlot = 0xFFFFFFFFu
};

// MathPresso Internal Options
// ===========================

//! \internal
//!
//! Compilation options MATHPRESSO uses internally.
enum InternalOptions {
  //! Set if `OutputLog` is present. MATHPRESSO then checks only this flag to use it.
  kInternalOptionLog = 0x00010000
};

// MathPresso - Assertions
// =======================

//! \internal
//!
//! MathPresso assertion handler.
MATHPRESSO_NOAPI void assertion_failure(const char* file, int line, const char* msg);

// MathPresso - Tracing
// ====================

MATHPRESSO_NOAPI Error make_error(Error error);

// MathPresso - OpInfo
// ===================

//! Operator information.
struct OpInfo {
  // Members
  // -------

  uint8_t type;
  uint8_t alt_type;
  uint8_t precedence;
  uint8_t reserved;
  uint32_t flags;
  char name[16];

  // Statics
  // -------

  static MATHPRESSO_INLINE const OpInfo& get(uint32_t op_type);

  // Accessors
  // ---------

  MATHPRESSO_INLINE bool is_unary() const { return (flags & kOpFlagUnary) != 0; }
  MATHPRESSO_INLINE bool is_binary() const { return (flags & kOpFlagBinary) != 0; }
  MATHPRESSO_INLINE uint32_t op_count() const { return 1 + ((flags & kOpFlagBinary) != 0); }

  MATHPRESSO_INLINE bool is_intrinsic() const { return (flags & kOpFlagIntrinsic) != 0; }

  MATHPRESSO_INLINE bool is_left_to_right() const { return (flags & kOpFlagRightToLeft) == 0; }
  MATHPRESSO_INLINE bool is_right_to_left() const { return (flags & kOpFlagRightToLeft) != 0; }

  MATHPRESSO_INLINE bool is_assignment() const { return (flags & kOpFlagAssign) != 0; }
  MATHPRESSO_INLINE bool is_arithmetic() const { return (flags & kOpFlagArithmetic) != 0; }
  MATHPRESSO_INLINE bool is_condition() const { return (flags & kOpFlagCondition) != 0; }

  MATHPRESSO_INLINE bool is_rounding() const { return (flags & kOpFlagRounding) != 0; }
  MATHPRESSO_INLINE bool is_trigonometric() const { return (flags & kOpFlagTrigonometric) != 0; }

  MATHPRESSO_INLINE bool right_associate(uint32_t right_precedence) const {
    return precedence > right_precedence ||
           (precedence == right_precedence && is_right_to_left());
  }
};
extern const OpInfo op_info_table[kOpCount];

MATHPRESSO_INLINE const OpInfo& OpInfo::get(uint32_t op) {
  MATHPRESSO_ASSERT(op < kOpCount);
  return op_info_table[op];
}

// MathPresso - StringRef
// ======================

//! String reference (pointer to string data data and size).
//!
//! \note MATHPRESSO always provides NULL terminated string with size known. On
//! the other hand MATHPRESSO doesn't require NULL terminated strings when passed
//! to MATHPRESSO APIs.
struct StringRef {
  // Members
  // -------

  const char* _data;
  size_t _size;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE StringRef()
    : _data(nullptr),
      _size(0) {}

  explicit MATHPRESSO_INLINE StringRef(const char* data)
    : _data(data),
      _size(::strlen(data)) {}

  MATHPRESSO_INLINE StringRef(const char* data, size_t size)
    : _data(data),
      _size(size) {}

  // Reset & Setup
  // -------------

  MATHPRESSO_INLINE void reset() {
    set(nullptr, 0);
  }

  MATHPRESSO_INLINE void set(const char* data) {
    set(data, ::strlen(data));
  }

  MATHPRESSO_INLINE void set(const char* data, size_t size) {
    _data = data;
    _size = size;
  }

  // Accessors
  // ---------

  //! Get the string data.
  MATHPRESSO_INLINE const char* data() const { return _data; }
  //! Get the string size.
  MATHPRESSO_INLINE size_t size() const { return _size; }

  // Equality
  // --------

  MATHPRESSO_INLINE bool eq(const char* s) const {
    const char* a = _data;
    const char* a_end = a + _size;

    while (a != a_end) {
      if (*a++ != *s++)
        return false;
    }

    return *s == '\0';
  }

  MATHPRESSO_INLINE bool eq(const char* s, size_t size) const {
    return size == _size && ::memcmp(_data, s, size) == 0;
  }
};

// MathPresso - Error Reporter
// ===========================

//! Error reporter.
struct ErrorReporter {
  // Members
  // -------

  const char* _body;
  size_t _size;

  uint32_t _options;
  OutputLog* _log;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE ErrorReporter(const char* body, size_t size, uint32_t options, OutputLog* log)
    : _body(body),
      _size(size),
      _options(options),
      _log(log) {

    // These should be handled by MATHPRESSO before the `ErrorReporter` is created.
    MATHPRESSO_ASSERT((log == nullptr && (_options & kInternalOptionLog) == 0) ||
                      (log != nullptr && (_options & kInternalOptionLog) != 0) );
  }

  // Interface
  // ---------

  MATHPRESSO_INLINE bool does_report_errors() const { return (_options & kInternalOptionLog) != 0; }
  MATHPRESSO_INLINE bool does_report_warnings() const { return (_options & kOptionVerbose) != 0; }

  void get_line_and_column(uint32_t position, uint32_t& line, uint32_t& column);

  void on_warning(uint32_t position, const char* fmt, ...);
  void on_warning(uint32_t position, const String& msg);

  Error on_error(Error error, uint32_t position, const char* fmt, ...);
  Error on_error(Error error, uint32_t position, const String& msg);
};

} // {mathpresso}

#endif // _MATHPRESSO_MATHPRESSO_P_H
