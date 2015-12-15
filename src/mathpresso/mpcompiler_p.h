// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPCOMPILER_P_H
#define _MATHPRESSO_MPCOMPILER_P_H

// [Dependencies]
#include "./mpast_p.h"

namespace mathpresso {

MATHPRESSO_NOAPI CompiledFunc mpCompileFunction(AstBuilder* ast, uint32_t options, OutputLog* log);
MATHPRESSO_NOAPI void mpFreeFunction(void* fn);

} // mathpresso namespace

// [Guard]
#endif // _MATHPRESSO_MPCOMPILER_P_H
