// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mpeval_p.h"
#include "./mphash_p.h"
#include "./mptokenizer_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::Tokenizer]
// ============================================================================

//! \internal
//!
//! Character classes used by tokenizer.
enum TokenChar {
  // Digit.
  kTokenChar0x0, kTokenChar0x1, kTokenChar0x2, kTokenChar0x3,
  kTokenChar0x4, kTokenChar0x5, kTokenChar0x6, kTokenChar0x7,
  kTokenChar0x8, kTokenChar0x9,

  // Digit-Hex.
  kTokenChar0xA, kTokenChar0xB, kTokenChar0xC, kTokenChar0xD,
  kTokenChar0xE, kTokenChar0xF,

  // Non-Hex ASCII [A-Z] Letter and Underscore [_].
  kTokenCharSym,

  // Punctuation.
  kTokenCharDot = kTokenDot,          // .
  kTokenCharCom = kTokenComma,        // ,
  kTokenCharSem = kTokenSemicolon,    // ;
  kTokenCharQue = kTokenQMark,        // ?
  kTokenCharCol = kTokenColon,        // :
  kTokenCharLCu = kTokenLCurl,        // {
  kTokenCharRCu = kTokenRCurl,        // }
  kTokenCharLBr = kTokenLBracket,     // [
  kTokenCharRBr = kTokenRBracket,     // ]
  kTokenCharLPa = kTokenLParen,       // (
  kTokenCharRPa = kTokenRParen,       // )

  kTokenCharAdd = kTokenAdd,          // +
  kTokenCharSub = kTokenSub,          // -
  kTokenCharMul = kTokenMul,          // *
  kTokenCharDiv = kTokenDiv,          // /
  kTokenCharMod = kTokenMod,          // %
  kTokenCharNot = kTokenNot,          // !
  kTokenCharAnd = kTokenBitAnd,       // &
  kTokenCharOr  = kTokenBitOr,        // |
  kTokenCharXor = kTokenBitXor,       // ^
  kTokenCharNeg = kTokenBitNeg,       // ~
  kTokenCharEq  = kTokenAssign,       // =
  kTokenCharLt  = kTokenLt,           // <
  kTokenCharGt  = kTokenGt,           // >

  // Space.
  kTokenCharSpc = 63,

  // Extended ASCII character (0x80 and above), acts as non-recognized.
  kTokenCharExt,
  // Invalid (non-recognized) character.
  kTokenCharInv,

  kTokenCharSingleCharTokenEnd = kTokenCharRPa
};

#define C(_Id_) kTokenChar##_Id_
static const uint8_t mpCharClass[] = {
  C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), // 000-007 ........ | All invalid.
  C(Inv), C(Spc), C(Spc), C(Spc), C(Spc), C(Spc), C(Inv), C(Inv), // 008-015 .     .. | Spaces: 0x9-0xD.
  C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), // 016-023 ........ | All invalid.
  C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), C(Inv), // 024-031 ........ | All invalid.
  C(Spc), C(Not), C(Inv), C(Inv), C(Inv), C(Mod), C(And), C(Inv), // 032-039  !"#$%&' | Unassigned: "#$'.
  C(LPa), C(RPa), C(Mul), C(Add), C(Com), C(Sub), C(Dot), C(Div), // 040-047 ()*+,-./ |
  C(0x0), C(0x1), C(0x2), C(0x3), C(0x4), C(0x5), C(0x6), C(0x7), // 048-055 01234567 |
  C(0x8), C(0x9), C(Col), C(Sem), C(Lt ), C(Eq ), C(Gt ), C(Que), // 056-063 89:;<=>? |
  C(Inv), C(0xA), C(0xB), C(0xC), C(0xD), C(0xE), C(0xF), C(Sym), // 064-071 @ABCDEFG | Unassigned: @.
  C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), // 072-079 HIJKLMNO |
  C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), // 080-087 PQRSTUVW |
  C(Sym), C(Sym), C(Sym), C(LBr), C(Inv), C(RBr), C(Xor), C(Sym), // 088-095 XYZ[\]^_ | Unassigned: \.
  C(Inv), C(0xA), C(0xB), C(0xC), C(0xD), C(0xE), C(0xF), C(Sym), // 096-103 `abcdefg | Unassigned: `.
  C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), // 104-111 hijklmno |
  C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), C(Sym), // 112-119 pqrstuvw |
  C(Sym), C(Sym), C(Sym), C(LCu), C(Or ), C(RCu), C(Neg), C(Inv), // 120-127 xyz{|}~  |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 128-135 ........ | Extended.
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 136-143 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 144-151 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 152-159 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 160-167 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 168-175 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 176-183 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 184-191 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 192-199 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 200-207 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 208-215 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 216-223 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 224-231 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 232-239 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), // 240-247 ........ |
  C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext), C(Ext)  // 248-255 ........ |
};
#undef C

