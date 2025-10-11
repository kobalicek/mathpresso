// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mpast_p.h"
#include "./mpcompiler_p.h"
#include "./mpeval_p.h"

#include <asmjit/ujit.h>

namespace mathpresso {

using namespace asmjit;

// MathPresso - JIT Globals
// ========================

struct JitGlobal {
  JitRuntime runtime;
};
static JitGlobal jit_global;

// MathPresso - JIT Error Handler
// ==============================

class JitErrorHandler : public ErrorHandler {
public:
  asmjit::Error _err {};

  void handle_error(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    Support::maybe_unused(origin);

    _err = err;
    fprintf(stderr, "AsmJit error: %s\n", message);
  }
};

// MathPresso - JIT Utilities
// ==========================

struct JitUtils {
  static void* func_by_op(uint32_t op) {
    switch (op) {
      case kOpIsNan        : return (void*)(Arg1Func)mp_is_nan;
      case kOpIsInf        : return (void*)(Arg1Func)mp_is_inf;
      case kOpIsFinite     : return (void*)(Arg1Func)mp_is_finite;
      case kOpSignBit      : return (void*)(Arg1Func)mp_sign_bit;

      case kOpTrunc        : return (void*)(Arg1Func)trunc;
      case kOpFloor        : return (void*)(Arg1Func)floor;
      case kOpCeil         : return (void*)(Arg1Func)ceil;
      case kOpRoundEven    : return (void*)(Arg1Func)rint;
      case kOpRoundHalfAway: return (void*)(Arg1Func)mp_round_half_away;
      case kOpRoundHalfUp  : return (void*)(Arg1Func)mp_round_half_up;

      case kOpAbs          : return (void*)(Arg1Func)fabs;
      case kOpExp          : return (void*)(Arg1Func)exp;
      case kOpLog          : return (void*)(Arg1Func)log;
      case kOpLog2         : return (void*)(Arg1Func)log2;
      case kOpLog10        : return (void*)(Arg1Func)log10;
      case kOpSqrt         : return (void*)(Arg1Func)sqrt;
      case kOpFrac         : return (void*)(Arg1Func)mp_frac;
      case kOpRecip        : return (void*)(Arg1Func)mp_recip;

      case kOpSin          : return (void*)(Arg1Func)sin;
      case kOpCos          : return (void*)(Arg1Func)cos;
      case kOpTan          : return (void*)(Arg1Func)tan;
      case kOpSinh         : return (void*)(Arg1Func)sinh;
      case kOpCosh         : return (void*)(Arg1Func)cosh;
      case kOpTanh         : return (void*)(Arg1Func)tanh;
      case kOpAsin         : return (void*)(Arg1Func)asin;
      case kOpAcos         : return (void*)(Arg1Func)acos;
      case kOpAtan         : return (void*)(Arg1Func)atan;

      case kOpAvg          : return (void*)(Arg2Func)mp_avg;
      case kOpMin          : return (void*)(Arg2Func)mp_min<double>;
      case kOpMax          : return (void*)(Arg2Func)mp_max<double>;
      case kOpPow          : return (void*)(Arg2Func)pow;
      case kOpAtan2        : return (void*)(Arg2Func)atan2;
      case kOpHypot        : return (void*)(Arg2Func)hypot;
      case kOpCopySign     : return (void*)(Arg2Func)mp_copy_sign;

      default:
        MATHPRESSO_ASSERT_NOT_REACHED();
        return nullptr;
    }
  }
};

// MathPresso - JIT Variable
// =========================

struct MATHPRESSO_NOAPI JitVar {
  enum FLAGS {
    FLAG_NONE = 0,
    FLAG_RO = 1
  };

  inline JitVar() noexcept
    : _op(),
      _flags(FLAG_NONE) {}

  inline JitVar(Operand op, uint32_t flags) noexcept
    : _op(op),
      _flags(flags) {}

  inline JitVar(const JitVar& other) noexcept = default;
  inline JitVar& operator=(const JitVar& other) noexcept = default;

  inline void reset() noexcept {
    _op.reset();
    _flags = FLAG_NONE;
  }

  inline void swap_with(JitVar& other) noexcept {
    JitVar t(*this);
    *this = other;
    other = t;
  }

