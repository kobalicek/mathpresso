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
#include <asmjit/asmjit.h>

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
# if _MSC_VER >= 1400
#  include <intrin.h>
# endif // _MSC_VER >= 1400
#endif // _MSC_VER

// ============================================================================
// [mathpresso::Architecture]
// ============================================================================

#if (defined(_M_X64  ) || defined(__x86_64) || defined(__x86_64__) || \
     defined(_M_AMD64) || defined(__amd64 ) || defined(__amd64__ ))
# define MATHPRESSO_ARCH_X64          (1)
#else
# define MATHPRESSO_ARCH_X64          (0)
#endif
#if (defined(_M_IX86 ) || defined(__X86__ ) || defined(__i386  ) || \
     defined(__IA32__) || defined(__I86__ ) || defined(__i386__) || \
     defined(__i486__) || defined(__i586__) || defined(__i686__))
# define MATHPRESSO_ARCH_X86          (!MATHPRESSO_ARCH_X64)
#else
# define MATHPRESSO_ARCH_X86          (0)
#endif

#if (defined(_M_ARM  ) || defined(__arm__ ) || defined(__arm) || \
     defined(_M_ARMT ) || defined(__thumb__))
# define MATHPRESSO_ARCH_ARM          (1)
# define MATHPRESSO_ARCH_ARM64        (0)
#else
# define MATHPRESSO_ARCH_ARM          (0)
# define MATHPRESSO_ARCH_ARM64        (0)
#endif

#if MATHPRESSO_ARCH_X86 || MATHPRESSO_ARCH_X64
# define MATHPRESSO_ARCH_64BIT        (MATHPRESSO_ARCH_X64)
# define MATHPRESSO_ARCH_BE           (0)
# define MATHPRESSO_ARCH_LE           (1)
#endif

#if MATHPRESSO_ARCH_ARM || MATHPRESSO_ARCH_ARM64
# define MATHPRESSO_ARCH_64BIT        (MATHPRESSO_ARCH_ARM64)
# define MATHPRESSO_ARCH_BE           (0)
# define MATHPRESSO_ARCH_LE           (1)
#endif

// ============================================================================
// [MathPresso::Likely / Unlikely]
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
# define MATHPRESSO_LIKELY(exp) __builtin_expect(!!(exp), 1)
# define MATHPRESSO_UNLIKELY(exp) __builtin_expect(!!(exp), 0)
#else
# define MATHPRESSO_LIKELY(exp) exp
# define MATHPRESSO_UNLIKELY(exp) exp
#endif

// ============================================================================
// [MathPresso::Error Handling]
// ============================================================================

//! \internal
#define MATHPRESSO_ASSERT(exp) do { \
  if (!(exp)) \
    ::mathpresso::mpAssertionFailure(__FILE__, __LINE__, #exp); \
  } while (0)

//! \internal
#define MATHPRESSO_ASSERT_NOT_REACHED() do { \
    ::mathpresso::mpAssertionFailure(__FILE__, __LINE__, "Not reached"); \
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
  ::mathpresso::mpTraceError(error)

// ============================================================================
// [mathpresso::]
// ============================================================================

namespace mathpresso {

// ============================================================================
// [Reuse]
// ============================================================================

// Reuse these classes - we depend on asmjit anyway and these are internal.
using asmjit::String;
using asmjit::StringTmp;

using asmjit::Zone;
using asmjit::ZoneVector;
using asmjit::ZoneAllocator;

// ============================================================================
// [mathpresso::OpType]
// ============================================================================

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

  kOpRound,             // round(a)
  kOpRoundEven,         // roundeven(a)
  kOpTrunc,             // trunc(a)
  kOpFloor,             // floor(a)
  kOpCeil,              // ceil(a)

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

// ============================================================================
// [mathpresso::OpFlags]
// ============================================================================

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

// ============================================================================
// [mpsl::InternalConsts]
// ============================================================================

enum InternalConsts {
  kInvalidSlot = 0xFFFFFFFFu
};

// ============================================================================
// [mpsl::InternalOptions]
// ============================================================================

//! \internal
//!
//! Compilation options MATHPRESSO uses internally.
enum InternalOptions {
  //! Set if `OutputLog` is present. MATHPRESSO then checks only this flag to use it.
  kInternalOptionLog = 0x00010000
};

// ============================================================================
// [mathpresso::mpAssertionFailure]
// ============================================================================

//! \internal
//!
//! MathPresso assertion handler.
MATHPRESSO_NOAPI void mpAssertionFailure(const char* file, int line, const char* msg);

// ============================================================================
// [mathpresso::mpTraceError]
// ============================================================================

MATHPRESSO_NOAPI Error mpTraceError(Error error);

// ============================================================================
// [mpsl::OpInfo]
// ============================================================================

//! Operator information.
struct OpInfo {
  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static MATHPRESSO_INLINE const OpInfo& get(uint32_t opType);

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE bool isUnary() const { return (flags & kOpFlagUnary) != 0; }
  MATHPRESSO_INLINE bool isBinary() const { return (flags & kOpFlagBinary) != 0; }
  MATHPRESSO_INLINE uint32_t opCount() const { return 1 + ((flags & kOpFlagBinary) != 0); }