//! \internal
//!
//! RAW lowercase conversion.
//!
//! This method exploits how ASCII table has been designed. It expects ASCII
//! character on the input that will be lowercased by setting the 0x20 bit on.
static MATHPRESSO_INLINE uint32_t mpGetLower(uint32_t c) { return c | 0x20; }

//! \internal
static const double mpPow10Table[] = {
  1e+0 , 1e+1 , 1e+2 , 1e+3 , 1e+4 , 1e+5 , 1e+6 , 1e+7 ,
  1e+8 , 1e+9 , 1e+10, 1e+11, 1e+12, 1e+13, 1e+14, 1e+15
};

//! \internal
enum {
  kSafeDigits = 15,
  kPow10TableSize = static_cast<int>(MATHPRESSO_ARRAY_SIZE(mpPow10Table))
};

#define CHAR4X(C0, C1, C2, C3) \
  ( (static_cast<uint32_t>(C0)      ) + \
    (static_cast<uint32_t>(C1) <<  8) + \
    (static_cast<uint32_t>(C2) << 16) + \
    (static_cast<uint32_t>(C3) << 24) )

//! \internal
//!
//! Converts a given symbol `s` of `size` to a keyword token.
static uint32_t mpGetKeyword(const uint8_t* s, size_t size) {
  if (size == 3 && s[0] == 'v' && s[1] == 'a' && s[2] == 'r')
    return kTokenVar;

  return kTokenSymbol;
}

uint32_t Tokenizer::peek(Token* token) {
  uint32_t uToken = _token._tokenType;
  if (uToken != kTokenInvalid || (uToken = next(&_token)) != kTokenInvalid)
    *token = _token;
  return uToken;
}