  inline bool is_none() const noexcept { return _op.is_none(); }
  inline bool is_mem() const noexcept { return _op.is_mem(); }
  inline bool is_vec() const noexcept { return _op.is_vec(); }

  inline const Operand& op() const noexcept { return _op; }
  inline const ujit::Mem& mem() const noexcept { return _op.as<ujit::Mem>(); }
  inline const ujit::Vec& vec() const noexcept { return _op.as<ujit::Vec>(); }

  inline uint32_t flags() const noexcept { return _flags; }
  inline bool isRO() const noexcept { return (_flags & FLAG_RO) != 0; }
  inline void setRO() noexcept { _flags |= FLAG_RO; }
  inline void clearRO() noexcept { _flags &= ~FLAG_RO; }

  // Members.
  Operand _op;
  uint32_t _flags;
};

// MathPresso - JIT Compiler
// =========================

struct MATHPRESSO_NOAPI JitCompiler {
  Arena& arena;
  ujit::UniCompiler uc;

  ujit::Gp var_ptr;
  ujit::Gp result_ptr;

  JitVar* var_slots = nullptr;
  BaseNode* func_body = nullptr;
  ConstPoolNode* const_pool = nullptr;

  JitCompiler(Arena& arena, ujit::BackendCompiler& cc, const CpuFeatures& cpu_features, CpuHints cpu_hints);
  ~JitCompiler();

  // Function Generator.
  void begin_function();
  void end_function();

  // Variable Management.
  JitVar copy_var(const JitVar& other, uint32_t flags);
  JitVar writable_var(const JitVar& other);
  JitVar register_var(const JitVar& other);

  // Compiler.
  void compile(AstBlock* node, AstScope* root_scope, uint32_t num_slots);

  JitVar on_node(AstNode* node);
  JitVar on_block(AstBlock* node);
  JitVar on_var_decl(AstVarDecl* node);
  JitVar on_var(AstVar* node);
  JitVar on_imm(AstImm* node);
  JitVar on_unary_op(AstUnaryOp* node);
  JitVar on_binary_op(AstBinaryOp* node);
  JitVar on_invoke(AstCall* node);

  // Helpers.
  void inline_invoke(const ujit::Vec& dst, const ujit::Vec* args, uint32_t count, void* fn);

