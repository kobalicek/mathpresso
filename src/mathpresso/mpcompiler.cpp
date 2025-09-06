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

namespace mathpresso {

using namespace asmjit;

// MathPresso - JIT Globals
// ========================

struct JitGlobal {
  JitRuntime runtime;
};
static JitGlobal jit_global;

// MathPresso - JIT Utilities
// ==========================

struct JitUtils {
  static void* func_by_op(uint32_t op) {
    switch (op) {
      case kOpIsNan    : return (void*)(Arg1Func)mp_is_nan;
      case kOpIsInf    : return (void*)(Arg1Func)mp_is_inf;
      case kOpIsFinite : return (void*)(Arg1Func)mp_is_finite;
      case kOpSignBit  : return (void*)(Arg1Func)mp_sign_bit;

      case kOpRound    : return (void*)(Arg1Func)mp_round;
      case kOpRoundEven: return (void*)(Arg1Func)rint;
      case kOpTrunc    : return (void*)(Arg1Func)trunc;
      case kOpFloor    : return (void*)(Arg1Func)floor;
      case kOpCeil     : return (void*)(Arg1Func)ceil;

      case kOpAbs      : return (void*)(Arg1Func)fabs;
      case kOpExp      : return (void*)(Arg1Func)exp;
      case kOpLog      : return (void*)(Arg1Func)log;
      case kOpLog2     : return (void*)(Arg1Func)log2;
      case kOpLog10    : return (void*)(Arg1Func)log10;
      case kOpSqrt     : return (void*)(Arg1Func)sqrt;
      case kOpFrac     : return (void*)(Arg1Func)mp_frac;
      case kOpRecip    : return (void*)(Arg1Func)mp_recip;

      case kOpSin      : return (void*)(Arg1Func)sin;
      case kOpCos      : return (void*)(Arg1Func)cos;
      case kOpTan      : return (void*)(Arg1Func)tan;
      case kOpSinh     : return (void*)(Arg1Func)sinh;
      case kOpCosh     : return (void*)(Arg1Func)cosh;
      case kOpTanh     : return (void*)(Arg1Func)tanh;
      case kOpAsin     : return (void*)(Arg1Func)asin;
      case kOpAcos     : return (void*)(Arg1Func)acos;
      case kOpAtan     : return (void*)(Arg1Func)atan;

      case kOpAvg      : return (void*)(Arg2Func)mp_avg;
      case kOpMin      : return (void*)(Arg2Func)mp_min<double>;
      case kOpMax      : return (void*)(Arg2Func)mp_max<double>;
      case kOpPow      : return (void*)(Arg2Func)pow;
      case kOpAtan2    : return (void*)(Arg2Func)atan2;
      case kOpHypot    : return (void*)(Arg2Func)hypot;
      case kOpCopySign : return (void*)(Arg2Func)mp_copy_sign;

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
  inline bool is_vec() const noexcept { return _op.is_reg(RegType::kVec128); }

  inline const Operand& op() const noexcept { return _op; }
  inline const x86::Mem& mem() const noexcept { return _op.as<x86::Mem>(); }
  inline const x86::Vec& vec() const noexcept { return _op.as<x86::Vec>(); }

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
  x86::Compiler* cc = nullptr;

  x86::Gp var_ptr;
  x86::Gp const_ptr;
  x86::Gp result_ptr;

  JitVar* var_slots = nullptr;
  BaseNode* func_body = nullptr;
  ConstPoolNode* const_pool = nullptr;

  bool enable_sse4_1 = false;

  JitCompiler(Arena& arena, x86::Compiler* c);
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
  void inline_round(const x86::Vec& dst, const x86::Vec& src, uint32_t op);
  void inline_invoke(const x86::Vec& dst, const x86::Vec* args, uint32_t count, void* fn);

  // Constants.
  void prepare_const_pool();
  JitVar get_constant_u64(uint64_t value);
  JitVar get_constant_u64_as_2xf64(uint64_t value);
  JitVar get_constant_u64_aligned(uint64_t value);
  JitVar get_constant_f64(double value);
  JitVar get_constant_f64_as_2xf64(double value);
  JitVar get_constant_f64_aligned(double value);
};

JitCompiler::JitCompiler(Arena& arena, x86::Compiler* cc)
  : arena(arena),
    cc(cc),
    var_slots(nullptr),
    func_body(nullptr) {
  const CpuFeatures::X86& features = CpuInfo::host().features().x86();
  enable_sse4_1 = features.has_sse4_1();
}

JitCompiler::~JitCompiler() {}

void JitCompiler::begin_function() {
  FuncNode* func_node = cc->add_func(FuncSignature::build<void, double*, double*>(CallConvId::kCDecl));

  var_ptr = cc->new_gpz("var_ptr");
  const_ptr = cc->new_gpz("const_ptr");
  result_ptr = cc->new_gpz("result_ptr");

  func_node->set_arg(0, result_ptr);
  func_node->set_arg(1, var_ptr);
  func_body = cc->cursor();
}

void JitCompiler::end_function() {
  cc->end_func();
  if (const_pool)
    cc->add_node(const_pool);
}

JitVar JitCompiler::copy_var(const JitVar& other, uint32_t flags) {
  JitVar v(cc->new_xmm_sd(), flags);
  cc->emit(other.is_vec() ? x86::Inst::kIdMovapd : x86::Inst::kIdMovsd, v.vec(), other.op());
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
        cc->emit(x86::Inst::kIdMovsd, x86::ptr(var_ptr, sym->var_offset()), register_var(v).vec());
      }

      it.next();
    }
  }

