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
    _position = 0;
    _size = 0;
    _value = 0.0;
    _tokenType = kTokenInvalid;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline uint32_t setData(size_t position, size_t size, uint32_t hashCode, uint32_t tokenType) {
    _position = position;
    _size = size;
    _hashCode = hashCode;
    _tokenType = tokenType;

    return tokenType;
  }

  inline uint32_t tokenType() const noexcept { return _tokenType; }
  inline uint32_t hashCode() const noexcept { return _hashCode; }

  inline size_t position() const noexcept { return _position; }
  inline size_t size() const noexcept { return _size; }

  inline uint32_t positionAsUInt() const noexcept {
    MATHPRESSO_ASSERT(_position < ~static_cast<uint32_t>(0));
    return static_cast<uint32_t>(_position);
  }

  inline double value() const noexcept { return _value; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _tokenType;                   //!< Token type.
  uint32_t _hashCode;                    //!< Token hash-code (only if the token is symbol or keyword).
  size_t _position;                      //!< Token position from the beginning of the input.
  size_t _size;                          //!< Token size.
  double _value;                         //!< Token value (if the token is a number).
};

// ============================================================================
// [mathpresso::Tokenizer]
// ============================================================================

struct Tokenizer {
  MATHPRESSO_NONCOPYABLE(Tokenizer)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE Tokenizer(const char* str, size_t size)
    : _p(str),
      _start(str),
      _end(str + size),
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
    _p = _start + token->_position + token->_size;
    _token = *token;
  }

  //! Consume a token got by using peek().
  MATHPRESSO_INLINE void consume() {
    _token._tokenType = kTokenInvalid;
  }

  //! Consume a token got by using peek() and call `peek()`.
  //!
  //! \note Can be called only immediately after peek().
  MATHPRESSO_INLINE uint32_t consumeAndPeek(Token* token) {
    consume();
    return peek(token);
  }

  //! Consume a token got by using peek() and call `next()`.
  //!
  //! \note Can be called only immediately after peek().
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
