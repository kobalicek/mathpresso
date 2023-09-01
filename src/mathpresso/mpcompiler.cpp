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
static JitGlobal jitGlobal;

// MathPresso - JIT Utilities
// ==========================

struct JitUtils {
  static void* funcByOp(uint32_t op) {
    switch (op) {
      case kOpIsNan    : return (void*)(Arg1Func)mpIsNan;
      case kOpIsInf    : return (void*)(Arg1Func)mpIsInf;
      case kOpIsFinite : return (void*)(Arg1Func)mpIsFinite;
      case kOpSignBit  : return (void*)(Arg1Func)mpSignBit;

      case kOpRound    : return (void*)(Arg1Func)mpRound;
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
      case kOpFrac     : return (void*)(Arg1Func)mpFrac;
      case kOpRecip    : return (void*)(Arg1Func)mpRecip;

      case kOpSin      : return (void*)(Arg1Func)sin;
      case kOpCos      : return (void*)(Arg1Func)cos;
      case kOpTan      : return (void*)(Arg1Func)tan;
      case kOpSinh     : return (void*)(Arg1Func)sinh;
      case kOpCosh     : return (void*)(Arg1Func)cosh;
      case kOpTanh     : return (void*)(Arg1Func)tanh;
      case kOpAsin     : return (void*)(Arg1Func)asin;
      case kOpAcos     : return (void*)(Arg1Func)acos;
      case kOpAtan     : return (void*)(Arg1Func)atan;

      case kOpAvg      : return (void*)(Arg2Func)mpAvg;
      case kOpMin      : return (void*)(Arg2Func)mpMin<double>;
      case kOpMax      : return (void*)(Arg2Func)mpMax<double>;
      case kOpPow      : return (void*)(Arg2Func)pow;
      case kOpAtan2    : return (void*)(Arg2Func)atan2;
      case kOpHypot    : return (void*)(Arg2Func)hypot;
      case kOpCopySign : return (void*)(Arg2Func)mpCopySign;

      default:
        MATHPRESSO_ASSERT_NOT_REACHED();
        return NULL;
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

  inline void swapWith(JitVar& other) noexcept {
    JitVar t(*this);
    *this = other;
    other = t;
  }

  inline bool isNone() const noexcept { return _op.isNone(); }
  inline bool isMem() const noexcept { return _op.isMem(); }
  inline bool isXmm() const noexcept { return _op.isReg(RegType::kX86_Xmm); }

  inline const Operand& op() const noexcept { return _op; }
  inline const x86::Mem& mem() const noexcept { return _op.as<x86::Mem>(); }
  inline const x86::Xmm& xmm() const noexcept { return _op.as<x86::Xmm>(); }

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
  JitCompiler(ZoneAllocator* allocator, x86::Compiler* c);
  ~JitCompiler();

  // Function Generator.
  void beginFunction();
  void endFunction();

  // Variable Management.
  JitVar copyVar(const JitVar& other, uint32_t flags);
  JitVar writableVar(const JitVar& other);
  JitVar registerVar(const JitVar& other);

  // Compiler.
  void compile(AstBlock* node, AstScope* rootScope, uint32_t numSlots);

  JitVar onNode(AstNode* node);
  JitVar onBlock(AstBlock* node);
  JitVar onVarDecl(AstVarDecl* node);
  JitVar onVar(AstVar* node);
  JitVar onImm(AstImm* node);
  JitVar onUnaryOp(AstUnaryOp* node);
  JitVar onBinaryOp(AstBinaryOp* node);
  JitVar onCall(AstCall* node);

  // Helpers.
  void inlineRound(const x86::Xmm& dst, const x86::Xmm& src, uint32_t op);
  void inlineCall(const x86::Xmm& dst, const x86::Xmm* args, uint32_t count, void* fn);

  // Constants.
  void prepareConstPool();
  JitVar getConstantU64(uint64_t value);
  JitVar getConstantU64AsPD(uint64_t value);
  JitVar getConstantU64Aligned(uint64_t value);
  JitVar getConstantD64(double value);
  JitVar getConstantD64AsPD(double value);
  JitVar getConstantD64Aligned(double value);

  // Members.
  ZoneAllocator* allocator = nullptr;
  x86::Compiler* cc = nullptr;

  x86::Gp varPtr;
  x86::Gp constPtr;
  x86::Gp resultPtr;

  JitVar* varSlots = nullptr;
  BaseNode* funcBody = nullptr;
  ConstPoolNode* constPool = nullptr;

  bool enableSSE4_1 = false;
};

JitCompiler::JitCompiler(ZoneAllocator* allocator, x86::Compiler* cc)
  : allocator(allocator),
    cc(cc),
    varSlots(NULL),
    funcBody(NULL) {

  const CpuFeatures::X86& features = CpuInfo::host().features().x86();
  enableSSE4_1 = features.hasSSE4_1();
}
JitCompiler::~JitCompiler() {}

void JitCompiler::beginFunction() {
  FuncNode* funcNode = cc->addFunc(FuncSignatureT<void, double*, double*>(CallConvId::kCDecl));

  varPtr = cc->newIntPtr("varPtr");
  constPtr = cc->newIntPtr("constPtr");
  resultPtr = cc->newIntPtr("resultPtr");

  funcNode->setArg(0, resultPtr);
  funcNode->setArg(1, varPtr);
  funcBody = cc->cursor();
}

void JitCompiler::endFunction() {
  cc->endFunc();
  if (constPool)
    cc->addNode(constPool);
}

JitVar JitCompiler::copyVar(const JitVar& other, uint32_t flags) {
  JitVar v(cc->newXmmSd(), flags);
  cc->emit(other.isXmm() ? x86::Inst::kIdMovapd : x86::Inst::kIdMovsd,
    v.xmm(), other.op());
  return v;
}

JitVar JitCompiler::writableVar(const JitVar& other) {
  if (other.isMem() || other.isRO())
    return copyVar(other, other.flags() & ~JitVar::FLAG_RO);
  else
    return other;
}

JitVar JitCompiler::registerVar(const JitVar& other) {
  if (other.isMem())
    return copyVar(other, other.flags());
  else
    return other;
}

void JitCompiler::compile(AstBlock* node, AstScope* rootScope, uint32_t numSlots) {
  if (numSlots != 0) {
    varSlots = static_cast<JitVar*>(allocator->alloc(sizeof(JitVar) * numSlots));
    if (varSlots == NULL) return;

    for (uint32_t i = 0; i < numSlots; i++)
      varSlots[i] = JitVar();
  }

  // Result of the function or NaN.
  JitVar result = onBlock(node);

  // Write altered global variables.
  {
    AstSymbolHashIterator it(rootScope->symbols());
    while (it.has()) {
      AstSymbol* sym = it.get();
      if (sym->isGlobal() && sym->isAltered()) {
        JitVar v = varSlots[sym->varSlotId()];
        cc->emit(x86::Inst::kIdMovsd,
          x86::ptr(varPtr, sym->varOffset()), registerVar(v).xmm());
      }

      it.next();
    }
  }

  // Return NaN if no result is given.
  x86::Xmm var;
  if (result.isNone())
    var = registerVar(getConstantD64(mpGetNan())).xmm();
  else
    var = registerVar(result).xmm();
  cc->movsd(x86::ptr(resultPtr), var);

  if (numSlots != 0)
    allocator->release(varSlots, sizeof(JitVar) * numSlots);
}

JitVar JitCompiler::onNode(AstNode* node) {
  switch (node->nodeType()) {
    case kAstNodeBlock    : return onBlock    (static_cast<AstBlock*   >(node));
    case kAstNodeVarDecl  : return onVarDecl  (static_cast<AstVarDecl* >(node));
    case kAstNodeVar      : return onVar      (static_cast<AstVar*     >(node));
    case kAstNodeImm      : return onImm      (static_cast<AstImm*     >(node));
    case kAstNodeUnaryOp  : return onUnaryOp  (static_cast<AstUnaryOp* >(node));
    case kAstNodeBinaryOp : return onBinaryOp (static_cast<AstBinaryOp*>(node));
    case kAstNodeCall     : return onCall     (static_cast<AstCall*    >(node));

    default:
      MATHPRESSO_ASSERT_NOT_REACHED();
      return JitVar();
  }
}

JitVar JitCompiler::onBlock(AstBlock* node) {
  JitVar result;
  uint32_t i, size = node->size();

  for (i = 0; i < size; i++)
    result = onNode(node->childAt(i));

  // Return the last result (or no result if the block is empty).
  return result;
}

JitVar JitCompiler::onVarDecl(AstVarDecl* node) {
  JitVar result;

  if (node->child())
    result = onNode(node->child());

  AstSymbol* sym = node->symbol();
  uint32_t slotId = sym->varSlotId();

  result.setRO();
  varSlots[slotId] = result;

  return result;
}

JitVar JitCompiler::onVar(AstVar* node) {
  AstSymbol* sym = node->symbol();
  uint32_t slotId = sym->varSlotId();

  JitVar result = varSlots[slotId];
  if (result.isNone()) {
    if (sym->isGlobal()) {
      result = JitVar(x86::ptr(varPtr, sym->varOffset()), JitVar::FLAG_RO);
      varSlots[slotId] = result;
      if (sym->writeCount() > 0)
        result = copyVar(result, JitVar::FLAG_NONE);
    }
    else {
      result = getConstantD64(mpGetNan());
      varSlots[slotId] = result;
    }
  }

  return result;
}

JitVar JitCompiler::onImm(AstImm* node) {
  return getConstantD64(node->value());
}

JitVar JitCompiler::onUnaryOp(AstUnaryOp* node) {
  uint32_t op = node->opType();
  JitVar var = onNode(node->child());

  switch (op) {
    case kOpNone:
      return var;

    case kOpNeg: {
      x86::Xmm result = cc->newXmmSd();
      cc->emit(x86::Inst::kIdXorpd, result, result);
      cc->emit(x86::Inst::kIdSubsd, result, var.op());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpNot: {
      var = writableVar(var);
      cc->cmpsd(var.xmm(), getConstantD64AsPD(0.0).mem(), x86::CmpImm::kEQ);
      cc->andpd(var.xmm(), getConstantD64AsPD(1.0).mem());
      return var;
    }

    case kOpIsNan: {
      var = writableVar(var);
      cc->cmpsd(var.xmm(), var.xmm(), x86::CmpImm::kEQ);
      cc->andnpd(var.xmm(), getConstantD64AsPD(1.0).mem());
      return var;
    }

    case kOpIsInf: {
      var = writableVar(var);
      cc->orpd(var.xmm(), getConstantU64AsPD(0x8000000000000000u).mem());
      cc->cmpsd(var.xmm(), getConstantU64(0xFFF0000000000000u).mem(), x86::CmpImm::kEQ);
      cc->andpd(var.xmm(), getConstantD64AsPD(1.0).mem());
      return var;
    }

    case kOpIsFinite: {
      var = writableVar(var);
      cc->orpd(var.xmm(), getConstantU64AsPD(0x8000000000000000u).mem());
      cc->cmpsd(var.xmm(), getConstantD64(0.0).mem(), x86::CmpImm::kLE);
      cc->andpd(var.xmm(), getConstantD64AsPD(1.0).mem());
      return var;
    }

    case kOpSignBit: {
      x86::Xmm result = cc->newXmmSd();
      cc->pshufd(result, registerVar(var).xmm(), x86::shuffleImm(3, 2, 1, 1));
      cc->psrad(result, 31);
      cc->andpd(result, getConstantD64AsPD(1.0).mem());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpRound:
    case kOpRoundEven:
    case kOpTrunc:
    case kOpFloor:
    case kOpCeil: {
      var = writableVar(var);
      inlineRound(var.xmm(), var.xmm(), op);
      return var;
    }

    case kOpAbs: {
      x86::Xmm result = cc->newXmmSd();
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
      x86::Xmm result = cc->newXmmSd();
      cc->emit(x86::Inst::kIdSqrtsd, result, var.op());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpFrac: {
      var = writableVar(var);
      x86::Xmm tmp = cc->newXmmSd();

      if (enableSSE4_1) {
        cc->emit(x86::Inst::kIdRoundsd, tmp, var.op(), x86::RoundImm::kDown | x86::RoundImm::kSuppress);
        cc->emit(x86::Inst::kIdSubsd, var.op(), tmp);
        return var;
      }
      else {
        // Pure SSE2 `frac()`, uses the same rounding trick as `floor()`.
        inlineRound(tmp, var.xmm(), kOpFloor);
        cc->subsd(var.xmm(), tmp);
        return var;
      }
    }

    case kOpRecip: {
      x86::Xmm result = cc->newXmmSd();
      cc->emit(x86::Inst::kIdMovsd, result, getConstantD64(1.0).op());
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
  x86::Xmm result = cc->newXmmSd();
  x86::Xmm args[1] = { registerVar(var).xmm() };
  inlineCall(result, args, 1, JitUtils::funcByOp(op));

  return JitVar(result, JitVar::FLAG_NONE);
}

JitVar JitCompiler::onBinaryOp(AstBinaryOp* node) {
  uint32_t op = node->opType();

  AstNode* left  = node->left();
  AstNode* right = node->right();

  // Compile assignment.
  if (op == kOpAssign) {
    AstVar* varNode = reinterpret_cast<AstVar*>(left);
    MATHPRESSO_ASSERT(varNode->nodeType() == kAstNodeVar);

    AstSymbol* sym = varNode->symbol();
    uint32_t slotId = sym->varSlotId();

    JitVar result = onNode(right);
    result.setRO();

    sym->setAltered();
    varSlots[slotId] = result;

    return result;
  }

  // Handle the case that the operands are the same variable.
  JitVar vl, vr;
  if (left->nodeType() == kAstNodeVar &&
      right->nodeType() == kAstNodeVar &&
      static_cast<AstVar*>(left)->symbol() == static_cast<AstVar*>(right)->symbol()) {
    vl = vr = writableVar(onNode(node->left()));
  }
  else {
    vl = onNode(node->left());
    vr = onNode(node->right());

    // Commutativity.
    if (op == kOpAdd || op == kOpMul || op == kOpAvg || op == kOpMin || op == kOpMax) {
      if (vl.isRO() && !vr.isRO())
        vl.swapWith(vr);
    }

    vl = writableVar(vl);
  }

  uint32_t inst = 0;
  x86::CmpImm cmpImm {};

  switch (op) {
    case kOpEq: cmpImm = x86::CmpImm::kEQ ; goto emitCompare;
    case kOpNe: cmpImm = x86::CmpImm::kNEQ; goto emitCompare;
    case kOpGt: cmpImm = x86::CmpImm::kNLE; goto emitCompare;
    case kOpGe: cmpImm = x86::CmpImm::kNLT; goto emitCompare;
    case kOpLt: cmpImm = x86::CmpImm::kLT ; goto emitCompare;
    case kOpLe: cmpImm = x86::CmpImm::kLE ; goto emitCompare;
emitCompare: {
      vl = writableVar(vl);
      cc->emit(x86::Inst::kIdCmpsd, vl.xmm(), vr.op(), cmpImm);
      cc->emit(x86::Inst::kIdAndpd, vl.xmm(), getConstantD64AsPD(1.0).op());
      return vl;
    }

    case kOpAdd: inst = x86::Inst::kIdAddsd; goto emitInst;
    case kOpSub: inst = x86::Inst::kIdSubsd; goto emitInst;
    case kOpMul: inst = x86::Inst::kIdMulsd; goto emitInst;
    case kOpDiv: inst = x86::Inst::kIdDivsd; goto emitInst;
    case kOpMin: inst = x86::Inst::kIdMinsd; goto emitInst;
    case kOpMax: inst = x86::Inst::kIdMaxsd; goto emitInst;
emitInst: {
      cc->emit(inst, vl.op(), vr.op()); return vl;
    }

    case kOpAvg: {
      vl = writableVar(vl);
      cc->emit(x86::Inst::kIdAddsd, vl.xmm(), vr.op());
      cc->emit(x86::Inst::kIdMulsd, vl.xmm(), getConstantD64(0.5).op());
      return vl;
    }

    case kOpMod: {
      x86::Xmm result = cc->newXmmSd();

      vl = writableVar(vl);
      vr = registerVar(vr);

      cc->movapd(result, vl.xmm());
      cc->divsd(vl.xmm(), vr.xmm());
      inlineRound(vl.xmm(), vl.xmm(), kOpTrunc);
      cc->mulsd(vl.xmm(), vr.xmm());
      cc->subsd(result, vl.xmm());

      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpPow:
    case kOpAtan2:
    case kOpHypot:
      break;

    case kOpCopySign: {
      vl = writableVar(vl);
      vr = writableVar(vr);

      cc->andpd(vl.xmm(), getConstantU64AsPD(0x7FFFFFFFFFFFFFFFu).mem());
      cc->andpd(vr.xmm(), getConstantU64AsPD(0x8000000000000000u).mem());
      cc->orpd(vl.xmm(), vr.xmm());

      return vl;
    }

    default:
      MATHPRESSO_ASSERT_NOT_REACHED();
      return vl;
  }

  // No inline implementation -> function call.
  x86::Xmm result = cc->newXmmSd();
  x86::Xmm args[2] = { registerVar(vl).xmm(), registerVar(vr).xmm() };
  inlineCall(result, args, 2, JitUtils::funcByOp(op));

  return JitVar(result, JitVar::FLAG_NONE);
}

JitVar JitCompiler::onCall(AstCall* node) {
  uint32_t i, size = node->size();
  AstSymbol* sym = node->symbol();

  x86::Xmm result = cc->newXmmSd();
  x86::Xmm args[8];

  for (i = 0; i < size; i++)
    args[i] = registerVar(onNode(node->childAt(i))).xmm();

  inlineCall(result, args, size, sym->funcPtr());
  return JitVar(result, JitVar::FLAG_NONE);
}

void JitCompiler::inlineRound(const x86::Xmm& dst, const x86::Xmm& src, uint32_t op) {
  // SSE4.1 implementation is easy except `round()`, which is not `roundeven()`.
  if (enableSSE4_1) {
    if (op == kOpRound) {
      x86::Xmm tmp = cc->newXmmSd();
      cc->roundsd(tmp, src, x86::RoundImm::kDown | x86::RoundImm::kSuppress);

      if (dst.id() != src.id())
        cc->movapd(dst, src);

      cc->subsd(dst, tmp);
      cc->cmpsd(dst, getConstantD64(0.5).mem(), x86::CmpImm::kNLT);
      cc->andpd(dst, getConstantD64AsPD(1.0).mem());
      cc->addpd(dst, tmp);
    }
    else {
      x86::RoundImm roundImm {};

      switch (op) {
        case kOpRoundEven: roundImm = x86::RoundImm::kNearest; break;
        case kOpTrunc    : roundImm = x86::RoundImm::kTrunc  ; break;
        case kOpFloor    : roundImm = x86::RoundImm::kDown   ; break;
        case kOpCeil     : roundImm = x86::RoundImm::kUp     ; break;
        default:
          MATHPRESSO_ASSERT_NOT_REACHED();
      }
      cc->roundsd(dst, src, roundImm | x86::RoundImm::kSuppress);
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
    x86::Xmm tmp = cc->newXmmSd();

    cc->movapd(tmp, src);
    cc->cmpsd(tmp, getConstantD64(maxn).mem(), x86::CmpImm::kLT);
    cc->andpd(tmp, getConstantD64Aligned(magic0).mem());

    if (dst.id() != src.id())
      cc->movapd(dst, src);

    cc->addsd(dst, tmp);
    cc->subsd(dst, tmp);

    return;
  }

  // The `roundeven()` function can be used to implement efficiently the
  // remaining rounding functions. The easiest are `floor()` and `ceil()`.
  if (op == kOpRound || op == kOpFloor || op == kOpCeil) {
    x86::Xmm t1 = cc->newXmmSd();
    x86::Xmm t2 = cc->newXmmSd();
    x86::Xmm t3 = cc->newXmmSd();

    cc->movapd(t2, src);
    cc->movapd(t3, src);

    if (dst.id() != src.id())
      cc->movapd(dst, src);

    switch (op) {
      case kOpRound:
        cc->addsd(t2, getConstantD64(magic0).mem());
        cc->addsd(t3, getConstantD64(magic1).mem());

        cc->movapd(t1, src);
        cc->subsd(t2, getConstantD64(magic0).mem());
        cc->subsd(t3, getConstantD64(magic1).mem());

        cc->cmpsd(t1, getConstantD64(maxn).mem(), x86::CmpImm::kNLT);
        cc->maxsd(t2, t3);

        cc->andpd(dst, t1);
        break;

      case kOpFloor:
        cc->addsd(t2, getConstantD64(magic0).mem());
        cc->movapd(t1, src);

        cc->subsd(t2, getConstantD64(magic0).mem());
        cc->cmpsd(t1, getConstantD64(maxn).mem(), x86::CmpImm::kNLT);

        cc->cmpsd(t3, t2, x86::CmpImm::kLT);
        cc->andpd(t3, getConstantD64AsPD(1.0).mem());

        cc->andpd(dst, t1);
        cc->subpd(t2, t3);
        break;

      case kOpCeil:
        cc->addsd(t2, getConstantD64(magic0).mem());
        cc->movapd(t1, src);

        cc->subsd(t2, getConstantD64(magic0).mem());
        cc->cmpsd(t1, getConstantD64(maxn).mem(), x86::CmpImm::kNLT);

        cc->cmpsd(t3, t2, x86::CmpImm::kNLE);
        cc->andpd(t3, getConstantD64AsPD(1.0).mem());

        cc->andpd(dst, t1);
        cc->addpd(t2, t3);
        break;
    }

    cc->andnpd(t1, t2);
    cc->orpd(dst, t1);

    return;
  }

  if (op == kOpTrunc) {
    if (cc->is64Bit()) {
      // In 64-bit mode we can just do a roundtrip with CVTTSD2SI and CVTSI2SD
      // and use such value if the floating point was not already an integer.
      x86::Gp r = cc->newInt64();
      x86::Xmm t = cc->newXmmSd();
      x86::Xmm d = dst.id() == src.id() ? cc->newXmmSd() : dst;

      cc->cvttsd2si(r, src);
      cc->movsd(t, getConstantU64(0x7FFFFFFFFFFFFFFFu).mem());
      cc->andpd(t, src);

      cc->cvtsi2sd(d, r);
      cc->cmpsd(t, getConstantD64(maxn).mem(), x86::CmpImm::kLT);
      cc->andpd(d, t);
      cc->andnpd(t, src);
      cc->orpd(d, t);

      if (dst.id() != d.id())
        cc->movapd(dst, d);
    }
    else {
      x86::Xmm t1 = cc->newXmmSd();
      x86::Xmm t2 = cc->newXmmSd();
      x86::Xmm t3 = cc->newXmmSd();

      cc->movsd(t2, getConstantU64(0x7FFFFFFFFFFFFFFFu).mem());
      cc->andpd(t2, src);

      if (dst.id() != src.id())
        cc->movapd(dst, src);

      cc->movapd(t1, t2);
      cc->addsd(t2, getConstantD64(magic0).mem());
      cc->movapd(t3, t1);

      cc->subsd(t2, getConstantD64(magic0).mem());
      cc->cmpsd(t1, getConstantD64(maxn).mem(), x86::CmpImm::kNLT);

      cc->cmpsd(t3, t2, x86::CmpImm::kLT);
      cc->orpd(t1, getConstantU64AsPD(0x8000000000000000u).mem());
      cc->andpd(t3, getConstantD64AsPD(1.0).mem());

      cc->andpd(dst, t1);
      cc->subpd(t2, t3);

      cc->andnpd(t1, t2);
      cc->orpd(dst, t1);
    }
    return;
  }

  inlineCall(dst, &src, 1, (void*)(Arg1Func)mpRound);
}

void JitCompiler::inlineCall(const x86::Xmm& dst, const x86::Xmm* args, uint32_t count, void* fn) {
  uint32_t i;

  // Use function builder to build a function prototype.
  FuncSignatureBuilder signature;
  signature.setRetT<double>();

  for (i = 0; i < count; i++)
    signature.addArgT<double>();

  // Create the function call.
  InvokeNode* invokeNode;
  cc->invoke(&invokeNode, (uint64_t)fn, signature);
  invokeNode->setRet(0, dst);

  for (i = 0; i < count; i++)
    invokeNode->setArg(static_cast<uint32_t>(i), args[i]);
}

void JitCompiler::prepareConstPool() {
  if (!constPool) {
    cc->newConstPoolNode(&constPool);

    BaseNode* prev = cc->setCursor(funcBody);
    cc->lea(constPtr, x86::ptr(constPool->label()));
    if (prev != funcBody) cc->setCursor(prev);
  }
}

JitVar JitCompiler::getConstantU64(uint64_t value) {
  prepareConstPool();

  size_t offset;
  if (constPool->add(&value, sizeof(uint64_t), offset) != kErrorOk)
    return JitVar();

  return JitVar(x86::ptr(constPtr, static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::getConstantU64AsPD(uint64_t value) {
  prepareConstPool();

  uint64_t data[2] = { value, 0 };
  size_t offset;

  if (constPool->add(data, sizeof(data), offset) != kErrorOk)
    return JitVar();

  return JitVar(x86::ptr(constPtr, static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::getConstantU64Aligned(uint64_t value) {
  prepareConstPool();

  uint64_t two[2] = { value, value };

  size_t offset;
  if (constPool->add(two, sizeof(uint64_t) * 2, offset) != kErrorOk)
    return JitVar();

  return JitVar(x86::ptr(constPtr, static_cast<int>(offset)), JitVar::FLAG_NONE);
}

JitVar JitCompiler::getConstantD64(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64(bits.u);
}

JitVar JitCompiler::getConstantD64AsPD(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64AsPD(bits.u);
}

JitVar JitCompiler::getConstantD64Aligned(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64Aligned(bits.u);
}

CompiledFunc mpCompileFunction(AstBuilder* ast, uint32_t options, OutputLog* log) {
  StringLogger logger;

  CodeHolder code;
  code.init((jitGlobal.runtime.environment()));

  x86::Compiler c(&code);
  bool debugMachineCode = log != nullptr && (options & kOptionDebugMachineCode) != 0;
  bool debugCompiler    = log != nullptr && (options & kOptionDebugCompiler   ) != 0;

  if (debugMachineCode || debugCompiler) {
    logger.addFlags(FormatFlags::kMachineCode | FormatFlags::kRegCasts | FormatFlags::kExplainImms);
    code.setLogger(&logger);

    if (debugCompiler)
      c.addDiagnosticOptions(DiagnosticOptions::kRAAnnotate | DiagnosticOptions::kRADebugAll);
  }

  JitCompiler jitCompiler(ast->allocator(), &c);
  if ((options & kOptionDisableSSE4_1) != 0)
    jitCompiler.enableSSE4_1 = false;

  jitCompiler.beginFunction();
  jitCompiler.compile(ast->programNode(), ast->rootScope(), ast->_numSlots);
  jitCompiler.endFunction();

  c.finalize();

  CompiledFunc fn;
  jitGlobal.runtime.add(&fn, &code);

  if (debugMachineCode || debugCompiler)
    log->log(OutputLog::kMessageAsm, 0, 0, logger.data(), logger.dataSize());

  return fn;
}

void mpFreeFunction(void* fn) {
  jitGlobal.runtime.release(fn);
}

} // {mathpresso}
