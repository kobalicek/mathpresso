// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPTOKENIZER_P_H
#define _MATHPRESSO_MPTOKENIZER_P_H

// [Dependencies]
#include "./mathpresso_p.h"
#include "./mpstrtod_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::Token]
// ============================================================================

//! \internal
//!
//! Token type.
enum TokenType {
  kTokenInvalid = 0,  // <invalid>

  kTokenSymbol,       // <symbol>
  kTokenNumber,       // <number>

  kTokenVar,          // 'var' keyword
  kTokenReserved,     // reserved keyword

  kTokenDot = 36,     // .
  kTokenComma,        // ,
  kTokenSemicolon,    // ;

  kTokenQMark,        // ?
  kTokenColon,        // :

  kTokenLCurl,        // {
  kTokenRCurl,        // }

  kTokenLBracket,     // [
  kTokenRBracket,     // ]

  kTokenLParen,       // (
  kTokenRParen,       // )

  kTokenAdd,          // +
  kTokenSub,          // -
  kTokenMul,          // *
  kTokenDiv,          // /
  kTokenMod,          // %
  kTokenNot,          // !

  kTokenBitAnd,       // &
  kTokenBitOr,        // |
  kTokenBitXor,       // ^
  kTokenBitNeg,       // ~

  kTokenAssign,       // =
  kTokenLt,           // <
  kTokenGt,           // >

  kTokenPlusPlus,     // ++
  kTokenMinusMinus,   // --

  kTokenEq,           // ==
  kTokenNe,           // !=
  kTokenLe,           // <=
  kTokenGe,           // >=

  kTokenLogAnd,       // &&
  kTokenLogOr,        // ||

  kTokenBitSar,       // >>
  kTokenBitShr,       // >>>
  kTokenBitShl,       // <<

  kTokenAssignAdd,    // +=
  kTokenAssignSub,    // -=
  kTokenAssignMul,    // *=
  kTokenAssignDiv,    // /=
  kTokenAssignMod,    // %=

  kTokenAssignBitAnd, // &=
  kTokenAssignBitOr,  // |=
  kTokenAssignBitXor, // ^=
  kTokenAssignBitSar, // >>=
  kTokenAssignBitShr, // >>>=
  kTokenAssignBitShl, // <<=

  kTokenEnd           // <end>
};

// ============================================================================
// [mathpresso::Token]
// ============================================================================

//! \internal
//!
//! Token.
struct Token {
  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE void reset() {
    position = 0;
    length = 0;
    value = 0.0;
    token = kTokenInvalid;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE uint32_t setData(size_t position, size_t length, uint32_t hVal, uint32_t token) {
    this->position = position;
    this->length = length;
    this->hVal = hVal;
    this->token = token;

    return token;
  }

  MATHPRESSO_INLINE uint32_t getPosAsUInt() const {
    MATHPRESSO_ASSERT(position < ~static_cast<uint32_t>(0));
    return static_cast<uint32_t>(position);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Token position from the beginning of the input.
  size_t position;
  //! Token string length.
  size_t length;

  //! Token hash (only if the token is symbol or keyword).
  uint32_t hVal;
  //! Token type.
  uint32_t token;

  //! Token value (if the token is a number).
  double value;
};

// ============================================================================
// [mathpresso::Tokenizer]
// ============================================================================

struct Tokenizer {
  MATHPRESSO_NO_COPY(Tokenizer)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE Tokenizer(const char* s, size_t sLen)
    : _p(s),
      _start(s),
      _end(s + sLen),
      _strtod() {
    _token.reset();
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Get the current token.
  uint32_t peek(Token* token);
  //! Get the current token and advance.
  uint32_t next(Token* token);

  //! Set the token that will be returned by `next()` and `peek()` functions.
  MATHPRESSO_INLINE void set(Token* token) {
    // We have to update also _p in case that multiple tokens were put back.
    _p = _start + token->position + token->length;
    _token = *token;
  }

  //! Consume a token got by using peek().
  MATHPRESSO_INLINE void consume() {
    _token.token = kTokenInvalid;
  }

  //! Consume a token got by using peek() and call `peek()`.
  //!
  //! NOTE: Can be called only immediately after peek().
  MATHPRESSO_INLINE uint32_t consumeAndPeek(Token* token) {
    consume();
    return peek(token);
  }

  //! Consume a token got by using peek() and call `next()`.
  //!
  //! NOTE: Can be called only immediately after peek().
  MATHPRESSO_INLINE uint32_t consumeAndNext(Token* token) {
    consume();
    return next(token);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const char* _p;
  const char* _start;
  const char* _end;

  StrToD _strtod;
  Token _token;
};

} // mathpresso namespace

// [Guard]
#endif // _MATHPRESSO_MPTOKENIZER_P_H
