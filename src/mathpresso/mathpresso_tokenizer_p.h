// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _MATHPRESSO_TOKENIZER_P_H
#define _MATHPRESSO_TOKENIZER_P_H

#include "mathpresso.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [Constants]
// ============================================================================

//! \internal
//!
//! Token type.
enum MPTokenType {
  kMPTokenError,
  kMPTokenEOI,
  kMPTokenComma,
  kMPTokenSemicolon,
  kMPTokenLParen,
  kMPTokenRParen,
  kMPTokenOperator,
  kMPTokenInt,
  kMPTokenDouble,
  kMPTokenSymbol
};

// ============================================================================
// [mathpresso::MPToken]
// ============================================================================

struct MPToken {
  // Parser position from the beginning of buffer and token length.
  size_t pos;
  size_t len;

  // Token type.
  uint tokenType;

  // Token value.
  union {
    int operatorType;
    double value;
  };
};

// ============================================================================
// [mathpresso::MPTokenizer]
// ============================================================================

struct MPTokenizer {
  MPTokenizer(const char* input, size_t length);
  ~MPTokenizer();

  uint next(MPToken* dst);
  uint peek(MPToken* dst);
  void back(MPToken* to);

  const char* beg;
  const char* cur;
  const char* end;
};

} // mathpresso namespace

#endif // _MATHPRESSO_TOKENIZER_P_H
