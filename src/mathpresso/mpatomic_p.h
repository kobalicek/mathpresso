// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPATOMIC_P_H
#define _MATHPRESSO_MPATOMIC_P_H

// [Dependencies]
#include "./mathpresso_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::mpAtomic]
// ============================================================================

//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicGet(const uintptr_t* atomic) {
  return *(const uintptr_t volatile*)atomic;
}

//! \internal
static MATHPRESSO_INLINE void mpAtomicSet(uintptr_t* atomic, size_t value) {
  *(volatile uintptr_t*)atomic = value;
}

#if defined(_MSC_VER)
# if defined(__x86_64__) || defined(_WIN64) || defined(_M_IA64) || defined(_M_X64)
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicSetXchg(uintptr_t* atomic, uintptr_t value) {
  return _InterlockedExchange64((__int64 volatile *)atomic, static_cast<__int64>(value));
};
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicInc(uintptr_t* atomic) {
  return _InterlockedIncrement64((__int64 volatile *)atomic);
};
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicDec(uintptr_t* atomic) {
  return _InterlockedDecrement64((__int64 volatile *)atomic);
}
# else
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicSetXchg(uintptr_t* atomic, uintptr_t value) {
  return _InterlockedExchange((long volatile *)atomic, static_cast<long>(value));
};
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicInc(uintptr_t* atomic) {
  return _InterlockedIncrement((long volatile *)atomic);
}
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicDec(uintptr_t* atomic) {
  return _InterlockedDecrement((long volatile *)atomic);
}
# endif // _64BIT
#elif defined(__GNUC__) || defined(__clang__)
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicSetXchg(uintptr_t* atomic, uintptr_t value) {
  return __sync_lock_test_and_set(atomic, value);
};
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicInc(uintptr_t* atomic) {
  return __sync_add_and_fetch(atomic, 1);
}
//! \internal
static MATHPRESSO_INLINE uintptr_t mpAtomicDec(uintptr_t* atomic) {
  return __sync_sub_and_fetch(atomic, 1);
}
#endif // __GNUC__

template<typename T>
MATHPRESSO_INLINE T mpAtomicSetXchgT(T* atomic, T value) {
  return (T)mpAtomicSetXchg((uintptr_t *)atomic, (uintptr_t)value);
}

} // mathpresso namespace

// [Guard]
#endif // _MATHPRESSO_MPATOMIC_P_H
