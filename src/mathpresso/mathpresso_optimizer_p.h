// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _MATHPRESSO_OPTIMIZER_P_H
#define _MATHPRESSO_OPTIMIZER_P_H

#include "mathpresso.h"
#include "mathpresso_ast_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::MPOptimizer]
// ============================================================================

//! \internal
//!
//! Optimizes the AST expression tree by evaluating constant expressions.
struct MPOptimizer {
  MPOptimizer(WorkContext& ctx);
  ~MPOptimizer();

  ASTNode* onNode(ASTNode* node);
  ASTNode* onBlock(ASTBlock* node);
  ASTNode* onUnaryOp(ASTUnaryOp* node);
  ASTNode* onBinaryOp(ASTBinaryOp* node);
  ASTNode* onCall(ASTCall* node);

  ASTNode* findConstNode(ASTNode* node, int op);

  WorkContext& _ctx;
};

} // mathpresso namespace

#endif // _MATHPRESSO_OPTIMIZER_P_H
