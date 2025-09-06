// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPOPTIMIZER_P_H
#define _MATHPRESSO_MPOPTIMIZER_P_H

// [Dependencies]
#include "./mpast_p.h"

namespace mathpresso {

// MathPresso - AstOptimizer
// =========================

struct AstOptimizer : public AstVisitor {
  MATHPRESSO_NONCOPYABLE(AstOptimizer)

  // Members
  // -------

  ErrorReporter* _error_reporter;

  // Construction & Destruction
  // --------------------------

  AstOptimizer(AstBuilder* ast, ErrorReporter* error_reporter);
  virtual ~AstOptimizer();

  virtual Error on_block(AstBlock* node);
  virtual Error on_var_decl(AstVarDecl* node);
  virtual Error on_var(AstVar* node);
  virtual Error on_imm(AstImm* node);
  virtual Error on_unary_op(AstUnaryOp* node);
  virtual Error on_binary_op(AstBinaryOp* node);
  virtual Error on_invoke(AstCall* node);
};

} // {mathpresso}

// [Guard]
#endif // _MATHPRESSO_MPOPTIMIZER_P_H
