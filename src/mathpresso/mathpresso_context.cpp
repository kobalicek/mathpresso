// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_EXPORTS

// [Dependencies]
#include "mathpresso.h"
#include "mathpresso_ast_p.h"
#include "mathpresso_context_p.h"
#include "mathpresso_parser_p.h"
#include "mathpresso_tokenizer_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::ContextPrivate]
// ============================================================================

ContextPrivate::ContextPrivate() {
  refCount.init(1);
}
ContextPrivate::~ContextPrivate() {}

ContextPrivate* ContextPrivate::copy() const {
  ContextPrivate* ctx = new(std::nothrow) ContextPrivate();
  if (ctx == NULL) return NULL;

  if (!ctx->variables.mergeWith(variables)) { delete ctx; return NULL; }
  if (!ctx->functions.mergeWith(functions)) { delete ctx; return NULL; }
  return ctx;
}

// ============================================================================
// [mathpresso::WorkContext]
// ============================================================================

WorkContext::WorkContext(const Context& ctx) : _id(0) {
  _ctx = reinterpret_cast<ContextPrivate*>(ctx._privateData);
}
WorkContext::~WorkContext() {}

} // mathpresso namespace
