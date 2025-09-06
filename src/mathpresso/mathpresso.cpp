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

// MathPresso - OpInfo
// ===================

// Operator information, precedence and association. The table is mostly based on the C-language standard, but also
// adjusted to support MATHPRESSO specific operators and rules. However, the associativity and precedence should be
// fully compatible with C.
#define ROW(op_type, alt_type, params, precedence, assignment, intrinsic, flags, name) \
  { \
    uint8_t(kOp##op_type), \
    uint8_t(kOp##alt_type), \
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
const OpInfo op_info_table[kOpCount] = {
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

// MathPresso - Assertions
// =======================

void assertion_failure(const char* file, int line, const char* msg) {
  fprintf(stderr,
    "[mathpresso] Assertion failed at %s (line %d):\n"
    "[mathpresso] %s\n", file, line, msg);

  ::abort();
}

// MathPresso - Tracing
// ====================

MATHPRESSO_NOAPI Error make_error(Error error) {
  return error;
}

// MathPresso - Dummy Function
// ===========================

//! \internal
//!
//! Used instead of nullptr in `Expression::_func`.
//!
//! Returns NaN.
static void dummy_func(double* result, void*) {
  *result = mp_get_nan();
}

// MathPresso - Context Impl
// =========================

//! \internal
//!
//! Null context data.
static const ContextImpl mp_context_null = { 0 };

//! \internal
//!
//! Internal context data.
struct ContextInternalImpl : public ContextImpl {
  MATHPRESSO_INLINE ContextInternalImpl()
    : _arena(32768),
      _builder(_arena),
      _scope(&_builder, nullptr, kAstScopeGlobal) {
    mp_atomic_set(&_ref_count, 1);
  }
  MATHPRESSO_INLINE ~ContextInternalImpl() {}

  Arena _arena;
  AstBuilder _builder;
  AstScope _scope;
};

static MATHPRESSO_INLINE ContextImpl* mp_context_add_ref(ContextImpl* d) {
  if (d != &mp_context_null)
    mp_atomic_inc(&d->_ref_count);
  return d;
}

static MATHPRESSO_INLINE void mp_context_release(ContextImpl* d) {
  if (d != &mp_context_null && !mp_atomic_dec(&d->_ref_count))
    delete static_cast<ContextInternalImpl*>(d);
}

static ContextImpl* mp_context_clone(ContextImpl* other_d) {
  ContextInternalImpl* d = new(std::nothrow) ContextInternalImpl();
  if (MATHPRESSO_UNLIKELY(!d))
    return nullptr;

  if (other_d != &mp_context_null) {
    ContextInternalImpl* otherD = static_cast<ContextInternalImpl*>(other_d);
    AstSymbolHashIterator it(otherD->_scope._symbols);

    while (it.has()) {
      AstSymbol* sym = it.get();

      StringRef name(sym->_name, sym->_name_size);
      uint32_t hash_code = sym->hash_code();
      uint32_t type = sym->symbol_type();

      AstSymbol* cloned_symbol = d->_builder.new_symbol(name, hash_code, type, otherD->_scope.scope_type());
      if (MATHPRESSO_UNLIKELY(!cloned_symbol)) {
        delete d;
        return nullptr;
      }

      cloned_symbol->_symbol_flags = sym->_symbol_flags;
      switch (type) {
        case kAstSymbolVariable:
          cloned_symbol->set_var_slot_id(sym->var_slot_id());
          cloned_symbol->set_var_offset(sym->var_offset());
          cloned_symbol->_value = sym->value();
          break;

        case kAstSymbolIntrinsic:
        case kAstSymbolFunction:
          cloned_symbol->set_op_type(sym->op_type());
          cloned_symbol->set_func_args(sym->func_args());
          cloned_symbol->set_func_ptr(sym->func_ptr());
          break;

        default:
          MATHPRESSO_ASSERT_NOT_REACHED();
      }

      d->_scope.put_symbol(cloned_symbol);
      it.next();
    }
  }

  return d;
}

static Error mp_context_make_mutable(Context* self, ContextInternalImpl** out) {
  ContextImpl* d = self->_d;

  if (d != &mp_context_null && mp_atomic_get(&d->_ref_count) == 1) {
    *out = static_cast<ContextInternalImpl*>(d);
    return kErrorOk;
  }
  else {
    d = mp_context_clone(d);
    if (MATHPRESSO_UNLIKELY(!d))
      return MATHPRESSO_TRACE_ERROR(kErrorNoMemory);

    mp_context_release(
      mp_atomic_set_xchg_t<ContextImpl*>(
        &self->_d, d));

    *out = static_cast<ContextInternalImpl*>(d);
    return kErrorOk;
  }
}

// MathPresso - Context API
// ========================

Context::Context()
  : _d(const_cast<ContextImpl*>(&mp_context_null)) {}

Context::Context(const Context& other)
  : _d(mp_context_add_ref(other._d)) {}

Context::~Context() {
  mp_context_release(_d);
}

Error Context::reset() {
  mp_context_release(
    mp_atomic_set_xchg_t<ContextImpl*>(
      &_d, const_cast<ContextImpl*>(&mp_context_null)));

  return kErrorOk;
}

Context& Context::operator=(const Context& other) {
  mp_context_release(
    mp_atomic_set_xchg_t<ContextImpl*>(
      &_d, mp_context_add_ref(other._d)));
  return *this;
}

struct GlobalConstant {
  char name[8];
  double value;
};

Error Context::add_builtins(void) {
  ContextInternalImpl* d;
  MATHPRESSO_PROPAGATE(mp_context_make_mutable(this, &d));

  uint32_t i;

  for (i = kOpNone + 1; i < kOpCount; i++) {
    const OpInfo& op = OpInfo::get(i);
    MATHPRESSO_ASSERT(op.type == i);

    if (!op.is_intrinsic())
      continue;

    StringRef name(op.name, ::strlen(op.name));
    uint32_t hash_code = HashUtils::hash_string(name.data(), name.size());

    AstSymbol* sym = d->_builder.new_symbol(name, hash_code, kAstSymbolIntrinsic, kAstScopeGlobal);
    MATHPRESSO_NULLCHECK(sym);

    sym->mark_declared();
    sym->set_op_type(op.type);
    sym->set_func_args(op.op_count());
    sym->set_func_ptr(nullptr);

    d->_scope.put_symbol(sym);
  }

  const GlobalConstant global_constants_table[] = {
    { "NaN", mp_get_nan() },
    { "INF", mp_get_inf() },
    { "PI" , 3.14159265358979323846 },
    { "E"  , 2.7182818284590452354 }
  };

  for (const GlobalConstant& c : global_constants_table) {
    StringRef name(c.name, ::strlen(c.name));
    uint32_t hash_code = HashUtils::hash_string(name.data(), name.size());

    AstSymbol* sym = d->_builder.new_symbol(name, hash_code, kAstSymbolVariable, kAstScopeGlobal);
    MATHPRESSO_NULLCHECK(sym);

    sym->add_symbol_flags(kAstSymbolIsDeclared | kAstSymbolIsAssigned | kAstSymbolIsReadOnly);
    sym->set_var_slot_id(kInvalidSlot);
    sym->set_var_offset(0);
    sym->set_value(c.value);

    d->_scope.put_symbol(sym);
  }

  return kErrorOk;
}

#define MATHPRESSO_ADD_SYMBOL(name, type) \
  AstSymbol* sym; \
  { \
    size_t name_size = strlen(name); \
    uint32_t hash_code = HashUtils::hash_string(name, name_size); \
    \
    sym = d->_scope.get_symbol(StringRef(name, name_size), hash_code); \
    if (sym) \
      return MATHPRESSO_TRACE_ERROR(kErrorSymbolAlreadyExists); \
    \
    sym = d->_builder.new_symbol(StringRef(name, name_size), hash_code, type, kAstScopeGlobal); \
    if (!sym) \
      return MATHPRESSO_TRACE_ERROR(kErrorNoMemory); \
    d->_scope.put_symbol(sym); \
  }

Error Context::add_constant(const char* name, double value) {
  ContextInternalImpl* d;

  MATHPRESSO_PROPAGATE(mp_context_make_mutable(this, &d));
  MATHPRESSO_ADD_SYMBOL(name, kAstSymbolVariable);

  sym->set_value(value);
  sym->add_symbol_flags(kAstSymbolIsDeclared | kAstSymbolIsReadOnly | kAstSymbolIsAssigned);

  return kErrorOk;
}

Error Context::add_variable(const char* name, int offset, unsigned int flags) {
  ContextInternalImpl* d;

  MATHPRESSO_PROPAGATE(mp_context_make_mutable(this, &d));
  MATHPRESSO_ADD_SYMBOL(name, kAstSymbolVariable);

  sym->add_symbol_flags(kAstSymbolIsDeclared);
  sym->set_var_slot_id(kInvalidSlot);
  sym->set_var_offset(offset);

  if (flags & kVariableRO)
    sym->add_symbol_flags(kAstSymbolIsReadOnly);

  return kErrorOk;
}

Error Context::add_function(const char* name, void* fn, unsigned int flags) {
  ContextInternalImpl* d;

  MATHPRESSO_PROPAGATE(mp_context_make_mutable(this, &d));
  MATHPRESSO_ADD_SYMBOL(name, kAstSymbolFunction);

  sym->add_symbol_flags(kAstSymbolIsDeclared);
  sym->set_func_ptr(fn);
  // TODO: Other function flags.
  sym->set_func_args(flags & _kFunctionArgMask);

  return kErrorOk;
}

Error Context::del_symbol(const char* name) {
  ContextInternalImpl* d;
  MATHPRESSO_PROPAGATE(mp_context_make_mutable(this, &d));

  size_t nlen = strlen(name);
  uint32_t hash_code = HashUtils::hash_string(name, nlen);

  AstSymbol* sym = d->_scope.get_symbol(StringRef(name, nlen), hash_code);
  if (!sym)
    return MATHPRESSO_TRACE_ERROR(kErrorSymbolNotFound);

  d->_builder.delete_symbol(d->_scope.remove_symbol(sym));
  return kErrorOk;
}

// MathPresso - Expression API
// ===========================

Expression::Expression() : _func(dummy_func) {}
Expression::~Expression() { reset(); }

Error Expression::compile(const Context& ctx, const char* body, unsigned int options, OutputLog* log) {
  // Init options first.
  options &= _kOptionsMask;

  if (log)
    options |= kInternalOptionLog;
  else
    options &= ~(kOptionVerbose | kOptionDebugAst | kOptionDebugMachineCode | kOptionDebugCompiler);

  Arena arena(32768);
  StringTmp<512> sb_tmp;

  // Initialize AST.
  AstBuilder ast(arena);
  MATHPRESSO_PROPAGATE(ast.init_program_scope());

  ContextImpl* d = ctx._d;
  if (d != &mp_context_null)
    ast.root_scope()->shadow_context_scope(&static_cast<ContextInternalImpl*>(d)->_scope);

  // Setup basic data structures used during parsing and compilation.
  size_t size = ::strlen(body);
  ErrorReporter error_reporter(body, size, options, log);

  // Parse the expression into AST.
  { MATHPRESSO_PROPAGATE(Parser(&ast, &error_reporter, body, size).parse_program(ast.program_node())); }

  if (options & kOptionDebugAst) {
    ast.dump(sb_tmp);
    log->log(OutputLog::kMessageAstInitial, 0, 0, sb_tmp.data(), sb_tmp.size());
    sb_tmp.clear();
  }

  // Perform basic optimizations at AST level.
  { MATHPRESSO_PROPAGATE(AstOptimizer(&ast, &error_reporter).on_program(ast.program_node())); }

  if (options & kOptionDebugAst) {
    ast.dump(sb_tmp);
    log->log(OutputLog::kMessageAstFinal, 0, 0, sb_tmp.data(), sb_tmp.size());
    sb_tmp.clear();
  }

  // Compile the function to machine code.
  CompiledFunc fn = compile_function(&ast, options, log);
  if (!fn)
    return MATHPRESSO_TRACE_ERROR(kErrorNoMemory);

  reset();
  _func = fn;

  return kErrorOk;
}

bool Expression::is_compiled() const {
  return _func != dummy_func;
}

void Expression::reset() {
  // Allocated by a JIT memory manager, free it.
  if (_func != dummy_func) {
    free_compiled_function((void*)_func);
    _func = dummy_func;
  }
}

// MathPresso - OutputLog - API
// ============================

OutputLog::OutputLog() {}
OutputLog::~OutputLog() {}

// MathPresso - Error Reporter API
// ===============================

void ErrorReporter::get_line_and_column(uint32_t position, uint32_t& line, uint32_t& column) {
  // Should't happen, but be defensive.
  if (static_cast<size_t>(position) >= _size) {
    line = 0;
    column = 0;
    return;
  }

  const char* ptr_start = _body;
  const char* ptr = ptr_start + position;

  uint32_t x = 0;
  uint32_t y = 1;

  // Find column.
  while (ptr[0] != '\n') {
    x++;
    if (ptr == ptr_start)
      break;
    ptr--;
  }

  // Find line.
  while (ptr != ptr_start) {
    y += ptr[0] == '\n';
    ptr--;
  }

  line = y;
  column = x;
}

void ErrorReporter::on_warning(uint32_t position, const char* fmt, ...) {
  if (does_report_warnings()) {
    StringTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);

    sb.append_vformat(fmt, ap);

    va_end(ap);
    on_warning(position, sb);
  }
}

void ErrorReporter::on_warning(uint32_t position, const String& msg) {
  if (does_report_warnings()) {
    uint32_t line, column;
    get_line_and_column(position, line, column);
    _log->log(OutputLog::kMessageWarning, line, column, msg.data(), msg.size());
  }
}

Error ErrorReporter::on_error(Error error, uint32_t position, const char* fmt, ...) {
  if (does_report_errors()) {
    StringTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);

    sb.append_vformat(fmt, ap);

    va_end(ap);
    return on_error(error, position, sb);
  }
  else {
    return MATHPRESSO_TRACE_ERROR(error);
  }
}

Error ErrorReporter::on_error(Error error, uint32_t position, const String& msg) {
  if (does_report_errors()) {
    uint32_t line, column;
    get_line_and_column(position, line, column);
    _log->log(OutputLog::kMessageError, line, column, msg.data(), msg.size());
  }

  return MATHPRESSO_TRACE_ERROR(error);
}

} // {mathpresso}