  // Constants.
  void prepare_const_pool();
  JitVar get_constant_u64(uint64_t value);
  JitVar get_constant_u64_as_f64x2(uint64_t value);
  JitVar get_constant_u64_aligned(uint64_t value);
  JitVar get_constant_f64(double value);
  JitVar get_constant_f64_as_f64x2(double value);
  JitVar get_constant_f64_aligned(double value);
};

JitCompiler::JitCompiler(Arena& arena, ujit::BackendCompiler& cc, const CpuFeatures& cpu_features, CpuHints cpu_hints)
  : arena(arena),
    uc(&cc, cpu_features, cpu_hints),
    var_slots(nullptr),
    func_body(nullptr) {}

JitCompiler::~JitCompiler() {}

void JitCompiler::begin_function() {
  FuncNode* func_node = uc.cc->new_func(FuncSignature::build<void, double*, double*>(CallConvId::kCDecl));
  uc.init_function(func_node);

  var_ptr = uc.new_gpz("var_ptr");
  result_ptr = uc.new_gpz("result_ptr");

  func_node->set_arg(0, result_ptr);
  func_node->set_arg(1, var_ptr);
  func_body = uc.cc->cursor();
}

void JitCompiler::end_function() {
  uc.cc->end_func();
  if (const_pool) {
    uc.cc->add_node(const_pool);
  }
}

JitVar JitCompiler::copy_var(const JitVar& other, uint32_t flags) {
  JitVar v(uc.new_vec128_f64x1(), flags);

  if (other.is_vec()) {
    uc.v_mov(v.vec(), other.vec());
  }
  else if (other.is_mem()) {
    uc.v_loadu64_f64(v.vec(), other.mem());
  }
  else {
    MATHPRESSO_ASSERT_NOT_REACHED();
  }

  return v;
}

JitVar JitCompiler::writable_var(const JitVar& other) {
  if (other.is_mem() || other.isRO())
    return copy_var(other, other.flags() & ~JitVar::FLAG_RO);
  else
    return other;
}

JitVar JitCompiler::register_var(const JitVar& other) {
  if (other.is_mem())
    return copy_var(other, other.flags());
  else
    return other;
}

void JitCompiler::compile(AstBlock* node, AstScope* root_scope, uint32_t num_slots) {
  if (num_slots != 0) {
    var_slots = static_cast<JitVar*>(arena.alloc_reusable(Arena::aligned_size(sizeof(JitVar) * num_slots)));
    if (var_slots == nullptr) {
      return;
    }

    for (uint32_t i = 0; i < num_slots; i++) {
      var_slots[i] = JitVar();
    }
  }

  // Result of the function or NaN.
  JitVar result = on_block(node);

  // Write altered global variables.
  {
    AstSymbolHashIterator it(root_scope->symbols());
    while (it.has()) {
      AstSymbol* sym = it.get();
      if (sym->is_global() && sym->is_altered()) {
        JitVar v = var_slots[sym->var_slot_id()];
        uc.v_storeu64_f64(ujit::mem_ptr(var_ptr, sym->var_offset()), register_var(v).vec());
      }

      it.next();
    }
  }

  // Return NaN if no result is given.
  ujit::Vec var;
  if (result.is_none())
    var = register_var(get_constant_f64(mp_get_nan())).vec();
  else
    var = register_var(result).vec();
  uc.v_storeu64_f64(ujit::mem_ptr(result_ptr), var);

  if (num_slots != 0) {
    arena.free_reusable(var_slots, sizeof(JitVar) * num_slots);
  }
}

JitVar JitCompiler::on_node(AstNode* node) {
  switch (node->node_type()) {
    case kAstNodeBlock    : return on_block    (static_cast<AstBlock*   >(node));
    case kAstNodeVarDecl  : return on_var_decl (static_cast<AstVarDecl* >(node));
    case kAstNodeVar      : return on_var      (static_cast<AstVar*     >(node));
    case kAstNodeImm      : return on_imm      (static_cast<AstImm*     >(node));
    case kAstNodeUnaryOp  : return on_unary_op (static_cast<AstUnaryOp* >(node));
    case kAstNodeBinaryOp : return on_binary_op(static_cast<AstBinaryOp*>(node));
    case kAstNodeCall     : return on_invoke   (static_cast<AstCall*    >(node));

    default:
      MATHPRESSO_ASSERT_NOT_REACHED();
      return JitVar();
  }
}

JitVar JitCompiler::on_block(AstBlock* node) {
  JitVar result;
  uint32_t i, size = node->size();

  for (i = 0; i < size; i++) {
    result = on_node(node->child_at(i));
  }

  // Return the last result (or no result if the block is empty).
  return result;
}

JitVar JitCompiler::on_var_decl(AstVarDecl* node) {
  JitVar result;

  if (node->child())
    result = on_node(node->child());

  AstSymbol* sym = node->symbol();
  uint32_t slot_id = sym->var_slot_id();

  result.setRO();
  var_slots[slot_id] = result;

  return result;
}

JitVar JitCompiler::on_var(AstVar* node) {
  AstSymbol* sym = node->symbol();
  uint32_t slot_id = sym->var_slot_id();

  JitVar result = var_slots[slot_id];
  if (result.is_none()) {
    if (sym->is_global()) {
      result = JitVar(ujit::mem_ptr(var_ptr, sym->var_offset()), JitVar::FLAG_RO);
      var_slots[slot_id] = result;
      if (sym->write_count() > 0) {
        result = copy_var(result, JitVar::FLAG_NONE);
      }
    }
    else {
      result = get_constant_f64(mp_get_nan());
      var_slots[slot_id] = result;
    }
  }

  return result;
}

JitVar JitCompiler::on_imm(AstImm* node) {
  return get_constant_f64(node->value());
}

JitVar JitCompiler::on_unary_op(AstUnaryOp* node) {
  uint32_t op = node->op_type();

  JitVar var = on_node(node->child());
  ujit::Vec result = uc.new_vec128_f64x1();

  switch (op) {
    case kOpNone:
      return var;

    case kOpNeg: uc.s_neg_f64(result, var.op()); break;
    case kOpNot: uc.v_not_f64(result, register_var(var).op()); break;

    case kOpIsNan: {
      var = register_var(var);
      uc.s_cmp_eq_f64(result, var.vec(), var.vec());
      uc.v_andn_f64(result, result, get_constant_f64_as_f64x2(1.0).op());
      break;
    }

    case kOpIsInf: {
      var = register_var(var);
      uc.v_or_f64(result, var.vec(), get_constant_u64_as_f64x2(0x8000000000000000u).op());
      uc.s_cmp_eq_f64(result, result, get_constant_f64(0xFFF0000000000000u).op());
      uc.v_and_f64(result, result, get_constant_f64_as_f64x2(1.0).op());
      break;
    }

    case kOpIsFinite: {
      var = register_var(var);
      uc.v_or_f64(result, var.vec(), get_constant_u64_as_f64x2(0x8000000000000000u).op());
      uc.s_cmp_le_f64(result, result, get_constant_f64(0.0).op());
      uc.v_and_f64(result, result, get_constant_f64_as_f64x2(1.0).op());
      break;
    }

    case kOpSignBit: {
      uc.v_srai_i64(result, var.op(), 63);
      uc.v_and_f64(result, result, get_constant_f64_as_f64x2(1.0).op());
      break;
    }

    case kOpTrunc        : uc.s_trunc_f64(result, var.op()); break;
    case kOpFloor        : uc.s_floor_f64(result, var.op()); break;
    case kOpCeil         : uc.s_ceil_f64(result, var.op()); break;
    case kOpRoundEven    : uc.s_round_even_f64(result, var.op()); break;
    case kOpRoundHalfAway: uc.s_round_half_away_f64(result, var.op()); break;
    case kOpRoundHalfUp  : uc.s_round_half_up_f64(result, var.op()); break;

    case kOpAbs: uc.s_abs_f64(result, var.op()); break;
    case kOpSqrt: uc.s_sqrt_f64(result, var.op()); break;

    case kOpFrac: {
      ujit::Vec tmp = uc.new_vec128_f64x1();
      var = register_var(var);

      uc.s_floor_f64(tmp, var.vec());
      uc.s_sub_f64(result, var.vec(), tmp);
      break;
    }

    case kOpRecip: {
      uc.v_loada64_f64(result, get_constant_f64(1.0).mem());
      uc.s_div_f64(result, result, var.op());
      break;
    }

    default: {
      // No inline implementation -> function call.
      ujit::Vec args[1] = { register_var(var).vec() };
      inline_invoke(result, args, 1, JitUtils::func_by_op(op));
      break;
    }
  }

  return JitVar(result, JitVar::FLAG_NONE);;
}

JitVar JitCompiler::on_binary_op(AstBinaryOp* node) {
  uint32_t op = node->op_type();

  AstNode* left  = node->left();
  AstNode* right = node->right();

  // Compile assignment.
  if (op == kOpAssign) {
    AstVar* var_node = reinterpret_cast<AstVar*>(left);
    MATHPRESSO_ASSERT(var_node->node_type() == kAstNodeVar);

    AstSymbol* sym = var_node->symbol();
    uint32_t slot_id = sym->var_slot_id();

    JitVar result = on_node(right);
    result.setRO();

    sym->mark_altered();
    var_slots[slot_id] = result;

    return result;
  }

  // Handle the case that the operands are the same variable.
  JitVar vl, vr;
  if (left->node_type() == kAstNodeVar &&
      right->node_type() == kAstNodeVar &&
      static_cast<AstVar*>(left)->symbol() == static_cast<AstVar*>(right)->symbol()) {
    vl = vr = writable_var(on_node(node->left()));
  }
  else {
    vl = on_node(node->left());
    vr = on_node(node->right());

    // Commutativity.
    if (op == kOpAdd || op == kOpMul || op == kOpAvg || op == kOpMin || op == kOpMax) {
      if (vl.isRO() && !vr.isRO()) {
        vl.swap_with(vr);
      }
    }

    vl = writable_var(vl);
  }

  ujit::Vec result = uc.new_vec128_f64x1();

  switch (op) {
    case kOpEq: uc.s_cmp_eq_f64(result, register_var(vl).vec(), vr.op()); uc.v_and_f64(result, result, uc.simd_const(&uc.ct().f64_1, ujit::Bcst::k64, result)); break;
    case kOpNe: uc.s_cmp_ne_f64(result, register_var(vl).vec(), vr.op()); uc.v_and_f64(result, result, uc.simd_const(&uc.ct().f64_1, ujit::Bcst::k64, result)); break;
    case kOpGt: uc.s_cmp_gt_f64(result, register_var(vl).vec(), vr.op()); uc.v_and_f64(result, result, uc.simd_const(&uc.ct().f64_1, ujit::Bcst::k64, result)); break;
    case kOpGe: uc.s_cmp_ge_f64(result, register_var(vl).vec(), vr.op()); uc.v_and_f64(result, result, uc.simd_const(&uc.ct().f64_1, ujit::Bcst::k64, result)); break;
    case kOpLt: uc.s_cmp_lt_f64(result, register_var(vl).vec(), vr.op()); uc.v_and_f64(result, result, uc.simd_const(&uc.ct().f64_1, ujit::Bcst::k64, result)); break;
    case kOpLe: uc.s_cmp_le_f64(result, register_var(vl).vec(), vr.op()); uc.v_and_f64(result, result, uc.simd_const(&uc.ct().f64_1, ujit::Bcst::k64, result)); break;

    case kOpAdd: uc.s_add_f64(result, register_var(vl).vec(), vr.op()); break;
    case kOpSub: uc.s_sub_f64(result, register_var(vl).vec(), vr.op()); break;
    case kOpMul: uc.s_mul_f64(result, register_var(vl).vec(), vr.op()); break;
    case kOpDiv: uc.s_div_f64(result, register_var(vl).vec(), vr.op()); break;

    case kOpMod: {
      vl = register_var(vl);
      vr = register_var(vr);
      uc.s_mod_f64(result, vl.vec(), vr.vec());
      break;
    }

    case kOpAvg: {
      uc.s_add_f64(result, register_var(vl).vec(), vr.op());
      uc.s_mul_f64(result, result, get_constant_f64(0.5).op());
      break;
    }

    case kOpMin: uc.s_min_f64(result, register_var(vl).vec(), vr.op()); break;
    case kOpMax: uc.s_max_f64(result, register_var(vl).vec(), vr.op()); break;

    case kOpCopySign: {
      ujit::Vec tmp = uc.new_vec128_f64x1();
      vl = writable_var(vl);
      vr = writable_var(vr);

      uc.v_and_f64(result, vl.vec(), get_constant_u64_as_f64x2(0x7FFFFFFFFFFFFFFFu).mem());
      uc.v_and_f64(tmp, vr.vec(), get_constant_u64_as_f64x2(0x8000000000000000u).mem());
      uc.v_or_f64(result, result, tmp);

      return JitVar(result, JitVar::FLAG_NONE);
    }

    default: {
      // No inline implementation -> function call.
      ujit::Vec args[2] = { register_var(vl).vec(), register_var(vr).vec() };
      inline_invoke(result, args, 2, JitUtils::func_by_op(op));

      return JitVar(result, JitVar::FLAG_NONE);
    }
  }

  return JitVar(result, JitVar::FLAG_NONE);
}

JitVar JitCompiler::on_invoke(AstCall* node) {
  uint32_t i, size = node->size();
  AstSymbol* sym = node->symbol();

  ujit::Vec result = uc.new_vec128_f64x1();
  ujit::Vec args[8];
  MATHPRESSO_ASSERT(size <= 8);

  for (i = 0; i < size; i++) {
    args[i] = register_var(on_node(node->child_at(i))).vec();
  }

  inline_invoke(result, args, size, sym->func_ptr());
  return JitVar(result, JitVar::FLAG_NONE);
}

void JitCompiler::inline_invoke(const ujit::Vec& dst, const ujit::Vec* args, uint32_t count, void* fn) {
  uint32_t i;

  // Use function builder to build a function prototype.
  FuncSignature signature;
  signature.set_ret_t<double>();

  for (i = 0; i < count; i++) {
    signature.add_arg_t<double>();
  }

  // Create the function call.
  InvokeNode* invoke_node;

#if defined(ASMJIT_UJIT_AARCH64)
  ujit::Gp func_ptr = uc.new_gp_ptr("func_ptr");
  uc.mov(func_ptr, (uint64_t)fn);
  uc.cc->invoke(asmjit::Out(invoke_node), func_ptr, signature);
#else
  uc.cc->invoke(asmjit::Out(invoke_node), (uint64_t)fn, signature);
#endif
  invoke_node->set_ret(0, dst);

  for (i = 0; i < count; i++) {
    invoke_node->set_arg(i, args[i]);
  }
}

void JitCompiler::prepare_const_pool() {
  if (!const_pool) {
    uc.cc->new_const_pool_node(asmjit::Out(const_pool));
  }
}

JitVar JitCompiler::get_constant_u64(uint64_t value) {
  prepare_const_pool();

  size_t offset;
  if (const_pool->add(&value, sizeof(uint64_t), asmjit::Out(offset)) != asmjit::Error::kOk)
    return JitVar();

  return JitVar(ujit::mem_ptr(const_pool->label(), static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::get_constant_u64_as_f64x2(uint64_t value) {
  prepare_const_pool();

  uint64_t data[2] = { value, 0 };
  size_t offset;

  if (const_pool->add(data, sizeof(data), asmjit::Out(offset)) != asmjit::Error::kOk)
    return JitVar();

  return JitVar(ujit::mem_ptr(const_pool->label(), static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::get_constant_u64_aligned(uint64_t value) {
  prepare_const_pool();

  uint64_t two[2] = { value, value };

  size_t offset;
  if (const_pool->add(two, sizeof(uint64_t) * 2, asmjit::Out(offset)) != asmjit::Error::kOk)
    return JitVar();

  return JitVar(ujit::mem_ptr(const_pool->label(), static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::get_constant_f64(double value) {
  DoubleBits bits;
  bits.d = value;
  return get_constant_u64(bits.u);
}

JitVar JitCompiler::get_constant_f64_as_f64x2(double value) {
  DoubleBits bits;
  bits.d = value;
  return get_constant_u64_as_f64x2(bits.u);
}

JitVar JitCompiler::get_constant_f64_aligned(double value) {
  DoubleBits bits;
  bits.d = value;
  return get_constant_u64_aligned(bits.u);
}

CompiledFunc compile_function(AstBuilder* ast, [[maybe_unused]] uint32_t options, OutputLog* log) {
  StringLogger logger;
  CpuFeatures features = jit_global.runtime.cpu_features();

#if defined(ASMJIT_UJIT_X86)
  if ((options & (kOptionDisableAVX512 | kOptionDisableAVX | kOptionDisableSSE4_1)) != 0) {
    features.x86().remove_avx512();
  }

  if ((options & (kOptionDisableAVX | kOptionDisableSSE4_1)) != 0) {
    features.x86().remove_avx();
  }

  if ((options & kOptionDisableSSE4_1) != 0) {
    features.remove(CpuFeatures::X86::kSSE4_1);
    features.remove(CpuFeatures::X86::kSSE4_2);
  }
#endif

  JitErrorHandler eh;
  CodeHolder code;

  code.init(jit_global.runtime.environment(), features);
  code.set_error_handler(&eh);

  ujit::BackendCompiler cc(&code);

  bool debug_machine_code = log != nullptr && (options & kOptionDebugMachineCode) != 0;
  bool debug_compiler     = log != nullptr && (options & kOptionDebugCompiler   ) != 0;

  if (debug_machine_code || debug_compiler) {
    logger.add_flags(FormatFlags::kMachineCode | FormatFlags::kRegCasts | FormatFlags::kExplainImms);
    code.set_logger(&logger);

    if (debug_compiler) {
      cc.add_diagnostic_options(DiagnosticOptions::kRAAnnotate | DiagnosticOptions::kRADebugAll);
    }
  }

  {
    JitCompiler jit_compiler(ast->arena(), cc, features, CpuInfo::recalculate_hints(CpuInfo::host(), features));
    jit_compiler.begin_function();
    jit_compiler.compile(ast->program_node(), ast->root_scope(), ast->_num_slots);
    jit_compiler.end_function();
  }

  if (cc.finalize() != asmjit::Error::kOk) {
    return nullptr;
  }

  CompiledFunc fn;
  if (jit_global.runtime.add(&fn, &code) != asmjit::Error::kOk) {
    return nullptr;
  }

  if (debug_machine_code || debug_compiler) {
    log->log(OutputLog::kMessageAsm, 0, 0, logger.data(), logger.data_size());
  }

  return fn;
}

void free_compiled_function(void* fn) {
  jit_global.runtime.release(fn);
}

} // {mathpresso}
