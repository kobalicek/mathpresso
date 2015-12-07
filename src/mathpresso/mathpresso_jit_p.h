// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _MATHPRESSO_JIT_P_H
#define _MATHPRESSO_JIT_P_H

#include "mathpresso.h"
#include "mathpresso_ast_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

MATHPRESSO_NOAPI MPEvalFunc mpCompileFunction(WorkContext& ctx, ASTNode* tree);
MATHPRESSO_NOAPI void mpFreeFunction(void* fn);

} // mathpresso namespace

#endif // _MATHPRESSO_JIT_P_H