  // Return NaN if no result is given.
  x86::Vec var;
  if (result.is_none())
    var = register_var(get_constant_f64(mp_get_nan())).vec();
  else
    var = register_var(result).vec();
  cc->movsd(x86::ptr(result_ptr), var);

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

  for (i = 0; i < size; i++)
    result = on_node(node->child_at(i));

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
      result = JitVar(x86::ptr(var_ptr, sym->var_offset()), JitVar::FLAG_RO);
      var_slots[slot_id] = result;
      if (sym->write_count() > 0)
        result = copy_var(result, JitVar::FLAG_NONE);
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

  switch (op) {
    case kOpNone:
      return var;

    case kOpNeg: {
      x86::Vec result = cc->new_xmm_sd();
      cc->emit(x86::Inst::kIdXorpd, result, result);
      cc->emit(x86::Inst::kIdSubsd, result, var.op());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpNot: {
      var = writable_var(var);
      cc->cmpsd(var.vec(), get_constant_f64_as_2xf64(0.0).mem(), x86::CmpImm::kEQ);
      cc->andpd(var.vec(), get_constant_f64_as_2xf64(1.0).mem());
      return var;
    }

    case kOpIsNan: {
      var = writable_var(var);
      cc->cmpsd(var.vec(), var.vec(), x86::CmpImm::kEQ);
      cc->andnpd(var.vec(), get_constant_f64_as_2xf64(1.0).mem());
      return var;
    }

    case kOpIsInf: {
      var = writable_var(var);
      cc->orpd(var.vec(), get_constant_u64_as_2xf64(0x8000000000000000u).mem());
      cc->cmpsd(var.vec(), get_constant_u64(0xFFF0000000000000u).mem(), x86::CmpImm::kEQ);
      cc->andpd(var.vec(), get_constant_f64_as_2xf64(1.0).mem());
      return var;
    }

    case kOpIsFinite: {
      var = writable_var(var);
      cc->orpd(var.vec(), get_constant_u64_as_2xf64(0x8000000000000000u).mem());
      cc->cmpsd(var.vec(), get_constant_f64(0.0).mem(), x86::CmpImm::kLE);
      cc->andpd(var.vec(), get_constant_f64_as_2xf64(1.0).mem());
      return var;
    }

    case kOpSignBit: {
      x86::Vec result = cc->new_xmm_sd();
      cc->pshufd(result, register_var(var).vec(), x86::shuffle_imm(3, 2, 1, 1));
      cc->psrad(result, 31);
      cc->andpd(result, get_constant_f64_as_2xf64(1.0).mem());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpRound:
    case kOpRoundEven:
    case kOpTrunc:
    case kOpFloor:
    case kOpCeil: {
      var = writable_var(var);
      inline_round(var.vec(), var.vec(), op);
      return var;
    }

    case kOpAbs: {
      x86::Vec result = cc->new_xmm_sd();
      cc->emit(x86::Inst::kIdXorpd, result, result);
      cc->emit(x86::Inst::kIdSubsd, result, var.op());
      cc->emit(x86::Inst::kIdMaxsd, result, var.op());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpExp:
    case kOpLog:
    case kOpLog2:
    case kOpLog10:
      break;

    case kOpSqrt: {
      x86::Vec result = cc->new_xmm_sd();
      cc->emit(x86::Inst::kIdSqrtsd, result, var.op());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpFrac: {
      var = writable_var(var);
      x86::Vec tmp = cc->new_xmm_sd();

      if (enable_sse4_1) {
        cc->emit(x86::Inst::kIdRoundsd, tmp, var.op(), x86::RoundImm::kDown | x86::RoundImm::kSuppress);
        cc->emit(x86::Inst::kIdSubsd, var.op(), tmp);
        return var;
      }
      else {
        // Pure SSE2 `frac()`, uses the same rounding trick as `floor()`.
        inline_round(tmp, var.vec(), kOpFloor);
        cc->subsd(var.vec(), tmp);
        return var;
      }
    }

    case kOpRecip: {
      x86::Vec result = cc->new_xmm_sd();
      cc->emit(x86::Inst::kIdMovsd, result, get_constant_f64(1.0).op());
      cc->emit(x86::Inst::kIdDivsd, result, var.op());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpSin:
    case kOpCos:
    case kOpTan:
    case kOpSinh:
    case kOpCosh:
    case kOpTanh:
    case kOpAsin:
    case kOpAcos:
    case kOpAtan:
      break;
  }

  // No inline implementation -> function call.
  x86::Vec result = cc->new_xmm_sd();
  x86::Vec args[1] = { register_var(var).vec() };
  inline_invoke(result, args, 1, JitUtils::func_by_op(op));

  return JitVar(result, JitVar::FLAG_NONE);
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
      if (vl.isRO() && !vr.isRO())
        vl.swap_with(vr);
    }

    vl = writable_var(vl);
  }

  uint32_t inst = 0;
  x86::CmpImm cmp_imm {};

  switch (op) {
    case kOpEq: cmp_imm = x86::CmpImm::kEQ ; goto emit_compare;
    case kOpNe: cmp_imm = x86::CmpImm::kNEQ; goto emit_compare;
    case kOpGt: cmp_imm = x86::CmpImm::kNLE; goto emit_compare;
    case kOpGe: cmp_imm = x86::CmpImm::kNLT; goto emit_compare;
    case kOpLt: cmp_imm = x86::CmpImm::kLT ; goto emit_compare;
    case kOpLe: cmp_imm = x86::CmpImm::kLE ; goto emit_compare;
emit_compare: {
      vl = writable_var(vl);
      cc->emit(x86::Inst::kIdCmpsd, vl.vec(), vr.op(), cmp_imm);
      cc->emit(x86::Inst::kIdAndpd, vl.vec(), get_constant_f64_as_2xf64(1.0).op());
      return vl;
    }

    case kOpAdd: inst = x86::Inst::kIdAddsd; goto emit_inst;
    case kOpSub: inst = x86::Inst::kIdSubsd; goto emit_inst;
    case kOpMul: inst = x86::Inst::kIdMulsd; goto emit_inst;
    case kOpDiv: inst = x86::Inst::kIdDivsd; goto emit_inst;
    case kOpMin: inst = x86::Inst::kIdMinsd; goto emit_inst;
    case kOpMax: inst = x86::Inst::kIdMaxsd; goto emit_inst;
emit_inst: {
      cc->emit(inst, vl.op(), vr.op()); return vl;
    }

    case kOpAvg: {
      vl = writable_var(vl);
      cc->emit(x86::Inst::kIdAddsd, vl.vec(), vr.op());
      cc->emit(x86::Inst::kIdMulsd, vl.vec(), get_constant_f64(0.5).op());
      return vl;
    }

    case kOpMod: {
      x86::Vec result = cc->new_xmm_sd();

      vl = writable_var(vl);
      vr = register_var(vr);

      cc->movapd(result, vl.vec());
      cc->divsd(vl.vec(), vr.vec());
      inline_round(vl.vec(), vl.vec(), kOpTrunc);
      cc->mulsd(vl.vec(), vr.vec());
      cc->subsd(result, vl.vec());

      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpPow:
    case kOpAtan2:
    case kOpHypot:
      break;

    case kOpCopySign: {
      vl = writable_var(vl);
      vr = writable_var(vr);

      cc->andpd(vl.vec(), get_constant_u64_as_2xf64(0x7FFFFFFFFFFFFFFFu).mem());
      cc->andpd(vr.vec(), get_constant_u64_as_2xf64(0x8000000000000000u).mem());
      cc->orpd(vl.vec(), vr.vec());

      return vl;
    }

    default:
      MATHPRESSO_ASSERT_NOT_REACHED();
      return vl;
  }

  // No inline implementation -> function call.
  x86::Vec result = cc->new_xmm_sd();
  x86::Vec args[2] = { register_var(vl).vec(), register_var(vr).vec() };
  inline_invoke(result, args, 2, JitUtils::func_by_op(op));

  return JitVar(result, JitVar::FLAG_NONE);
}

JitVar JitCompiler::on_invoke(AstCall* node) {
  uint32_t i, size = node->size();
  AstSymbol* sym = node->symbol();

  x86::Vec result = cc->new_xmm_sd();
  x86::Vec args[8];

  for (i = 0; i < size; i++)
    args[i] = register_var(on_node(node->child_at(i))).vec();

  inline_invoke(result, args, size, sym->func_ptr());
  return JitVar(result, JitVar::FLAG_NONE);
}

void JitCompiler::inline_round(const x86::Vec& dst, const x86::Vec& src, uint32_t op) {
  // SSE4.1 implementation is easy except `round()`, which is not `roundeven()`.
  if (enable_sse4_1) {
    if (op == kOpRound) {
      x86::Vec tmp = cc->new_xmm_sd();
      cc->roundsd(tmp, src, x86::RoundImm::kDown | x86::RoundImm::kSuppress);

      if (dst.id() != src.id())
        cc->movapd(dst, src);

      cc->subsd(dst, tmp);
      cc->cmpsd(dst, get_constant_f64(0.5).mem(), x86::CmpImm::kNLT);
      cc->andpd(dst, get_constant_f64_as_2xf64(1.0).mem());
      cc->addpd(dst, tmp);
    }
    else {
      x86::RoundImm round_imm {};

      switch (op) {
        case kOpRoundEven: round_imm = x86::RoundImm::kNearest; break;
        case kOpTrunc    : round_imm = x86::RoundImm::kTrunc  ; break;
        case kOpFloor    : round_imm = x86::RoundImm::kDown   ; break;
        case kOpCeil     : round_imm = x86::RoundImm::kUp     ; break;
        default:
          MATHPRESSO_ASSERT_NOT_REACHED();
      }
      cc->roundsd(dst, src, round_imm | x86::RoundImm::kSuppress);
    }
    return;
  }

  // Pure SSE2 requires the following rounding trick:
  //   double roundeven(double x) {
  //     double maxn   = pow(2, 52);
  //     double magic0 = pow(2, 52) + pow(2, 51);
  //     return x >= maxn ? x : x + magic0 - magic0;
  //   }
  const double maxn = 4503599627370496.0;
  const double magic0 = 6755399441055744.0;
  const double magic1 = 6755399441055745.0;

  if (op == kOpRoundEven) {
    x86::Vec tmp = cc->new_xmm_sd();

    cc->movapd(tmp, src);
    cc->cmpsd(tmp, get_constant_f64(maxn).mem(), x86::CmpImm::kLT);
    cc->andpd(tmp, get_constant_f64_aligned(magic0).mem());

    if (dst.id() != src.id())
      cc->movapd(dst, src);

    cc->addsd(dst, tmp);
    cc->subsd(dst, tmp);

    return;
  }

  // The `roundeven()` function can be used to implement efficiently the
  // remaining rounding functions. The easiest are `floor()` and `ceil()`.
  if (op == kOpRound || op == kOpFloor || op == kOpCeil) {
    x86::Vec t1 = cc->new_xmm_sd();
    x86::Vec t2 = cc->new_xmm_sd();
    x86::Vec t3 = cc->new_xmm_sd();

    cc->movapd(t2, src);
    cc->movapd(t3, src);

    if (dst.id() != src.id())
      cc->movapd(dst, src);

    switch (op) {
      case kOpRound:
        cc->addsd(t2, get_constant_f64(magic0).mem());
        cc->addsd(t3, get_constant_f64(magic1).mem());

        cc->movapd(t1, src);
        cc->subsd(t2, get_constant_f64(magic0).mem());
        cc->subsd(t3, get_constant_f64(magic1).mem());

        cc->cmpsd(t1, get_constant_f64(maxn).mem(), x86::CmpImm::kNLT);
        cc->maxsd(t2, t3);

        cc->andpd(dst, t1);
        break;

      case kOpFloor:
        cc->addsd(t2, get_constant_f64(magic0).mem());
        cc->movapd(t1, src);

        cc->subsd(t2, get_constant_f64(magic0).mem());
        cc->cmpsd(t1, get_constant_f64(maxn).mem(), x86::CmpImm::kNLT);

        cc->cmpsd(t3, t2, x86::CmpImm::kLT);
        cc->andpd(t3, get_constant_f64_as_2xf64(1.0).mem());

        cc->andpd(dst, t1);
        cc->subpd(t2, t3);
        break;

      case kOpCeil:
        cc->addsd(t2, get_constant_f64(magic0).mem());
        cc->movapd(t1, src);

        cc->subsd(t2, get_constant_f64(magic0).mem());
        cc->cmpsd(t1, get_constant_f64(maxn).mem(), x86::CmpImm::kNLT);

        cc->cmpsd(t3, t2, x86::CmpImm::kNLE);
        cc->andpd(t3, get_constant_f64_as_2xf64(1.0).mem());

        cc->andpd(dst, t1);
        cc->addpd(t2, t3);
        break;
    }

    cc->andnpd(t1, t2);
    cc->orpd(dst, t1);

    return;
  }

  if (op == kOpTrunc) {
    if (cc->is_64bit()) {
      // In 64-bit mode we can just do a roundtrip with CVTTSD2SI and CVTSI2SD
      // and use such value if the floating point was not already an integer.
      x86::Gp r = cc->new_gp64();
      x86::Vec t = cc->new_xmm_sd();
      x86::Vec d = dst.id() == src.id() ? cc->new_xmm_sd() : dst;

      cc->cvttsd2si(r, src);
      cc->movsd(t, get_constant_u64(0x7FFFFFFFFFFFFFFFu).mem());
      cc->andpd(t, src);

      cc->cvtsi2sd(d, r);
      cc->cmpsd(t, get_constant_f64(maxn).mem(), x86::CmpImm::kLT);
      cc->andpd(d, t);
      cc->andnpd(t, src);
      cc->orpd(d, t);

      if (dst.id() != d.id())
        cc->movapd(dst, d);
    }
    else {
      x86::Vec t1 = cc->new_xmm_sd();
      x86::Vec t2 = cc->new_xmm_sd();
      x86::Vec t3 = cc->new_xmm_sd();

      cc->movsd(t2, get_constant_u64(0x7FFFFFFFFFFFFFFFu).mem());
      cc->andpd(t2, src);

      if (dst.id() != src.id())
        cc->movapd(dst, src);

      cc->movapd(t1, t2);
      cc->addsd(t2, get_constant_f64(magic0).mem());
      cc->movapd(t3, t1);

      cc->subsd(t2, get_constant_f64(magic0).mem());
      cc->cmpsd(t1, get_constant_f64(maxn).mem(), x86::CmpImm::kNLT);

      cc->cmpsd(t3, t2, x86::CmpImm::kLT);
      cc->orpd(t1, get_constant_u64_as_2xf64(0x8000000000000000u).mem());
      cc->andpd(t3, get_constant_f64_as_2xf64(1.0).mem());

      cc->andpd(dst, t1);
      cc->subpd(t2, t3);

      cc->andnpd(t1, t2);
      cc->orpd(dst, t1);
    }
    return;
  }

  inline_invoke(dst, &src, 1, (void*)(Arg1Func)mp_round);
}

void JitCompiler::inline_invoke(const x86::Vec& dst, const x86::Vec* args, uint32_t count, void* fn) {
  uint32_t i;

  // Use function builder to build a function prototype.
  FuncSignature signature;
  signature.set_ret_t<double>();

  for (i = 0; i < count; i++)
    signature.add_arg_t<double>();

  // Create the function call.
  InvokeNode* invoke_node;
  cc->invoke(asmjit::Out(invoke_node), (uint64_t)fn, signature);
  invoke_node->set_ret(0, dst);

  for (i = 0; i < count; i++)
    invoke_node->set_arg(static_cast<uint32_t>(i), args[i]);
}

void JitCompiler::prepare_const_pool() {
  if (!const_pool) {
    cc->new_const_pool_node(asmjit::Out(const_pool));

    BaseNode* prev = cc->set_cursor(func_body);
    cc->lea(const_ptr, x86::ptr(const_pool->label()));
    if (prev != func_body) cc->set_cursor(prev);
  }
}

JitVar JitCompiler::get_constant_u64(uint64_t value) {
  prepare_const_pool();

  size_t offset;
  if (const_pool->add(&value, sizeof(uint64_t), asmjit::Out(offset)) != asmjit::Error::kOk)
    return JitVar();

  return JitVar(x86::ptr(const_ptr, static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::get_constant_u64_as_2xf64(uint64_t value) {
  prepare_const_pool();

  uint64_t data[2] = { value, 0 };
  size_t offset;

  if (const_pool->add(data, sizeof(data), asmjit::Out(offset)) != asmjit::Error::kOk)
    return JitVar();

  return JitVar(x86::ptr(const_ptr, static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::get_constant_u64_aligned(uint64_t value) {
  prepare_const_pool();

  uint64_t two[2] = { value, value };

  size_t offset;
  if (const_pool->add(two, sizeof(uint64_t) * 2, asmjit::Out(offset)) != asmjit::Error::kOk)
    return JitVar();

  return JitVar(x86::ptr(const_ptr, static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::get_constant_f64(double value) {
  DoubleBits bits;
  bits.d = value;
  return get_constant_u64(bits.u);
}

JitVar JitCompiler::get_constant_f64_as_2xf64(double value) {
  DoubleBits bits;
  bits.d = value;
  return get_constant_u64_as_2xf64(bits.u);
}

JitVar JitCompiler::get_constant_f64_aligned(double value) {
  DoubleBits bits;
  bits.d = value;
  return get_constant_u64_aligned(bits.u);
}

CompiledFunc compile_function(AstBuilder* ast, uint32_t options, OutputLog* log) {
  StringLogger logger;

  CodeHolder code;
  code.init((jit_global.runtime.environment()));

  x86::Compiler c(&code);
  bool debug_machine_code = log != nullptr && (options & kOptionDebugMachineCode) != 0;
  bool debug_compiler     = log != nullptr && (options & kOptionDebugCompiler   ) != 0;

  if (debug_machine_code || debug_compiler) {
    logger.add_flags(FormatFlags::kMachineCode | FormatFlags::kRegCasts | FormatFlags::kExplainImms);
    code.set_logger(&logger);

    if (debug_compiler)
      c.add_diagnostic_options(DiagnosticOptions::kRAAnnotate | DiagnosticOptions::kRADebugAll);
  }

  JitCompiler jit_compiler(ast->arena(), &c);
  if ((options & kOptionDisableSSE4_1) != 0)
    jit_compiler.enable_sse4_1 = false;

  jit_compiler.begin_function();
  jit_compiler.compile(ast->program_node(), ast->root_scope(), ast->_num_slots);
  jit_compiler.end_function();

  c.finalize();

  CompiledFunc fn;
  jit_global.runtime.add(&fn, &code);

  if (debug_machine_code || debug_compiler)
    log->log(OutputLog::kMessageAsm, 0, 0, logger.data(), logger.data_size());

  return fn;
}

void free_compiled_function(void* fn) {
  jit_global.runtime.release(fn);
}

} // {mathpresso}
