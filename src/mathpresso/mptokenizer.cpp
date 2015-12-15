// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_EXPORTS

// [Dependencies]
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

#define CHAR4X(C0, C1, C2, C3) \
  ( (static_cast<uint32_t>(C0)      ) + \
    (static_cast<uint32_t>(C1) <<  8) + \
    (static_cast<uint32_t>(C2) << 16) + \
    (static_cast<uint32_t>(C3) << 24) )

//! \internal
//!
//! Converts a given symbol `s` of `sLen` to a keyword token.
static uint32_t mpGetKeyword(const char* s, size_t sLen) {
  if (sLen == 3 && s[0] == 'v' && s[1] == 'a' && s[2] == 'r')
    return kTokenVar;

  return kTokenSymbol;
}

uint32_t Tokenizer::peek(Token* token) {
  uint32_t uToken = _token.token;
  if (uToken != kTokenInvalid || (uToken = next(&_token)) != kTokenInvalid)
    *token = _token;
  return uToken;
}

uint32_t Tokenizer::next(Token* token) {
  // Skip parsing if the next token is already done, caused by `peek()`.
  uint32_t c = _token.token;
  uint32_t hVal;

  if (c != kTokenInvalid) {
    *token = _token;
    _token.token = kTokenInvalid;
    return c;
  }

  // Input string.
  const char* p = _p;
  const char* pToken = p;
  const char* pEnd = _end;

_Repeat:
  // Skip spaces.
  for (;;) {
    if (p == pEnd)
      goto _EndOfInput;

    hVal = static_cast<uint8_t>(p[0]);
    c = mpCharClass[hVal];

    if (c != kTokenCharSpc)
      break;
    p++;
  }

  // Save the first character of the token.
  pToken = p;

  if (c <= kTokenChar0x9) {
    double iPart = static_cast<double>(static_cast<int>(c));
    double fPart = 0.0;
    int fPos = 0;

    // Parse the decimal part.
    for (;;) {
      if (++p == pEnd)
        goto _NumberEnd;

      c = static_cast<uint8_t>(p[0]) - '0';
      if (c > 9)
        break;

      iPart = (iPart * 10.0) + static_cast<double>(static_cast<int>(c));
    }

    // Parse an optional fraction.
    c = static_cast<uint8_t>(p[0]);

    if (c == '.') {
      for (;;) {
        if (++p == pEnd)
          goto _NumberEnd;

        c = static_cast<uint8_t>(p[0]) - '0';
        if (c > 9)
          break;

        fPart = (fPart * 10.0) + static_cast<double>(static_cast<int>(c));
        fPos--;
      }
    }

    // Error if there is an alpha-numeric character after the number.
    if (p != pEnd && mpCharClass[static_cast<uint8_t>(p[0])] <= kTokenCharSym) {
      goto _Invalid;
    }

_NumberEnd:
    {
      double val = iPart;
      if (fPos != 0)
        val += fPart * ::pow(10.0, fPos);

      token->value = val;
      token->setData((size_t)(pToken - _start), (size_t)(p - pToken), 0, kTokenNumber);
    }

    _p = p;
    return kTokenNumber;
  }

  // Symbol or Keyword.
  else if (c <= kTokenCharSym) {
    // We always generate the hVal during tokenization to improve performance.
    size_t len;

    while (++p != pEnd) {
      uint32_t ord = static_cast<uint8_t>(p[0]);
      c = mpCharClass[ord];
      if (c > kTokenCharSym)
        break;
      hVal = HashUtils::hashChar(hVal, ord);
    }

    len = (size_t)(p - pToken);
    _p = p;
    return token->setData((size_t)(pToken - _start), len, hVal, mpGetKeyword(pToken, len));
  }

  // Single-Char Punctuation.
  else if (c <= kTokenCharSingleCharTokenEnd) {
    _p = ++p;
    return token->setData((size_t)(pToken - _start), (size_t)(p - pToken), 0, c);
  }

  // Single-Char/Multi-Char Punctuation.
  else if (c < kTokenCharSpc) {
    p++;
    uint32_t c1 = 0;

    if (p != pEnd)
      c1 = static_cast<uint8_t>(p[0]);

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

    _p = p;
    return token->setData((size_t)(pToken - _start), (size_t)(p - pToken), 0, c);
  }

_Invalid:
  _p = pToken;
  return token->setData((size_t)(pToken - _start), (size_t)(p - pToken), 0, kTokenInvalid);

_Comment:
  for (;;) {
    if (p == pEnd)
      goto _EndOfInput;

    c = static_cast<uint8_t>(*p++);
    if (c == '\n')
      goto _Repeat;
  }

_EndOfInput:
  _p = _end;
  return token->setData((size_t)(pToken - _start), (size_t)(p - pToken), 0, kTokenEnd);
}

} // mathpresso namespace
