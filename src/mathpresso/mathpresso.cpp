// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mathpresso_p.h"
#include "./mpast_p.h"
#include "./mpatomic_p.h"
#include "./mpcompiler_p.h"
#include "./mpeval_p.h"
#include "./mpoptimizer_p.h"
#include "./mpparser_p.h"
#include "./mptokenizer_p.h"

#include <math.h>
#include <string.h>

namespace mathpresso {

// ============================================================================
// [mathpresso::mpOpInfo]
// ============================================================================

// Operator information, precedence and association. The table is mostly based
// on the C-language standard, but also adjusted to support MATHPRESSO specific
// operators and rules. However, the associativity and precedence should be
// fully compatible with C.
#define ROW(opType, altType, params, precedence, assignment, intrinsic, flags, name) \
  { \
    uint8_t(kOp##opType), \
    uint8_t(kOp##altType), \
    uint8_t(precedence), \
    uint8_t(0), \
    uint32_t(flags | (assignment != 0 ? uint32_t(kOpFlagAssign   ) : uint32_t(0)) \
                   | (params     == 1 ? uint32_t(kOpFlagUnary    ) : uint32_t(0)) \
                   | (params     == 2 ? uint32_t(kOpFlagBinary   ) : uint32_t(0)) \
                   | (intrinsic  == 1 ? uint32_t(kOpFlagIntrinsic) : uint32_t(0))), \
    name \
  }
#define LTR 0
#define RTL kOpFlagRightToLeft
#define F(flag) kOpFlag##flag
const OpInfo mpOpInfo[kOpCount] = {
  // +-------------+----------+--+--+--+--+-----+------------------------------------+------------+
  // |Operator     | Alt.     |#N|#P|:=|#I|Assoc| Flags                              | Name       |
  // +-------------+----------+--+--+--+--+-----+------------------------------------+------------+
  ROW(None         , None     , 0, 0, 0, 0, LTR | 0                                  , "<none>"   ),
  ROW(Neg          , Neg      , 1, 3, 0, 0, RTL | F(Arithmetic)                      , "-"        ),
  ROW(Not          , Not      , 1, 3, 0, 0, RTL | F(Condition)                       , "!"        ),
  ROW(IsNan        , IsNan    , 1, 0, 0, 1, LTR | F(Condition)                       , "isnan"    ),
  ROW(IsInf        , IsInf    , 1, 0, 0, 1, LTR | F(Condition)                       , "isinf"    ),
  ROW(IsFinite     , IsFinite , 1, 0, 0, 1, LTR | F(Condition)                       , "isfinite" ),
  ROW(SignBit      , SignBit  , 1, 0, 0, 1, LTR | F(Condition)                       , "signbit"  ),
  ROW(Round        , Round    , 1, 0, 0, 1, LTR | F(Rounding)                        , "round"    ),
  ROW(RoundEven    , RoundEven, 1, 0, 0, 1, LTR | F(Rounding)                        , "roundeven"),
  ROW(Trunc        , Trunc    , 1, 0, 0, 1, LTR | F(Rounding)                        , "trunc"    ),
  ROW(Floor        , Floor    , 1, 0, 0, 1, LTR | F(Rounding)                        , "floor"    ),
  ROW(Ceil         , Ceil     , 1, 0, 0, 1, LTR | F(Rounding)                        , "ceil"     ),
  ROW(Abs          , Abs      , 1, 0, 0, 1, LTR | 0                                  , "abs"      ),
  ROW(Exp          , Exp      , 1, 0, 0, 1, LTR | 0                                  , "exp"      ),
  ROW(Log          , Log      , 1, 0, 0, 1, LTR | 0                                  , "log"      ),
  ROW(Log2         , Log2     , 1, 0, 0, 1, LTR | 0                                  , "log2"     ),
  ROW(Log10        , Log10    , 1, 0, 0, 1, LTR | 0                                  , "log10"    ),
  ROW(Sqrt         , Sqrt     , 1, 0, 0, 1, LTR | 0                                  , "sqrt"     ),
  ROW(Frac         , Frac     , 1, 0, 0, 1, LTR | 0                                  , "frac"     ),
  ROW(Recip        , Recip    , 1, 0, 0, 1, LTR | 0                                  , "recip"    ),
  ROW(Sin          , Sin      , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "sin"      ),
  ROW(Cos          , Cos      , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "cos"      ),
  ROW(Tan          , Tan      , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "tan"      ),
  ROW(Sinh         , Sinh     , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "sinh"     ),
  ROW(Cosh         , Cosh     , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "cosh"     ),
  ROW(Tanh         , Tanh     , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "tanh"     ),
  ROW(Asin         , Asin     , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "asin"     ),
  ROW(Acos         , Acos     , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "acos"     ),
  ROW(Atan         , Atan     , 1, 0, 0, 1, LTR | F(Trigonometric)                   , "atan"     ),
  ROW(Assign       , Assign   , 2,15,-1, 0, RTL | 0                                  , "="        ),
  ROW(Eq           , Eq       , 2, 9, 0, 0, LTR | F(Condition)                       , "=="       ),
  ROW(Ne           , Ne       , 2, 9, 0, 0, LTR | F(Condition)                       , "!="       ),
  ROW(Lt           , Lt       , 2, 8, 0, 0, LTR | F(Condition)                       , "<"        ),
  ROW(Le           , Le       , 2, 8, 0, 0, LTR | F(Condition)                       , "<="       ),
  ROW(Gt           , Gt       , 2, 8, 0, 0, LTR | F(Condition)                       , ">"        ),
  ROW(Ge           , Ge       , 2, 8, 0, 0, LTR | F(Condition)                       , ">="       ),
  ROW(Add          , Add      , 2, 6, 0, 0, LTR | F(Arithmetic)    | F(NopIfZero)    , "+"        ),
  ROW(Sub          , Sub      , 2, 6, 0, 0, LTR | F(Arithmetic)    | F(NopIfRZero)   , "-"        ),
  ROW(Mul          , Mul      , 2, 5, 0, 0, LTR | F(Arithmetic)    | F(NopIfOne)     , "*"        ),
  ROW(Div          , Div      , 2, 5, 0, 0, LTR | F(Arithmetic)    | F(NopIfROne)    , "/"        ),
  ROW(Mod          , Mod      , 2, 5, 0, 0, LTR | F(Arithmetic)                      , "%"        ),
  ROW(Avg          , Avg      , 2, 0, 0, 1, LTR | 0                                  , "avg"      ),
  ROW(Min          , Min      , 2, 0, 0, 1, LTR | 0                                  , "min"      ),
  ROW(Max          , Max      , 2, 0, 0, 1, LTR | 0                                  , "max"      ),
  ROW(Pow          , Pow      , 2, 0, 0, 1, LTR |                    F(NopIfROne)    , "pow"      ),
  ROW(Atan2        , Atan2    , 2, 0, 0, 1, LTR | F(Trigonometric)                   , "atan2"    ),
  ROW(Hypot        , Hypot    , 2, 0, 0, 1, LTR | F(Trigonometric)                   , "hypot"    ),
  ROW(CopySign     , CopySign , 2, 0, 0, 1, LTR | 0                                  , "copysign" )
};
#undef F
#undef RTL
#undef LTR
#undef ROW

// ============================================================================
// [mathpresso::mpAssertionFailure]
// ============================================================================

void mpAssertionFailure(const char* file, int line, const char* msg) {
  fprintf(stderr,
    "[mathpresso] Assertion failed at %s (line %d):\n"
    "[mathpresso] %s\n", file, line, msg);

  ::abort();
}

// ============================================================================
// [mathpresso::mpTraceError]
// ============================================================================

MATHPRESSO_NOAPI Error mpTraceError(Error error) {
  return error;
}

// ============================================================================
// [mathpresso::mpDummyFunc]
// ============================================================================

//! \internal
//!
//! Used instead of NULL in `Expression::_func`.
//!
//! Returns NaN.
static void mpDummyFunc(double* result, void*) {
  *result = mpGetNan();
}

// ============================================================================
// [mathpresso::ContextInternalImpl]
// ============================================================================

//! \internal
//!
//! Null context data.
static const ContextImpl mpContextNull = { 0 };

//! \internal
//!
//! Internal context data.
struct ContextInternalImpl : public ContextImpl {
  MATHPRESSO_INLINE ContextInternalImpl()
    : _zone(32768 - Zone::kBlockOverhead),
      _allocator(&_zone),
      _builder(&_allocator),
      _scope(&_builder, nullptr, kAstScopeGlobal) {
    mpAtomicSet(&_refCount, 1);
  }
  MATHPRESSO_INLINE ~ContextInternalImpl() {}

  Zone _zone;
  ZoneAllocator _allocator;
  AstBuilder _builder;
  AstScope _scope;
};

static MATHPRESSO_INLINE ContextImpl* mpContextAddRef(ContextImpl* d) {
  if (d != &mpContextNull)
    mpAtomicInc(&d->_refCount);
  return d;
}

static MATHPRESSO_INLINE void mpContextRelease(ContextImpl* d) {
  if (d != &mpContextNull && !mpAtomicDec(&d->_refCount))
    delete static_cast<ContextInternalImpl*>(d);
}

static ContextImpl* mpContextClone(ContextImpl* otherD_) {
  ContextInternalImpl* d = new(std::nothrow) ContextInternalImpl();
  if (MATHPRESSO_UNLIKELY(!d))
    return nullptr;

  if (otherD_ != &mpContextNull) {
    ContextInternalImpl* otherD = static_cast<ContextInternalImpl*>(otherD_);
    AstSymbolHashIterator it(otherD->_scope._symbols);

    while (it.has()) {
      AstSymbol* sym = it.get();

      StringRef name(sym->_name, sym->_nameSize);
      uint32_t hashCode = sym->hashCode();
      uint32_t type = sym->symbolType();

      AstSymbol* clonedSym = d->_builder.newSymbol(name, hashCode, type, otherD->_scope.scopeType());
      if (MATHPRESSO_UNLIKELY(!clonedSym)) {
        delete d;
        return nullptr;
      }

      clonedSym->_symbolFlags = sym->_symbolFlags;
      switch (type) {
        case kAstSymbolVariable:
          clonedSym->setVarSlotId(sym->varSlotId());
          clonedSym->setVarOffset(sym->varOffset());
          clonedSym->_value = sym->value();
          break;

        case kAstSymbolIntrinsic:
        case kAstSymbolFunction:
          clonedSym->setOpType(sym->opType());
          clonedSym->setFuncArgs(sym->funcArgs());
          clonedSym->setFuncPtr(sym->funcPtr());
          break;

        default:
          MATHPRESSO_ASSERT_NOT_REACHED();
      }

      d->_scope.putSymbol(clonedSym);
      it.next();
    }
  }

  return d;
}

static Error mpContextMutable(Context* self, ContextInternalImpl** out) {
  ContextImpl* d = self->_d;

  if (d != &mpContextNull && mpAtomicGet(&d->_refCount) == 1) {
    *out = static_cast<ContextInternalImpl*>(d);
    return kErrorOk;
  }
  else {
    d = mpContextClone(d);
    if (MATHPRESSO_UNLIKELY(!d))
      return MATHPRESSO_TRACE_ERROR(kErrorNoMemory);

    mpContextRelease(
      mpAtomicSetXchgT<ContextImpl*>(
        &self->_d, d));

    *out = static_cast<ContextInternalImpl*>(d);
    return kErrorOk;
  }
}

// ============================================================================
// [mathpresso::Context - Construction / Destruction]
// ============================================================================

Context::Context()
  : _d(const_cast<ContextImpl*>(&mpContextNull)) {}

Context::Context(const Context& other)
  : _d(mpContextAddRef(other._d)) {}

Context::~Context() {
  mpContextRelease(_d);
}

// ============================================================================
// [mathpresso::Context - Copy / Reset]
// ============================================================================

Error Context::reset() {
  mpContextRelease(
    mpAtomicSetXchgT<ContextImpl*>(
      &_d, const_cast<ContextImpl*>(&mpContextNull)));

  return kErrorOk;
}

Context& Context::operator=(const Context& other) {
  mpContextRelease(
    mpAtomicSetXchgT<ContextImpl*>(
      &_d, mpContextAddRef(other._d)));
  return *this;
}

// ============================================================================
// [mathpresso::Context - Interface]
// ============================================================================

struct GlobalConstant {
  char name[8];
  double value;
};

Error Context::addBuiltIns(void) {
  ContextInternalImpl* d;
  MATHPRESSO_PROPAGATE(mpContextMutable(this, &d));

  uint32_t i;

  for (i = kOpNone + 1; i < kOpCount; i++) {
    const OpInfo& op = OpInfo::get(i);
    MATHPRESSO_ASSERT(op.type == i);

    if (!op.isIntrinsic())
      continue;

    StringRef name(op.name, ::strlen(op.name));
    uint32_t hashCode = HashUtils::hashString(name.data(), name.size());

    AstSymbol* sym = d->_builder.newSymbol(name, hashCode, kAstSymbolIntrinsic, kAstScopeGlobal);
    MATHPRESSO_NULLCHECK(sym);

    sym->setDeclared();
    sym->setOpType(op.type);
    sym->setFuncArgs(op.opCount());
    sym->setFuncPtr(nullptr);

    d->_scope.putSymbol(sym);
  }

  const GlobalConstant mpGlobalConstants[] = {
    { "NaN", mpGetNan() },
    { "INF", mpGetInf() },
    { "PI" , 3.14159265358979323846 },
    { "E"  , 2.7182818284590452354 }
  };

  for (i = 0; i < MATHPRESSO_ARRAY_SIZE(mpGlobalConstants); i++) {
    const GlobalConstant& c = mpGlobalConstants[i];

    StringRef name(c.name, ::strlen(c.name));
    uint32_t hashCode = HashUtils::hashString(name.data(), name.size());

    AstSymbol* sym = d->_builder.newSymbol(name, hashCode, kAstSymbolVariable, kAstScopeGlobal);
    MATHPRESSO_NULLCHECK(sym);

    sym->setSymbolFlag(kAstSymbolIsDeclared | kAstSymbolIsAssigned | kAstSymbolIsReadOnly);
    sym->setVarSlotId(kInvalidSlot);
    sym->setVarOffset(0);
    sym->setValue(c.value);

    d->_scope.putSymbol(sym);
  }

  return kErrorOk;
}

#define MATHPRESSO_ADD_SYMBOL(name, type) \
  AstSymbol* sym; \
  { \
    size_t nameSize = strlen(name); \
    uint32_t hashCode = HashUtils::hashString(name, nameSize); \
    \
    sym = d->_scope.getSymbol(StringRef(name, nameSize), hashCode); \
    if (sym) \
      return MATHPRESSO_TRACE_ERROR(kErrorSymbolAlreadyExists); \
    \
    sym = d->_builder.newSymbol(StringRef(name, nameSize), hashCode, type, kAstScopeGlobal); \
    if (!sym) \
      return MATHPRESSO_TRACE_ERROR(kErrorNoMemory); \
    d->_scope.putSymbol(sym); \
  }

Error Context::addConstant(const char* name, double value) {
  ContextInternalImpl* d;

  MATHPRESSO_PROPAGATE(mpContextMutable(this, &d));
  MATHPRESSO_ADD_SYMBOL(name, kAstSymbolVariable);

  sym->setValue(value);
  sym->setSymbolFlag(kAstSymbolIsDeclared | kAstSymbolIsReadOnly | kAstSymbolIsAssigned);

  return kErrorOk;
}

Error Context::addVariable(const char* name, int offset, unsigned int flags) {
  ContextInternalImpl* d;

  MATHPRESSO_PROPAGATE(mpContextMutable(this, &d));
  MATHPRESSO_ADD_SYMBOL(name, kAstSymbolVariable);

  sym->setSymbolFlag(kAstSymbolIsDeclared);
  sym->setVarSlotId(kInvalidSlot);
  sym->setVarOffset(offset);

  if (flags & kVariableRO)
    sym->setSymbolFlag(kAstSymbolIsReadOnly);

  return kErrorOk;
}

Error Context::addFunction(const char* name, void* fn, unsigned int flags) {
  ContextInternalImpl* d;

  MATHPRESSO_PROPAGATE(mpContextMutable(this, &d));
  MATHPRESSO_ADD_SYMBOL(name, kAstSymbolFunction);

  sym->setSymbolFlag(kAstSymbolIsDeclared);
  sym->setFuncPtr(fn);
  // TODO: Other function flags.
  sym->setFuncArgs(flags & _kFunctionArgMask);

  return kErrorOk;
}

Error Context::delSymbol(const char* name) {
  ContextInternalImpl* d;
  MATHPRESSO_PROPAGATE(mpContextMutable(this, &d));

  size_t nlen = strlen(name);
  uint32_t hashCode = HashUtils::hashString(name, nlen);

  AstSymbol* sym = d->_scope.getSymbol(StringRef(name, nlen), hashCode);
  if (!sym)
    return MATHPRESSO_TRACE_ERROR(kErrorSymbolNotFound);

  d->_builder.deleteSymbol(d->_scope.removeSymbol(sym));
  return kErrorOk;
}

// ============================================================================
// [mathpresso::Expression - Construction / Destruction]
// ============================================================================

Expression::Expression() : _func(mpDummyFunc) {}
Expression::~Expression() { reset(); }

// ============================================================================
// [mathpresso::Expression - Interface]
// ============================================================================

Error Expression::compile(const Context& ctx, const char* body, unsigned int options, OutputLog* log) {
  // Init options first.
  options &= _kOptionsMask;

  if (log)
    options |= kInternalOptionLog;
  else
    options &= ~(kOptionVerbose | kOptionDebugAst | kOptionDebugMachineCode | kOptionDebugCompiler);

  Zone zone(32768 - Zone::kBlockOverhead);
  ZoneAllocator allocator(&zone);
  StringTmp<512> sbTmp;

  // Initialize AST.
  AstBuilder ast(&allocator);
  MATHPRESSO_PROPAGATE(ast.initProgramScope());

  ContextImpl* d = ctx._d;
  if (d != &mpContextNull)
    ast.rootScope()->shadowContextScope(&static_cast<ContextInternalImpl*>(d)->_scope);

  // Setup basic data structures used during parsing and compilation.
  size_t size = ::strlen(body);
  ErrorReporter errorReporter(body, size, options, log);

  // Parse the expression into AST.
  { MATHPRESSO_PROPAGATE(Parser(&ast, &errorReporter, body, size).parseProgram(ast.programNode())); }

  if (options & kOptionDebugAst) {
    ast.dump(sbTmp);
    log->log(OutputLog::kMessageAstInitial, 0, 0, sbTmp.data(), sbTmp.size());
    sbTmp.clear();
  }

  // Perform basic optimizations at AST level.
  { MATHPRESSO_PROPAGATE(AstOptimizer(&ast, &errorReporter).onProgram(ast.programNode())); }

  if (options & kOptionDebugAst) {
    ast.dump(sbTmp);
    log->log(OutputLog::kMessageAstFinal, 0, 0, sbTmp.data(), sbTmp.size());
    sbTmp.clear();
  }

  // Compile the function to machine code.
  CompiledFunc fn = mpCompileFunction(&ast, options, log);
  if (!fn)
    return MATHPRESSO_TRACE_ERROR(kErrorNoMemory);

  reset();
  _func = fn;

  return kErrorOk;
}

bool Expression::isCompiled() const {
  return _func != mpDummyFunc;
}

void Expression::reset() {
  // Allocated by a JIT memory manager, free it.
  if (_func != mpDummyFunc) {
    mpFreeFunction((void*)_func);
    _func = mpDummyFunc;
  }
}

// ============================================================================
// [mathpresso::OutputLog - Construction / Destruction]
// ============================================================================

OutputLog::OutputLog() {}
OutputLog::~OutputLog() {}

// ============================================================================
// [mathpresso::ErrorReporter - Interface]
// ============================================================================

void ErrorReporter::getLineAndColumn(uint32_t position, uint32_t& line, uint32_t& column) {
  // Should't happen, but be defensive.
  if (static_cast<size_t>(position) >= _size) {
    line = 0;
    column = 0;
    return;
  }

  const char* pStart = _body;
  const char* p = pStart + position;

  uint32_t x = 0;
  uint32_t y = 1;

  // Find column.
  while (p[0] != '\n') {
    x++;
    if (p == pStart)
      break;
    p--;
  }

  // Find line.
  while (p != pStart) {
    y += p[0] == '\n';
    p--;
  }

  line = y;
  column = x;
}

void ErrorReporter::onWarning(uint32_t position, const char* fmt, ...) {
  if (reportsWarnings()) {
    StringTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);

    sb.appendVFormat(fmt, ap);

    va_end(ap);
    onWarning(position, sb);
  }
}

void ErrorReporter::onWarning(uint32_t position, const String& msg) {
  if (reportsWarnings()) {
    uint32_t line, column;
    getLineAndColumn(position, line, column);
    _log->log(OutputLog::kMessageWarning, line, column, msg.data(), msg.size());
  }
}

Error ErrorReporter::onError(Error error, uint32_t position, const char* fmt, ...) {
  if (reportsErrors()) {
    StringTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);

    sb.appendVFormat(fmt, ap);

    va_end(ap);
    return onError(error, position, sb);
  }
  else {
    return MATHPRESSO_TRACE_ERROR(error);
  }
}

Error ErrorReporter::onError(Error error, uint32_t position, const String& msg) {
  if (reportsErrors()) {
    uint32_t line, column;
    getLineAndColumn(position, line, column);
    _log->log(OutputLog::kMessageError, line, column, msg.data(), msg.size());
  }

  return MATHPRESSO_TRACE_ERROR(error);
}

} // mathpresso namespace
