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
#include "mathpresso_optimizer_p.h"
#include "mathpresso_parser_p.h"
#include "mathpresso_tokenizer_p.h"
#include "mathpresso_util_p.h"
#include "mathpresso_jit_p.h"

#include <math.h>
#include <string.h>

namespace mathpresso {

// ============================================================================
// [mathpresso::Context - Construction / Destruction]
// ============================================================================

Context::Context() {
  _privateData = new(std::nothrow) ContextPrivate();
}

Context::Context(const Context& other) {
  ContextPrivate* d = reinterpret_cast<ContextPrivate*>(other._privateData);
  if (d) d->addRef();
  _privateData = d;
}

Context::~Context() {
  ContextPrivate* d = reinterpret_cast<ContextPrivate*>(_privateData);
  if (d) d->release();
}

// ============================================================================
// [mathpresso::Context - Private]
// ============================================================================

static MPResult Context_addFunction(Context* self, const char* name, void* ptr, int prototype, int functionId = -1) {
  ContextPrivate* d = reinterpret_cast<ContextPrivate*>(self->_privateData);
  if (d == NULL) return kMPResultNoMemory;

  size_t nlen = strlen(name);
  Function* fdata = d->functions.get(name, nlen);
  if (fdata && fdata->ptr == ptr && fdata->prototype == prototype) return kMPResultOk;

  if (!d->isDetached()) {
    d = d->copy();
    if (!d) return kMPResultNoMemory;

    reinterpret_cast<ContextPrivate*>(self->_privateData)->release();
    self->_privateData = d;
  }

  return d->functions.put(name, nlen, Function(ptr, prototype, functionId))
    ? kMPResultOk
    : kMPResultNoMemory;
}

// ============================================================================
// [mathpresso::Context - Environment]
// ============================================================================

typedef double (*MPFunc1)(double x);
typedef double (*MPFunc2)(double x, double y);

static double mpMin(double x, double y) { return x < y ? x : y; }
static double mpMax(double x, double y) { return x > y ? x : y; }
static double mpAvg(double x, double y) { return (x + y) * 0.5; }
static double mpRecip(double x) { return 1.0 / x; }

#define MP_ADD_CONST(self, name, value) \
  do { \
    MPResult __r = self->addConstant(name, value); \
    if (__r != kMPResultOk) return __r; \
  } while (0)

#define MP_ADD_FUNCTION(self, name, proto, ptr, prototype, id) \
  do { \
    MPResult __r = Context_addFunction(self, name, (void*)(proto)(ptr), prototype, id); \
    if (__r != kMPResultOk) return __r; \
  } while (0)

static MPResult Context_addEnvironment_Math(Context* self) {
  // Constants.
  MP_ADD_CONST(self, "E", 2.7182818284590452354);
  MP_ADD_CONST(self, "PI", 3.14159265358979323846);

  // Functions.
  MP_ADD_FUNCTION(self, "round"     , MPFunc1, round  , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionRound);
  MP_ADD_FUNCTION(self, "floor"     , MPFunc1, floor  , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionFloor);
  MP_ADD_FUNCTION(self, "ceil"      , MPFunc1, ceil   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionCeil);

  MP_ADD_FUNCTION(self, "abs"       , MPFunc1, abs    , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionAbs);
  MP_ADD_FUNCTION(self, "sqrt"      , MPFunc1, sqrt   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionSqrt);
  MP_ADD_FUNCTION(self, "recip"     , MPFunc1, mpRecip, kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionRecip);

  MP_ADD_FUNCTION(self, "log"       , MPFunc1, log    , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionLog);
  MP_ADD_FUNCTION(self, "log10"     , MPFunc1, log10  , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionLog10);
  MP_ADD_FUNCTION(self, "sin"       , MPFunc1, sin    , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionSin);
  MP_ADD_FUNCTION(self, "cos"       , MPFunc1, cos    , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionCos);
  MP_ADD_FUNCTION(self, "tan"       , MPFunc1, tan    , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionTan);
  MP_ADD_FUNCTION(self, "sinh"      , MPFunc1, sinh   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionSinh);
  MP_ADD_FUNCTION(self, "cosh"      , MPFunc1, cosh   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionCosh);
  MP_ADD_FUNCTION(self, "tanh"      , MPFunc1, tanh   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionTanh);
  MP_ADD_FUNCTION(self, "asin"      , MPFunc1, asin   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionAsin);
  MP_ADD_FUNCTION(self, "acos"      , MPFunc1, acos   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionAcos);
  MP_ADD_FUNCTION(self, "atan"      , MPFunc1, atan   , kMPFuncProtoDoubleDouble1 | kMPFuncSafe, kMPFunctionAtan);
  MP_ADD_FUNCTION(self, "atan2"     , MPFunc2, atan2  , kMPFuncProtoDoubleDouble2 | kMPFuncSafe, kMPFunctionAtan2);

  MP_ADD_FUNCTION(self, "avg"       , MPFunc2, mpAvg  , kMPFuncProtoDoubleDouble2 | kMPFuncSafe, kMPFunctionAvg);
  MP_ADD_FUNCTION(self, "min"       , MPFunc2, mpMin  , kMPFuncProtoDoubleDouble2 | kMPFuncSafe, kMPFunctionMin);
  MP_ADD_FUNCTION(self, "max"       , MPFunc2, mpMax  , kMPFuncProtoDoubleDouble2 | kMPFuncSafe, kMPFunctionMax);
  MP_ADD_FUNCTION(self, "pow"       , MPFunc2, pow    , kMPFuncProtoDoubleDouble2 | kMPFuncSafe, kMPFunctionPow);

  return kMPResultOk;
}

MPResult Context::addEnvironment(int environmentId) {
  switch (environmentId) {
    case kMPEnvironmentMath:
      return Context_addEnvironment_Math(this);

    case kMPEnvironmentAll: {
      MPResult result;
      for (int i = 1; i < kMPEnvironmentCount; i++)
        if ((result = addEnvironment(i)) != kMPResultOk)
          return result;
      return kMPResultOk;
    }

    default:
      return kMPResultInvalidArgument;
  }
}

// ============================================================================
// [mathpresso::Context - Function]
// ============================================================================

MPResult Context::addFunction(const char* name, void* ptr, int prototype) {
  return Context_addFunction(this, name, ptr, prototype);
}

// ============================================================================
// [mathpresso::Context - Constant]
// ============================================================================

MPResult Context::addConstant(const char* name, double value) {
  ContextPrivate* d = reinterpret_cast<ContextPrivate*>(_privateData);
  if (d == NULL) return kMPResultNoMemory;

  size_t nlen = strlen(name);
  Variable* variable = d->variables.get(name, nlen);
  if (variable && variable->type == kMPVariableTypeConst && variable->c.value == value)
    return kMPResultOk;

  if (!d->isDetached()) {
    d = d->copy();
    if (!d) return kMPResultNoMemory;

    reinterpret_cast<ContextPrivate*>(_privateData)->release();
    _privateData = d;
  }

  return d->variables.put(name, nlen, Variable(kMPVariableTypeConst, value))
    ? kMPResultOk
    : kMPResultNoMemory;
}

// ============================================================================
// [mathpresso::Context - Variable]
// ============================================================================

MPResult Context::addVariable(const char* name, int offset, int flags) {
  ContextPrivate* d = reinterpret_cast<ContextPrivate*>(_privateData);
  if (d == NULL) return kMPResultNoMemory;

  size_t nlen = strlen(name);
  int type = (flags & kMPVarRO) ? kMPVariableTypeRO : kMPVariableTypeRW;

  Variable* variable = d->variables.get(name, nlen);
  if (variable && variable->type == type && 
                  variable->v.offset == offset && 
                  variable->v.flags == flags)
    return kMPResultOk;

  if (!d->isDetached()) {
    d = d->copy();
    if (!d) return kMPResultNoMemory;

    reinterpret_cast<ContextPrivate*>(_privateData)->release();
    _privateData = d;
  }

  return d->variables.put(name, nlen, Variable(type, offset, flags))
    ? kMPResultOk
    : kMPResultNoMemory;
}

// ============================================================================
// [mathpresso::Context - Symbol]
// ============================================================================

MPResult Context::delSymbol(const char* name) {
  ContextPrivate* d = reinterpret_cast<ContextPrivate*>(_privateData);
  if (d == NULL) return kMPResultNoMemory;

  size_t nlen = strlen(name);
  if (!d->variables.contains(name, nlen) &&
      !d->functions.contains(name, nlen))
    return kMPResultOk;

  if (!d->isDetached()) {
    d = d->copy();
    if (!d) return kMPResultNoMemory;

    reinterpret_cast<ContextPrivate*>(_privateData)->release();
    _privateData = d;
  }

  d->variables.remove(name, nlen);
  d->functions.remove(name, nlen);
  return kMPResultOk;
}

// ============================================================================
// [mathpresso::Context - Clear]
// ============================================================================

MPResult Context::clear() {
  ContextPrivate* d = reinterpret_cast<ContextPrivate*>(_privateData);
  if (d == NULL) return kMPResultNoMemory;

  if (!d->isDetached()) {
    d = new(std::nothrow) ContextPrivate();
    if (!d) return kMPResultNoMemory;

    reinterpret_cast<ContextPrivate*>(_privateData)->release();
    _privateData = d;
  }
  else {
    d->variables.clear();
    d->functions.clear();
  }

  return kMPResultOk;
}

// ============================================================================
// [mathpresso::Context - Operator Overload]
// ============================================================================

Context& Context::operator=(const Context& other) {
  ContextPrivate* newp = reinterpret_cast<ContextPrivate*>(other._privateData);
  if (newp) newp->addRef();

  ContextPrivate* oldp = reinterpret_cast<ContextPrivate*>(_privateData);
  if (oldp) oldp->release();

  _privateData = newp;
  return *this;
}

// ============================================================================
// [mathpresso::Eval Entry-Points]
// ============================================================================

static void mEvalDummy(const void*, double* result, void*) {
  *result = 0.0f;
}

static void mEvalExpression(const void* _p, double* result, void* data) {
  const ExpressionPrivate* p = reinterpret_cast<const ExpressionPrivate*>(_p);

  MP_ASSERT(p->ast != NULL);
  *result = p->ast->evaluate(data);
}

// ============================================================================
// [mathpresso::Expression - Construction / Destruction]
// ============================================================================

Expression::Expression()
  : _privateData(NULL),
    _evaluate(mEvalDummy) {
  _privateData = new(std::nothrow) ExpressionPrivate();
}

Expression::~Expression() {
  free();
  if (_privateData) delete reinterpret_cast<ExpressionPrivate*>(_privateData);
}

// ============================================================================
// [mathpresso::Expression - Create / Free]
// ============================================================================

MPResult Expression::create(const Context& ectx, const char* expression, int options) {
  if (_privateData == NULL)
    return kMPResultNoMemory;

  ExpressionPrivate* p = reinterpret_cast<ExpressionPrivate*>(_privateData);
  WorkContext ctx(ectx);

  // Destroy previous expression and prepare for error state (if something 
  // will fail).
  free();

  // Parse the expression.
  MPParser parser(ctx, expression, strlen(expression));

  ASTNode* ast = NULL;
  int result = parser.parse(&ast);
  if (result != kMPResultOk) return result;

  // If something failed, report it, but don't continue.
  if (ast == NULL) return kMPResultNoExpression;

  // Simplify the expression, evaluating all constant parts.
  {
    MPOptimizer optimizer(ctx);
    ast = optimizer.onNode(ast);
  }

  // Compile using JIT compiler if enabled.
  if (options & kMPOptionDisableJIT)
    _evaluate = NULL;
  else
    _evaluate = mpCompileFunction(ctx, ast, (options & kMPOptionDumpJIT) != 0);

  // Fallback to evaluation if JIT compiling failed or not enabled.
  if (_evaluate == NULL) {
    _evaluate = mEvalExpression;
    p->ast = ast;
  }
  else {
    delete ast;
  }

  p->ctx = ctx._ctx;
  p->ctx->addRef();
  return kMPResultOk;
}

void Expression::free() {
  if (_privateData == NULL) return;
  ExpressionPrivate* p = reinterpret_cast<ExpressionPrivate*>(_privateData);

  if (_evaluate != mEvalDummy && _evaluate != mEvalExpression) {
    // Allocated by JIT memory manager, free it.
    mpFreeFunction((void*)_evaluate);
  }

  // Set evaluate to dummy function so it will not crash when called through
  // Expression::evaluate().
  _evaluate = mEvalDummy;

  if (p->ast) {
    delete p->ast;
    p->ast = NULL;
  }

  if (p->ctx) {
    p->ctx->release();
    p->ctx = NULL;
  }
}

} // mathpresso namespace
