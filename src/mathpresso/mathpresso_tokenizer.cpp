// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_EXPORTS

// [Dependencies]
#include "mathpresso.h"
#include "mathpresso_ast_p.h"
#include "mathpresso_tokenizer_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::MPTokenizer]
// ============================================================================

MPTokenizer::MPTokenizer(const char* input, size_t length) {
  beg = input;
  cur = input;
  end = input + length;
}
MPTokenizer::~MPTokenizer() {}

uint MPTokenizer::next(MPToken* dst) {
  // Skip spaces.
  while (cur != end && mpIsSpace(*cur)) *cur++;

  // End of input.
  if (cur == end)
  {
    dst->pos = (size_t)(cur - beg);
    dst->len = 0;
    dst->tokenType = kMPTokenEOI;
    return kMPTokenEOI;
  }

  // Save first input character for error handling and postparsing.
  const char* first = cur;

  // Type of token is usually determined by first input character.
  uint uc = *cur;

  // Parse float.
  if (mpIsDigit(uc)) {
    uint t = kMPTokenInt;

    // Parse digit part.
    while (++cur != end) {
      uc = *cur;
      if (!mpIsDigit(uc)) break;
    }

    // Parse dot.
    if (cur != end && uc == '.') {
      while (++cur != end) {
        uc = *cur;
        if (!mpIsDigit(uc)) break;
        t = kMPTokenDouble;
      }
    }

    dst->pos = (size_t)(first - beg);
    dst->len = (size_t)(cur - first);

    if (mpIsAlpha(uc)) goto error;

    // Convert string to float.
    bool ok;
    double n = mpConvertToDouble(first, (size_t)(cur - first), &ok);
    if (!ok) goto error;

    dst->tokenType = t;
    dst->value = n;
    return t;
  }

  // Parse symbol.
  else if (mpIsAlpha(uc) || uc == '_') {
    while (++cur != end) {
      uc = *cur;
      if (!(mpIsAlnum(uc) || uc == '_')) break;
    }

    dst->pos = (size_t)(first - beg);
    dst->len = (size_t)(cur - first);

    dst->tokenType = kMPTokenSymbol;
    return kMPTokenSymbol;
  }

  // Parse operators, parenthesis, etc...
  else {
    cur++;

    dst->pos = (size_t)(first - beg);
    dst->len = (size_t)(cur - first);

    switch (uc) {
      case ',': dst->tokenType = kMPTokenComma; break;
      case '(': dst->tokenType = kMPTokenLParen; break;
      case ')': dst->tokenType = kMPTokenRParen; break;
      case ';': dst->tokenType = kMPTokenSemicolon; break;
      case '=': dst->tokenType = kMPTokenOperator; dst->operatorType = kMPBinaryOpAssign; break;
      case '+': dst->tokenType = kMPTokenOperator; dst->operatorType = kMPBinaryOpAdd; break;
      case '-': dst->tokenType = kMPTokenOperator; dst->operatorType = kMPBinaryOpSub; break;
      case '*': dst->tokenType = kMPTokenOperator; dst->operatorType = kMPBinaryOpMul; break;
      case '/': dst->tokenType = kMPTokenOperator; dst->operatorType = kMPBinaryOpDiv; break;
      case '%': dst->tokenType = kMPTokenOperator; dst->operatorType = kMPBinaryOpMod; break;
      default : dst->tokenType = kMPTokenError; break;
    }

    return dst->tokenType;
  }

error:
  dst->tokenType = kMPTokenError;
  cur = first;
  return kMPTokenError;
}

uint MPTokenizer::peek(MPToken* to) {
  next(to);
  back(to);
  return to->tokenType;
}

void MPTokenizer::back(MPToken* to) {
  cur = beg + to->pos;
}

} // mathpresso namespace
