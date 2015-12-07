// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _MATHPRESSO_PARSER_P_H
#define _MATHPRESSO_PARSER_P_H

#include "mathpresso.h"
#include "mathpresso_ast_p.h"
#include "mathpresso_tokenizer_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::MPParser]
// ============================================================================

//! \internal
//!
//! Expression parser.
struct MATHPRESSO_NOAPI MPParser {
  //! Create a new `MPParser` instance.
  MPParser(WorkContext& ctx, const char* input, size_t length);
  //! Destroy the `MPParser` instance.
  ~MPParser();

  //! Parse complete expression.
  MPResult parse(ASTNode** dst);
  //! Parse single expression tree.
  MPResult parseTree(ASTNode** dst);

  //! Parse single expression, terminating on right paren or semicolon.
  MPResult parseExpression(ASTNode** dst,
    ASTNode* _left,
    int minPriority,
    bool isInsideExpression);

protected:
  WorkContext& _ctx;

  MPTokenizer _tokenizer;
  MPToken _last;
};

} // mathpresso namespace

#endif // _MATHPRESSO_PARSER_P_H