uint32_t Tokenizer::next(Token* token) {
  // Skip parsing if the next token is already done, caused by `peek()`.
  uint32_t c = _token._tokenType;
  uint32_t hashCode;

  if (c != kTokenInvalid) {
    *token = _token;
    _token._tokenType = kTokenInvalid;
    return c;
  }

  // Input string.
  const uint8_t* p = reinterpret_cast<const uint8_t*>(_p);
  const uint8_t* pToken = p;

  const uint8_t* pStart = reinterpret_cast<const uint8_t*>(_start);
  const uint8_t* pEnd = reinterpret_cast<const uint8_t*>(_end);

  // --------------------------------------------------------------------------
  // [Spaces]
  // --------------------------------------------------------------------------

_Repeat:
  for (;;) {
    if (p == pEnd)
      goto _EndOfInput;

    hashCode = p[0];
    c = mpCharClass[hashCode];

    if (c != kTokenCharSpc)
      break;
    p++;
  }

  // Save the first character of the token.
  pToken = p;

  // --------------------------------------------------------------------------
  // [Number | Dot]
  // --------------------------------------------------------------------------

  if (c <= kTokenChar0x9 || c == kTokenCharDot) {
    // Parsing floating point is not that simple as it looks. To simplify the
    // most common cases we parse floating points up to `kSafeDigits` and then
    // use libc `strtod()` function to parse numbers that are more complicated.
    //
    // http://www.exploringbinary.com/fast-path-decimal-to-floating-point-conversion/
    double val = 0.0;
    size_t digits = 0;

    // Skip leading zeros.
    while (p[0] == '0') {
      if (++p == pEnd)
        break;
    }

    // Parse significand.
    size_t scale = 0;
    while (p != pEnd) {
      c = static_cast<uint32_t>(p[0]) - static_cast<uint32_t>('0');
      if (c > 9)
        break;
      scale++;

      if (c != 0) {
        if (scale < kPow10TableSize)
          val = val * mpPow10Table[scale] + static_cast<double>(static_cast<int>(c));
        digits += scale;
        scale = 0;
      }

      p++;
    }
    size_t significantDigits = digits + scale;

    // Parse fraction.
    if (p != pEnd && p[0] == '.') {
      while (++p != pEnd) {
        c = static_cast<uint32_t>(p[0]) - static_cast<uint32_t>('0');
        if (c > 9)
          break;
        scale++;

        if (c != 0) {
          if (scale < kPow10TableSize)
            val = val * mpPow10Table[scale] + static_cast<double>(static_cast<int>(c));
          digits += scale;
          scale = 0;
        }
      }

      // Token is '.'.
      if ((size_t)(p - pToken) == 1) {
        _p = reinterpret_cast<const char*>(p);
        return token->setData((size_t)(pToken - pStart), (size_t)(p - pToken), 0, kTokenDot);
      }
    }

    bool safe = digits <= kSafeDigits && significantDigits < 999999;
    int exponent = safe ? static_cast<int>(significantDigits) - static_cast<int>(digits) : 0;

    // Parse an optional exponent.
    if (p != pEnd && mpGetLower(p[0]) == 'e') {
      if (++p == pEnd)
        goto _Invalid;

      c = p[0];
      bool negative = c == '-';
      if (negative || c == '+')
        if (++p == pEnd)
          goto _Invalid;

      uint32_t e = 0;
      size_t expSize = 0;

      do {
        c = static_cast<uint32_t>(p[0]) - static_cast<uint32_t>('0');
        if (c > 9)
          break;

        e = e * 10 + c;
        expSize++;
      } while (++p != pEnd);

      // Error if there is no number after the 'e' token.
      if (expSize == 0)
        goto _Invalid;

      // If less than 10 digits it's safe to assume the exponent is zero if
      // `e` is zero. Otherwise it could have overflown the 32-bit integer.
      if (e == 0 && expSize < 10)
        expSize = 0; // No exponent.

      if (expSize <= 6)
        exponent += negative ? -static_cast<int>(e) : static_cast<int>(e);
      else
        safe = false;
    }

    // Error if there is an alpha-numeric character right next to the number.
    if (p != pEnd && mpCharClass[p[0]] <= kTokenCharSym)
      goto _Invalid;

    // Limit a range of safe values from Xe-15 to Xe15.
    safe = safe && exponent >= -kPow10TableSize && exponent <= kPow10TableSize;
    size_t size = (size_t)(p - pToken);

    if (safe) {
      if (exponent != 0)
        val = exponent < 0 ? val / mpPow10Table[-exponent] : val * mpPow10Table[exponent];
    }
    else {
      // Using libc's strtod is not optimal, but it's precise for complex cases.
      char tmp[512];
      char* buf = tmp;

      if (size >= MATHPRESSO_ARRAY_SIZE(tmp) && (buf = static_cast<char*>(::malloc(size + 1))) == NULL)
        return kTokenInvalid;

      memcpy(buf, pToken, size);
      buf[size] = '\0';

      val = _strtod.conv(buf, NULL);

      if (buf != tmp)
        ::free(buf);
    }

    token->_value = val;
    token->setData((size_t)(pToken - pStart), size, 0, kTokenNumber);

    _p = reinterpret_cast<const char*>(p);
    return kTokenNumber;
  }

  // --------------------------------------------------------------------------
  // [Symbol | Keyword]
  // --------------------------------------------------------------------------

  else if (c <= kTokenCharSym) {
    // We always calculate hashCode during tokenization to improve performance.
    while (++p != pEnd) {
      uint32_t ord = p[0];
      c = mpCharClass[ord];
      if (c > kTokenCharSym)
        break;
      hashCode = HashUtils::hashChar(hashCode, ord);
    }

    size_t size = (size_t)(p - pToken);
    _p = reinterpret_cast<const char*>(p);
    return token->setData((size_t)(pToken - pStart), size, hashCode, mpGetKeyword(pToken, size));
  }

  // --------------------------------------------------------------------------
  // [Single-Char]
  // --------------------------------------------------------------------------

  else if (c <= kTokenCharSingleCharTokenEnd) {
    _p = reinterpret_cast<const char*>(++p);
    return token->setData((size_t)(pToken - pStart), (size_t)(p - pToken), 0, c);
  }

  // --------------------------------------------------------------------------
  // [Single-Char | Multi-Char]
  // --------------------------------------------------------------------------

  else if (c < kTokenCharSpc) {
    uint32_t c1 = (++p != pEnd) ? static_cast<uint32_t>(p[0]) : static_cast<uint32_t>(0);

    switch (c) {
      case kTokenCharAdd: // `+=`, `++`, `+`.
        if (c1 == '=') { c = kTokenAssignAdd   ; p++; break; }
        if (c1 == '+') { c = kTokenPlusPlus    ; p++; break; }
        break;

      case kTokenCharSub: // `-=`, `--`, `-`.
        if (c1 == '=') { c = kTokenAssignSub   ; p++; break; }
        if (c1 == '-') { c = kTokenMinusMinus  ; p++; break; }
        break;

      case kTokenCharMul: // `*=`, `*`.
        if (c1 == '=') { c = kTokenAssignMul   ; p++; break; }
        break;

      case kTokenCharDiv: // `//`, `/=`, `/`.
        if (c1 == '/')
          goto _Comment;
        if (c1 == '=') { c = kTokenAssignDiv   ; p++; break; }
        break;

      case kTokenCharMod: // `%=`, `%`.
        if (c1 == '=') { c = kTokenAssignMod   ; p++; break; }
        break;

      case kTokenCharAnd: // `&=`, `&&`, `&`.
        if (c1 == '=') { c = kTokenAssignBitAnd; p++; break; }
        if (c1 == '&') { c = kTokenLogAnd      ; p++; break; }
        break;

      case kTokenCharOr: // `|=`, `||`, `|`.
        if (c1 == '=') { c = kTokenAssignBitOr ; p++; break; }
        if (c1 == '|') { c = kTokenLogOr       ; p++; break; }
        break;

      case kTokenCharXor: // `^=`, `^`.
        if (c1 == '=') { c = kTokenAssignBitXor; p++; break; }
        break;

      case kTokenCharNeg: // `~`.
        break;

      case kTokenCharNot: // `!=`, `!`.
        if (c1 == '=') { c = kTokenNe; p++; break; }
        break;

      case kTokenCharEq: // `==`, `=`.
        if (c1 == '=') { c = kTokenEq; p++; break; }
        break;

      case kTokenCharLt: // `<<=`, `<<`, `<=`, `<`.
        if (c1 == '<') {
          if (++p != pEnd && p[0] == '=') {
            c = kTokenAssignBitShl; p++;
          }
          else {
            c = kTokenBitShl;
          }
          break;
        }
        if (c1 == '=') { c = kTokenLe; p++; break; }
        break;

      case kTokenCharGt: // `>>>=`, `>>>`, `>>=`, `>>`, `>=`, `>`.
        if (c1 == '>') {
          if (++p != pEnd) {
            uint32_t c2 = static_cast<uint8_t>(p[0]);
            if (c2 == '>') {
              if (++p != pEnd && p[0] == '=') {
                c = kTokenAssignBitShr; p++;
              }
              else {
                c = kTokenBitShr;
              }
              break;
            }
            else if (c2 == '=') {
              c = kTokenAssignBitSar;
              break;
            }
          }
          c = kTokenBitSar;
          break;
        }
        if (c1 == '=') { c = kTokenGe; p++; break; }
        break;
    }

    _p = reinterpret_cast<const char*>(p);
    return token->setData((size_t)(pToken - pStart), (size_t)(p - pToken), 0, c);
  }

  // --------------------------------------------------------------------------
  // [Invalid]
  // --------------------------------------------------------------------------

_Invalid:
  _p = reinterpret_cast<const char*>(pToken);
  return token->setData((size_t)(pToken - pStart), (size_t)(p - pToken), 0, kTokenInvalid);

  // --------------------------------------------------------------------------
  // [Comment]
  // --------------------------------------------------------------------------

_Comment:
  for (;;) {
    if (p == pEnd)
      goto _EndOfInput;

    c = static_cast<uint8_t>(*p++);
    if (c == '\n')
      goto _Repeat;
  }

  // --------------------------------------------------------------------------
  // [EOI]
  // --------------------------------------------------------------------------

_EndOfInput:
  _p = _end;
  return token->setData((size_t)(pToken - pStart), (size_t)(p - pToken), 0, kTokenEnd);
}

} // mathpresso namespace