  MATHPRESSO_INLINE bool isIntrinsic() const { return (flags & kOpFlagIntrinsic) != 0; }

  MATHPRESSO_INLINE bool isLeftToRight() const { return (flags & kOpFlagRightToLeft) == 0; }
  MATHPRESSO_INLINE bool isRightToLeft() const { return (flags & kOpFlagRightToLeft) != 0; }

  MATHPRESSO_INLINE bool isAssignment() const { return (flags & kOpFlagAssign) != 0; }
  MATHPRESSO_INLINE bool isArithmetic() const { return (flags & kOpFlagArithmetic) != 0; }
  MATHPRESSO_INLINE bool isCondition() const { return (flags & kOpFlagCondition) != 0; }

  MATHPRESSO_INLINE bool isRounding() const { return (flags & kOpFlagRounding) != 0; }
  MATHPRESSO_INLINE bool isTrigonometric() const { return (flags & kOpFlagTrigonometric) != 0; }

  MATHPRESSO_INLINE bool rightAssociate(uint32_t rPrec) const {
    return precedence > rPrec || (precedence == rPrec && isRightToLeft());
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t type;
  uint8_t altType;
  uint8_t precedence;
  uint8_t reserved;
  uint32_t flags;
  char name[12];
};
extern const OpInfo mpOpInfo[kOpCount];

MATHPRESSO_INLINE const OpInfo& OpInfo::get(uint32_t op) {
  MATHPRESSO_ASSERT(op < kOpCount);
  return mpOpInfo[op];
}

// ============================================================================
// [mathpresso::StringRef]
// ============================================================================

//! String reference (pointer to string data data and size).
//!
//! \note MATHPRESSO always provides NULL terminated string with size known. On
//! the other hand MATHPRESSO doesn't require NULL terminated strings when passed
//! to MATHPRESSO APIs.
struct StringRef {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE StringRef()
    : _data(NULL),
      _size(0) {}

  explicit MATHPRESSO_INLINE StringRef(const char* data)
    : _data(data),
      _size(::strlen(data)) {}

  MATHPRESSO_INLINE StringRef(const char* data, size_t size)
    : _data(data),
      _size(size) {}

  // --------------------------------------------------------------------------
  // [Reset / Setup]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE void reset() {
    set(NULL, 0);
  }

  MATHPRESSO_INLINE void set(const char* data) {
    set(data, ::strlen(data));
  }

  MATHPRESSO_INLINE void set(const char* data, size_t size) {
    _data = data;
    _size = size;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the string data.
  MATHPRESSO_INLINE const char* data() const { return _data; }
  //! Get the string size.
  MATHPRESSO_INLINE size_t size() const { return _size; }

  // --------------------------------------------------------------------------
  // [Eq]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE bool eq(const char* s) const {
    const char* a = _data;
    const char* aEnd = a + _size;

    while (a != aEnd) {
      if (*a++ != *s++)
        return false;
    }

    return *s == '\0';
  }

  MATHPRESSO_INLINE bool eq(const char* s, size_t size) const {
    return size == _size && ::memcmp(_data, s, size) == 0;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const char* _data;
  size_t _size;
};

// ============================================================================
// [mpsl::ErrorReporter]
// ============================================================================

//! Error reporter.
struct ErrorReporter {
  MATHPRESSO_INLINE ErrorReporter(const char* body, size_t size, uint32_t options, OutputLog* log)
    : _body(body),
      _size(size),
      _options(options),
      _log(log) {

    // These should be handled by MATHPRESSO before the `ErrorReporter` is created.
    MATHPRESSO_ASSERT((log == NULL && (_options & kInternalOptionLog) == 0) ||
                      (log != NULL && (_options & kInternalOptionLog) != 0) );
  }

  // --------------------------------------------------------------------------
  // [Error Handling]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE bool reportsErrors() const { return (_options & kInternalOptionLog) != 0; }
  MATHPRESSO_INLINE bool reportsWarnings() const { return (_options & kOptionVerbose) != 0; }

  void getLineAndColumn(uint32_t position, uint32_t& line, uint32_t& column);

  void onWarning(uint32_t position, const char* fmt, ...);
  void onWarning(uint32_t position, const String& msg);

  Error onError(Error error, uint32_t position, const char* fmt, ...);
  Error onError(Error error, uint32_t position, const String& msg);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const char* _body;
  size_t _size;

  uint32_t _options;
  OutputLog* _log;
};

} // mathpresso namespace

#endif // _MATHPRESSO_MATHPRESSO_P_H
