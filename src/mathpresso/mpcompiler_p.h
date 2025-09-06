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

MATHPRESSO_NOAPI CompiledFunc compile_function(AstBuilder* ast, uint32_t options, OutputLog* log);
MATHPRESSO_NOAPI void free_compiled_function(void* fn);

} // {mathpresso}

// [Guard]
#endif // _MATHPRESSO_MPCOMPILER_P_H
