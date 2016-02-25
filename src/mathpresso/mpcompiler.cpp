// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_EXPORTS

// [Dependencies]
#include "./mpast_p.h"
#include "./mpcompiler_p.h"
#include "./mpeval_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::JitGlobal]
// ============================================================================

struct JitGlobal {
  asmjit::JitRuntime runtime;
};
static JitGlobal jitGlobal;

using asmjit::Operand;
using asmjit::X86Mem;
using asmjit::X86GpVar;
using asmjit::X86XmmVar;

// ============================================================================
// [mathpresso::JitUtils]
// ============================================================================

struct JitUtils {
  static void* getFuncByOp(uint32_t op) {
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

// ============================================================================
// [mathpresso::JitVar]
// ============================================================================

struct MATHPRESSO_NOAPI JitVar {
  enum FLAGS {
    FLAG_NONE = 0,
    FLAG_RO = 1
  };

  MATHPRESSO_INLINE JitVar() : op(), flags(FLAG_NONE) {}
  MATHPRESSO_INLINE JitVar(Operand op, uint32_t flags) : op(op), flags(flags) {}
  MATHPRESSO_INLINE JitVar(const JitVar& other) : op(other.op), flags(other.flags) {}
  MATHPRESSO_INLINE ~JitVar() {}

  // Reset
  MATHPRESSO_INLINE void reset() {
    op.reset();
    flags = FLAG_NONE;
  }

  // Operator Overload.
  MATHPRESSO_INLINE const JitVar& operator=(const JitVar& other) {
    op = other.op;
    flags = other.flags;
    return *this;
  }

  // Swap.
  MATHPRESSO_INLINE void swapWith(JitVar& other) {
    JitVar t(*this);
    *this = other;
    other = t;
  }

  // Operand management.
  MATHPRESSO_INLINE const Operand& getOperand() const { return op; }
  MATHPRESSO_INLINE const X86Mem& getMem() const { return *static_cast<const X86Mem*>(&op); }
  MATHPRESSO_INLINE const X86XmmVar& getXmm() const { return *static_cast<const X86XmmVar*>(&op); }

  MATHPRESSO_INLINE bool isNone() const { return op.isNone(); }
  MATHPRESSO_INLINE bool isMem() const { return op.isMem(); }
  MATHPRESSO_INLINE bool isXmm() const { return op.isRegType(asmjit::kX86RegTypeXmm); }

  // Flags.
  MATHPRESSO_INLINE bool isRO() const { return (flags & FLAG_RO) != 0; }

  // Members.
  Operand op;
  uint32_t flags;
};

// ============================================================================
// [mathpresso::JitCompiler]
// ============================================================================

struct MATHPRESSO_NOAPI JitCompiler {
  JitCompiler(Allocator* allocator, asmjit::X86Compiler* c);
  ~JitCompiler();

  // Function Generator.
  void beginFunction();
  void endFunction();

  // Variable Management.
  JitVar copyVar(const JitVar& other);
  JitVar writableVar(const JitVar& other);
  JitVar registerVar(const JitVar& other);

  // Compiler.
  void compile(AstBlock* node, uint32_t numSlots);

  JitVar onNode(AstNode* node);
  JitVar onBlock(AstBlock* node);
  JitVar onVarDecl(AstVarDecl* node);
  JitVar onVar(AstVar* node);
  JitVar onImm(AstImm* node);
  JitVar onUnaryOp(AstUnaryOp* node);
  JitVar onBinaryOp(AstBinaryOp* node);
  JitVar onCall(AstCall* node);

  // Helpers.
  void inlineRound(const X86XmmVar& dst, const X86XmmVar& src, uint32_t op);
  void inlineCall(const X86XmmVar& dst, const X86XmmVar* args, uint32_t count, void* fn);

  // Constants.
  void prepareConstPool();
  JitVar getConstantU64(uint64_t value);
  JitVar getConstantU64AsPD(uint64_t value);
  JitVar getConstantD64(double value);
  JitVar getConstantD64AsPD(double value);

  // Members.
  Allocator* allocator;
  asmjit::X86Compiler* c;

  X86GpVar resultAddress;
  X86GpVar variablesAddress;

  JitVar* varSlots;
  asmjit::HLNode* functionBody;

  asmjit::Label constLabel;
  X86GpVar constPtr;
  asmjit::ConstPool constPool;

  bool enableSSE4_1;
};

JitCompiler::JitCompiler(Allocator* allocator, asmjit::X86Compiler* c)
  : allocator(allocator),
    c(c),
    varSlots(NULL),
    functionBody(NULL),
    constPool(&c->_constAllocator) {

  enableSSE4_1 = asmjit::X86CpuInfo::getHost()->hasFeature(asmjit::kX86CpuFeatureSSE4_1);
}
JitCompiler::~JitCompiler() {}

void JitCompiler::beginFunction() {
  c->addFunc(asmjit::FuncBuilder2<asmjit::Void, double*, double*>(asmjit::kCallConvHostCDecl));
  c->getFunc()->setHint(asmjit::kFuncHintNaked, true);

  resultAddress = c->newIntPtr("pResult");
  variablesAddress = c->newIntPtr("pVariables");
  constPtr = c->newIntPtr("pConst");

  c->setArg(0, resultAddress);
  c->setArg(1, variablesAddress);

  c->setPriority(variablesAddress, 1);
  c->setPriority(constPtr, 2);

  functionBody = c->getCursor();
}

void JitCompiler::endFunction() {
  c->endFunc();

  if (constLabel.isInitialized())
    c->embedConstPool(constLabel, constPool);
}

JitVar JitCompiler::copyVar(const JitVar& other) {
  JitVar v(c->newXmmSd(), JitVar::FLAG_NONE);
  c->emit(asmjit::kX86InstIdMovsd, v.getXmm(), other.getOperand());
  return v;
}

JitVar JitCompiler::writableVar(const JitVar& other) {
  if (other.isMem() || other.isRO())
    return copyVar(other);
  else
    return other;
}

JitVar JitCompiler::registerVar(const JitVar& other) {
  if (other.isMem())
    return copyVar(other);
  else
    return other;
}

void JitCompiler::compile(AstBlock* node, uint32_t numSlots) {
  if (numSlots != 0) {
    varSlots = static_cast<JitVar*>(allocator->alloc(sizeof(JitVar) * numSlots));
    if (varSlots == NULL) return;

    for (uint32_t i = 0; i < numSlots; i++)
      varSlots[i] = JitVar(c->newXmmSd(), JitVar::FLAG_NONE);
  }

  JitVar result = onBlock(node);
  X86XmmVar var;

  // Return NaN if no result is given.
  if (result.isNone())
    var = registerVar(getConstantD64(mpGetNan())).getXmm();
  else
    var = registerVar(result).getXmm();
  c->movsd(asmjit::x86::ptr(resultAddress), var);

  if (numSlots != 0)
    allocator->release(varSlots, sizeof(JitVar) * numSlots);
}

JitVar JitCompiler::onNode(AstNode* node) {
  switch (node->getNodeType()) {
    case kAstNodeBlock    : return onBlock    (static_cast<AstBlock*    >(node));
    case kAstNodeVarDecl  : return onVarDecl  (static_cast<AstVarDecl*  >(node));
    case kAstNodeVar      : return onVar      (static_cast<AstVar*      >(node));
    case kAstNodeImm      : return onImm      (static_cast<AstImm*      >(node));
    case kAstNodeUnaryOp  : return onUnaryOp  (static_cast<AstUnaryOp*  >(node));
    case kAstNodeBinaryOp : return onBinaryOp (static_cast<AstBinaryOp* >(node));
    case kAstNodeCall     : return onCall     (static_cast<AstCall*     >(node));

    default:
      MATHPRESSO_ASSERT_NOT_REACHED();
      return JitVar();
  }
}

JitVar JitCompiler::onBlock(AstBlock* node) {
  JitVar result;
  uint32_t i, len = node->getLength();

  for (i = 0; i < len; i++)
    result = onNode(node->getAt(i));

  // Return the last result (or no result if the block is empty).
  return result;
}

JitVar JitCompiler::onVarDecl(AstVarDecl* node) {
  JitVar result;

  if (node->hasChild())
    result = onNode(node->getChild());

  AstSymbol* sym = node->getSymbol();
  varSlots[sym->getVarSlot()] = result;

  return result;
}

JitVar JitCompiler::onVar(AstVar* node) {
  AstSymbol* sym = node->getSymbol();
  if (sym->getVarSlot() == 0xFFFFFFFF)
    return JitVar(asmjit::x86::ptr(variablesAddress, sym->getVarOffset()), JitVar::FLAG_RO);
  
  JitVar result = varSlots[sym->getVarSlot()];
  if (result.isNone())
    result = getConstantD64(mpGetNan());
  return result;
}

JitVar JitCompiler::onImm(AstImm* node) {
  return getConstantD64(node->getValue());
}

JitVar JitCompiler::onUnaryOp(AstUnaryOp* node) {
  uint32_t op = node->getOp();
  JitVar var = onNode(node->getChild());

  switch (op) {
    case kOpNone:
      return var;

    case kOpNeg: {
      X86XmmVar result = c->newXmmSd();
      c->emit(asmjit::kX86InstIdXorpd, result, result);
      c->emit(asmjit::kX86InstIdSubsd, result, var.getOperand());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpNot: {
      var = writableVar(var);
      c->cmpsd(var.getXmm(), getConstantD64AsPD(0.0).getMem(), int(asmjit::kX86CmpEQ));
      c->andpd(var.getXmm(), getConstantD64AsPD(1.0).getMem());
      return var;
    }

    case kOpIsNan: {
      var = writableVar(var);
      c->cmpsd(var.getXmm(), var.getXmm(), int(asmjit::kX86CmpEQ));
      c->andnpd(var.getXmm(), getConstantD64AsPD(1.0).getMem());
      return var;
    }

    case kOpIsInf: {
      var = writableVar(var);
      c->orpd(var.getXmm(), getConstantU64AsPD(MATHPRESSO_UINT64_C(0x8000000000000000)).getMem());
      c->cmpsd(var.getXmm(), getConstantU64(MATHPRESSO_UINT64_C(0xFF80000000000000)).getMem(), int(asmjit::kX86CmpEQ));
      c->andpd(var.getXmm(), getConstantD64AsPD(1.0).getMem());
      return var;
    }

    case kOpIsFinite: {
      var = writableVar(var);
      c->orpd(var.getXmm(), getConstantU64AsPD(MATHPRESSO_UINT64_C(0x8000000000000000)).getMem());
      c->cmpsd(var.getXmm(), getConstantD64(0.0).getMem(), int(asmjit::kX86CmpLE));
      c->andpd(var.getXmm(), getConstantD64AsPD(1.0).getMem());
      return var;
    }

    case kOpSignBit: {
      X86XmmVar result = c->newXmmSd();
      c->pshufd(result, registerVar(var).getXmm(), asmjit::X86Util::shuffle(3, 2, 1, 1));
      c->psrad(result, 31);
      c->andpd(result, getConstantD64AsPD(1.0).getMem());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpRound:
    case kOpRoundEven:
    case kOpTrunc:
    case kOpFloor:
    case kOpCeil: {
      if (enableSSE4_1 || op != kOpRound) {
        var = writableVar(var);
        inlineRound(var.getXmm(), var.getXmm(), op);
        return var;
      }

      break;
    }

    case kOpAbs: {
      X86XmmVar result = c->newXmmSd();
      c->emit(asmjit::kX86InstIdXorpd, result, result);
      c->emit(asmjit::kX86InstIdSubsd, result, var.getOperand());
      c->emit(asmjit::kX86InstIdMaxsd, result, var.getOperand());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpExp:
    case kOpLog:
    case kOpLog2:
    case kOpLog10:
      break;

    case kOpSqrt: {
      X86XmmVar result = c->newXmmSd();
      c->emit(asmjit::kX86InstIdSqrtsd, result, var.getOperand());
      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpFrac: {
      var = writableVar(var);
      X86XmmVar tmp = c->newXmmSd();

      if (enableSSE4_1) {
        c->emit(asmjit::kX86InstIdRoundsd, tmp, var.getOperand(), int(asmjit::kX86RoundDown | asmjit::kX86RoundInexact));
        c->emit(asmjit::kX86InstIdSubsd, var.getOperand(), tmp);
        return var;
      }
      else {
        // Pure SSE2 `frac()`, uses the same rounding trick as `floor()`.
        inlineRound(tmp, var.getXmm(), kOpFloor);
        c->subsd(var.getXmm(), tmp);
        return var;
      }
    }

    case kOpRecip: {
      X86XmmVar result = c->newXmmSd();
      c->emit(asmjit::kX86InstIdMovsd, result, getConstantD64(1.0).getOperand());
      c->emit(asmjit::kX86InstIdDivsd, result, var.getOperand());
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
  X86XmmVar result = c->newXmmSd();
  X86XmmVar args[1] = { registerVar(var).getXmm() };
  inlineCall(result, args, 1, JitUtils::getFuncByOp(op));

  return JitVar(result, JitVar::FLAG_NONE);
}

JitVar JitCompiler::onBinaryOp(AstBinaryOp* node) {
  uint32_t op = node->getOp();

  AstNode* left  = node->getLeft();
  AstNode* right = node->getRight();

  if (op == kOpAssign) {
    AstVar* varNode = reinterpret_cast<AstVar*>(left);
    MATHPRESSO_ASSERT(varNode->getNodeType() == kAstNodeVar);

    AstSymbol* sym = varNode->getSymbol();
    JitVar result = registerVar(onNode(right));

    if (sym->getVarSlot() == 0xFFFFFFFF)
      c->emit(asmjit::kX86InstIdMovsd,
        asmjit::x86::ptr(variablesAddress, sym->getVarOffset()), result.getOperand());

    return result;
  }

  // Handle the case that the operands are the same variable.
  JitVar vl, vr;
  if (left->getNodeType() == kAstNodeVar &&
      right->getNodeType() == kAstNodeVar &&
      static_cast<AstVar*>(left)->getSymbol() == static_cast<AstVar*>(right)->getSymbol()) {
    vl = vr = writableVar(onNode(node->getLeft()));
  }
  else {
    vl = onNode(node->getLeft());
    vr = onNode(node->getRight());

    // Commutativity.
    if (op == kOpAdd || op == kOpMul || op == kOpAvg || op == kOpMin || op == kOpMax) {
      if (vl.isRO() && !vr.isRO())
        vl.swapWith(vr);
    }

    vl = writableVar(vl);
  }

  uint32_t inst = 0;
  int predicate = 0;

  switch (op) {
    case kOpEq: predicate = asmjit::kX86CmpEQ ; goto emitCompare;
    case kOpNe: predicate = asmjit::kX86CmpNEQ; goto emitCompare;
    case kOpGt: predicate = asmjit::kX86CmpNLE; goto emitCompare;
    case kOpGe: predicate = asmjit::kX86CmpNLT; goto emitCompare;
    case kOpLt: predicate = asmjit::kX86CmpLT ; goto emitCompare;
    case kOpLe: predicate = asmjit::kX86CmpLE ; goto emitCompare;
emitCompare: {
      vl = writableVar(vl);
      c->emit(asmjit::kX86InstIdCmpsd, vl.getXmm(), vr.getOperand(), predicate);
      c->emit(asmjit::kX86InstIdAndpd, vl.getXmm(), getConstantD64AsPD(1.0).getOperand());
      return vl;
    }

    case kOpAdd: inst = asmjit::kX86InstIdAddsd; goto emitInst;
    case kOpSub: inst = asmjit::kX86InstIdSubsd; goto emitInst;
    case kOpMul: inst = asmjit::kX86InstIdMulsd; goto emitInst;
    case kOpDiv: inst = asmjit::kX86InstIdDivsd; goto emitInst;
    case kOpMin: inst = asmjit::kX86InstIdMinsd; goto emitInst;
    case kOpMax: inst = asmjit::kX86InstIdMaxsd; goto emitInst;
emitInst: {
      c->emit(inst, vl.getOperand(), vr.getOperand()); return vl;
    }

    case kOpAvg: {
      vl = writableVar(vl);
      c->emit(asmjit::kX86InstIdAddsd, vl.getXmm(), vr.getOperand());
      c->emit(asmjit::kX86InstIdMulsd, vl.getXmm(), getConstantD64(0.5).getOperand());
      return vl;
    }

    case kOpMod: {
      X86XmmVar result = c->newXmmSd();
      X86XmmVar tmp = c->newXmmSd();

      vl = writableVar(vl);
      vr = registerVar(vr);

      c->movsd(result, vl.getXmm());
      c->divsd(vl.getXmm(), vr.getXmm());
      inlineRound(vl.getXmm(), vl.getXmm(), kOpTrunc);
      c->mulsd(vl.getXmm(), vr.getXmm());
      c->subsd(result, vl.getXmm());

      return JitVar(result, JitVar::FLAG_NONE);
    }

    case kOpPow:
    case kOpAtan2:
    case kOpHypot:
      break;

    case kOpCopySign: {
      vl = writableVar(vl);
      vr = writableVar(vr);

      c->andpd(vl.getXmm(), getConstantU64AsPD(MATHPRESSO_UINT64_C(0x7FFFFFFFFFFFFFFF)).getMem());
      c->andpd(vr.getXmm(), getConstantU64AsPD(MATHPRESSO_UINT64_C(0x8000000000000000)).getMem());
      c->orpd(vl.getXmm(), vr.getXmm());

      return vl;
    }

    default:
      MATHPRESSO_ASSERT_NOT_REACHED();
      return vl;
  }

  // No inline implementation -> function call.
  X86XmmVar result = c->newXmmSd();
  X86XmmVar args[2] = { registerVar(vl).getXmm(), registerVar(vr).getXmm() };
  inlineCall(result, args, 2, JitUtils::getFuncByOp(op));

  return JitVar(result, JitVar::FLAG_NONE);
}

JitVar JitCompiler::onCall(AstCall* node) {
  uint32_t i, count = node->getLength();
  AstSymbol* sym = node->getSymbol();

  X86XmmVar result = c->newXmmSd();
  X86XmmVar args[8];

  for (i = 0; i < count; i++)
    args[i] = registerVar(onNode(node->getAt(i))).getXmm();

  inlineCall(result, args, count, sym->getFuncPtr());
  return JitVar(result, JitVar::FLAG_NONE);
}

void JitCompiler::inlineRound(const X86XmmVar& dst, const X86XmmVar& src, uint32_t op) {
  // SSE4.1 implementation is easy except `round()`, which is not `roundeven()`.
  if (enableSSE4_1) {
    if (op == kOpRound) {
      X86XmmVar tmp = c->newXmmSd();
      c->roundsd(tmp, src, asmjit::kX86RoundDown | asmjit::kX86RoundInexact);

      if (dst.getId() != src.getId())
        c->movsd(dst, src);

      c->subsd(dst, tmp);
      c->cmpsd(dst, getConstantD64(0.5).getMem(), asmjit::kX86CmpNLT);
      c->andpd(dst, getConstantD64AsPD(1.0).getMem());
      c->addpd(dst, tmp);
    }
    else {
      int predicate = 0;

      switch (op) {
        case kOpRoundEven: predicate = asmjit::kX86RoundNearest; break;
        case kOpTrunc    : predicate = asmjit::kX86RoundTrunc  ; break;
        case kOpFloor    : predicate = asmjit::kX86RoundDown   ; break;
        case kOpCeil     : predicate = asmjit::kX86RoundUp     ; break;

        default:
          MATHPRESSO_ASSERT_NOT_REACHED();
      }
      c->roundsd(dst, src, predicate | asmjit::kX86RoundInexact);
    }
    return;
  }

  // Pure SSE2 requires the following rounding trick:
  //   double roundeven(double x) {
  //     double maxn  = pow(2, 52);
  //     double magic = pow(2, 52) + pow(2, 51);
  //     return x >= maxn ? x : x + magic - magic;
  //   }
  const double maxn = 4503599627370496.0;
  const double magic = 6755399441055744.0;

  if (op == kOpRoundEven) {
    X86XmmVar t1 = c->newXmmSd();
    X86XmmVar t2 = c->newXmmSd();

    c->movsd(t1, src);
    c->movsd(t2, src);

    c->addsd(t1, getConstantD64(magic).getMem());
    c->cmpsd(t2, getConstantD64(maxn).getMem(), int(asmjit::kX86CmpNLT));
    c->subsd(t1, getConstantD64(magic).getMem());

    // Combine the result.
    if (dst.getId() != src.getId())
      c->movsd(dst, src);

    c->andpd(dst, t2);
    c->andnpd(t2, t1);
    c->orpd(dst, t2);

    return;
  }

  // The `roundeven()` function can be used to implement efficiently the
  // remaining rounding functions. The easiest are `floor()` and `ceil()`.
  if (op == kOpFloor || op == kOpCeil) {
    X86XmmVar t1 = c->newXmmSd();
    X86XmmVar t2 = c->newXmmSd();
    X86XmmVar t3 = c->newXmmSd();

    c->movsd(t2, src);
    c->movsd(t3, src);

    if (dst.getId() != src.getId())
      c->movsd(dst, src);

    if (op == kOpFloor) {
      c->addsd(t2, getConstantD64(magic).getMem());
      c->movsd(t1, src);

      c->subsd(t2, getConstantD64(magic).getMem());
      c->cmpsd(t1, getConstantD64(maxn).getMem(), int(asmjit::kX86CmpNLT));

      c->cmpsd(t3, t2, int(asmjit::kX86CmpLT));
      c->andpd(t3, getConstantD64AsPD(1.0).getMem());

      c->andpd(dst, t1);
      c->subpd(t2, t3);
    }
    else {
      c->addsd(t2, getConstantD64(magic).getMem());
      c->movsd(t1, src);

      c->subsd(t2, getConstantD64(magic).getMem());
      c->cmpsd(t1, getConstantD64(maxn).getMem(), int(asmjit::kX86CmpNLT));

      c->cmpsd(t3, t2, int(asmjit::kX86CmpNLE));
      c->andpd(t3, getConstantD64AsPD(1.0).getMem());

      c->andpd(dst, t1);
      c->addpd(t2, t3);
    }

    c->andnpd(t1, t2);
    c->orpd(dst, t1);

    return;
  }

  if (op == kOpTrunc) {
    X86XmmVar t1 = c->newXmmSd();
    X86XmmVar t2 = c->newXmmSd();
    X86XmmVar t3 = c->newXmmSd();

    c->movsd(t2, getConstantU64(ASMJIT_UINT64_C(0x7FFFFFFFFFFFFFFF)).getMem());
    c->andpd(t2, src);

    if (dst.getId() != src.getId())
      c->movsd(dst, src);

    c->movsd(t1, t2);
    c->addsd(t2, getConstantD64(magic).getMem());
    c->movsd(t3, t1);

    c->subsd(t2, getConstantD64(magic).getMem());
    c->cmpsd(t1, getConstantD64(maxn).getMem(), int(asmjit::kX86CmpNLT));

    c->cmpsd(t3, t2, int(asmjit::kX86CmpLT));
    c->orpd(t1, getConstantU64AsPD(ASMJIT_UINT64_C(0x8000000000000000)).getMem());
    c->andpd(t3, getConstantD64AsPD(1.0).getMem());

    c->andpd(dst, t1);
    c->subpd(t2, t3);

    c->andnpd(t1, t2);
    c->orpd(dst, t1);

    return;
  }

  inlineCall(dst, &src, 1, (void*)(Arg1Func)mpRound);
}

void JitCompiler::inlineCall(const X86XmmVar& dst, const X86XmmVar* args, uint32_t count, void* fn) {
  uint32_t i;

  // Use function builder to build a function prototype.
  asmjit::FuncBuilderX builder;
  builder.setRetT<double>();

  for (i = 0; i < count; i++)
    builder.addArgT<double>();

  // Create the function call.
  asmjit::X86CallNode* ctx = c->call((asmjit::Ptr)fn, builder);
  ctx->setRet(0, dst);

  for (i = 0; i < count; i++)
    ctx->setArg(static_cast<uint32_t>(i), args[i]);
}

void JitCompiler::prepareConstPool() {
  if (!constLabel.isInitialized()) {
    constLabel = c->newLabel();

    asmjit::HLNode* prev = c->setCursor(functionBody);
    c->lea(constPtr, asmjit::x86::ptr(constLabel));
    if (prev != functionBody) c->setCursor(prev);
  }
}

JitVar JitCompiler::getConstantU64(uint64_t value) {
  prepareConstPool();

  size_t offset;
  if (constPool.add(&value, sizeof(uint64_t), offset) != asmjit::kErrorOk)
    return JitVar();

  return JitVar(asmjit::x86::ptr(constPtr, static_cast<int>(offset)), JitVar::FLAG_RO);
}

JitVar JitCompiler::getConstantU64AsPD(uint64_t value) {
  prepareConstPool();

  size_t offset;
  asmjit::Vec128 vec = asmjit::Vec128::fromSq(value, 0);
  if (constPool.add(&vec, sizeof(asmjit::Vec128), offset) != asmjit::kErrorOk)
    return JitVar();

  return JitVar(asmjit::x86::ptr(constPtr, static_cast<int>(offset)), JitVar::FLAG_RO);
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

CompiledFunc mpCompileFunction(AstBuilder* ast, uint32_t options, OutputLog* log) {
  asmjit::StringLogger logger;
  asmjit::X86Assembler a(&jitGlobal.runtime);
  asmjit::X86Compiler c(&a);

  bool debugAsm = log != NULL && (options & kOptionDebugAsm) != 0;
  if (debugAsm) {
    logger.setOption(asmjit::kLoggerOptionBinaryForm, true);
    a.setLogger(&logger);
  }

  JitCompiler jitCompiler(ast->getAllocator(), &c);

  if ((options & kOptionDisableSSE4_1) != 0)
    jitCompiler.enableSSE4_1 = false;

  jitCompiler.beginFunction();
  jitCompiler.compile(ast->getProgramNode(), ast->_numSlots);
  jitCompiler.endFunction();

  c.finalize();
  CompiledFunc fn = asmjit_cast<CompiledFunc>(a.make());

  if (debugAsm)
    log->log(OutputLog::kMessageAsm, 0, 0, logger.getString(), logger._stringBuilder.getLength());

  return fn;
}

void mpFreeFunction(void* fn) {
  jitGlobal.runtime.release((void*)fn);
}

} // mathpresso namespace
