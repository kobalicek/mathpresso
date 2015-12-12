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
#include "mathpresso_jit_p.h"
#include "mathpresso_util_p.h"

#include <asmjit/asmjit.h>

namespace mathpresso {

// ============================================================================
// [mathpresso::MPJITGlobal]
// ============================================================================

struct MPJITGlobal {
  asmjit::JitRuntime runtime;
};
static MPJITGlobal jitGlobal;

// ============================================================================
// [mathpresso::MPJITVar]
// ============================================================================

struct MATHPRESSO_NOAPI MPJITVar {
  enum FLAGS {
    FLAG_NONE = 0,
    FLAG_RO = 1
  };

  inline MPJITVar() : op(), flags(FLAG_NONE) {}
  inline MPJITVar(asmjit::Operand op, uint32_t flags) : op(op), flags(flags) {}
  inline MPJITVar(const MPJITVar& other) : op(other.op), flags(other.flags) {}
  inline ~MPJITVar() {}

  // Operator Overload.
  inline const MPJITVar& operator=(const MPJITVar& other) {
    op = other.op;
    flags = other.flags;
    return *this;
  }

  // Swap.
  void swapWith(MPJITVar& other) {
    MPJITVar t(*this);
    *this = other;
    other = t;
  }

  // Operand management.
  inline const asmjit::Operand& getOperand() const { return op; }
  inline const asmjit::X86Mem& getMem() const { return *static_cast<const asmjit::X86Mem*>(&op); }
  inline const asmjit::X86XmmVar& getXmm() const { return *static_cast<const asmjit::X86XmmVar*>(&op); }

  inline bool isNone() const { return op.isNone(); }
  inline bool isMem() const { return op.isMem(); }
  inline bool isXmm() const { return op.isRegType(asmjit::kX86RegTypeXmm); }

  // Flags.
  inline bool isRO() const { return (flags & FLAG_RO) != 0; }

  // Members.
  asmjit::Operand op;
  uint32_t flags;
};

// ============================================================================
// [mathpresso::MPJITConst]
// ============================================================================

struct MATHPRESSO_NOAPI MPJITConst {
  inline MPJITConst(const MPJITVar& var, int64_t value) : var(var), value(value) {}

  MPJITVar var;
  int64_t value;
};

// ============================================================================
// [mathpresso::MPJITCompiler]
// ============================================================================

struct MATHPRESSO_NOAPI MPJITCompiler {
  MPJITCompiler(WorkContext& ctx, asmjit::X86Compiler* c);
  ~MPJITCompiler();

  // Function Generator.
  void beginFunction();
  void endFunction();

  // Variable Management.
  MPJITVar copyVar(const MPJITVar& other);
  MPJITVar writableVar(const MPJITVar& other);
  MPJITVar registerVar(const MPJITVar& other);

  // Compiler.
  void onTree(ASTNode* tree);
  MPJITVar onNode(ASTNode* node);
  MPJITVar onBlock(ASTBlock* node);
  MPJITVar onConstant(ASTImmediate* node);
  MPJITVar onVariable(ASTVariable* node);
  MPJITVar onUnary(ASTUnaryOp* node);
  MPJITVar onOperator(ASTBinaryOp* node);
  MPJITVar onCall(ASTCall* node);

  // Constants.
  void prepareConstPool();
  MPJITVar getConstantI64(int64_t value);
  MPJITVar getConstantI64AsPD(int64_t value);
  MPJITVar getConstantD64(double value);
  MPJITVar getConstantD64AsPD(double value);

  // Members.
  WorkContext& ctx;
  asmjit::X86Compiler* c;

  asmjit::X86GpVar resultAddress;
  asmjit::X86GpVar variablesAddress;
  asmjit::HLNode* functionBody;

  asmjit::Label constLabel;
  asmjit::X86GpVar constPtr;
  asmjit::ConstPool constPool;
};

MPJITCompiler::MPJITCompiler(WorkContext& ctx, asmjit::X86Compiler* c)
  : ctx(ctx),
    c(c),
    constPool(&c->_constAllocator) {}
MPJITCompiler::~MPJITCompiler() {}

void MPJITCompiler::beginFunction() {
  c->addFunc(asmjit::FuncBuilder3<asmjit::Void, const void*, double*, double*>(asmjit::kCallConvHostCDecl));
  c->getFunc()->setHint(asmjit::kFuncHintNaked, true);

  resultAddress = c->newIntPtr("pResult");
  variablesAddress = c->newIntPtr("pVariables");
  constPtr = c->newIntPtr("pConst");

  c->setArg(0, c->newIntPtr());
  c->setArg(1, resultAddress);
  c->setArg(2, variablesAddress);

  c->setPriority(variablesAddress, 1);
  c->setPriority(constPtr, 2);

  functionBody = c->getCursor();
}

void MPJITCompiler::endFunction() {
  c->endFunc();

  if (constLabel.isInitialized())
    c->embedConstPool(constLabel, constPool);
}

MPJITVar MPJITCompiler::copyVar(const MPJITVar& other) {
  MPJITVar v(c->newXmmSd(), MPJITVar::FLAG_NONE);
  c->emit(asmjit::kX86InstIdMovsd, v.getXmm(), other.getOperand());
  return v;
}

MPJITVar MPJITCompiler::writableVar(const MPJITVar& other) {
  if (other.isMem() || other.isRO())
    return copyVar(other);
  else
    return other;
}

MPJITVar MPJITCompiler::registerVar(const MPJITVar& other) {
  if (other.isMem())
    return copyVar(other);
  else
    return other;
}

void MPJITCompiler::onTree(ASTNode* tree) {
  MPJITVar result = registerVar(onNode(tree));
  c->movsd(asmjit::x86::ptr(resultAddress), result.getXmm());
}

MPJITVar MPJITCompiler::onNode(ASTNode* node) {
  switch (node->getNodeType()) {
    case kMPNodeBlock:
      return onBlock(reinterpret_cast<ASTBlock*>(node));
    case kMPNodeImmediate:
      return onConstant(reinterpret_cast<ASTImmediate*>(node));
    case kMPNodeVariable:
      return onVariable(reinterpret_cast<ASTVariable*>(node));
    case kMPNodeUnaryOp:
      return onUnary(reinterpret_cast<ASTUnaryOp*>(node));
    case kMPNodeBinaryOp:
      return onOperator(reinterpret_cast<ASTBinaryOp*>(node));
    case kMPNodeCall:
      return onCall(reinterpret_cast<ASTCall*>(node));
    default:
      MP_ASSERT_NOT_REACHED();
      return MPJITVar();
  }
}

MPJITVar MPJITCompiler::onBlock(ASTBlock* node) {
  MPJITVar result;

  size_t i, len = node->_nodes.getLength();
  for (i = 0; i < len; i++) {
    result = onNode(node->_nodes[i]);
  }

  return result;
}

MPJITVar MPJITCompiler::onConstant(ASTImmediate* node) {
  return getConstantD64(node->getValue());
}

MPJITVar MPJITCompiler::onVariable(ASTVariable* node) {
  return MPJITVar(asmjit::x86::ptr(variablesAddress, (int)node->getOffset()), MPJITVar::FLAG_RO);
}

MPJITVar MPJITCompiler::onUnary(ASTUnaryOp* node) {
  uint op = node->getUnaryType();
  MPJITVar var = onNode(node->getChild());

  if (op != kMPOpNone)
    var = writableVar(var);

  switch (op) {
    case kMPOpNone:
      break;
    case kMPOpNegate: {
      asmjit::XmmVar t(c->newXmmSd());
      c->emit(asmjit::kX86InstIdXorpd, var.getOperand(), getConstantI64AsPD(ASMJIT_UINT64_C(0x8000000000000000)).getOperand());
      break;
    }
  }

  return var;
}

MPJITVar MPJITCompiler::onOperator(ASTBinaryOp* node) {
  uint operatorType = node->getBinaryType();

  MPJITVar vl;
  MPJITVar vr;

  ASTNode* left  = node->getLeft();
  ASTNode* right = node->getRight();

  if (operatorType == kMPOpAssign) {
    ASTVariable* varNode = reinterpret_cast<ASTVariable*>(left);
    MP_ASSERT(varNode->getNodeType() == kMPNodeVariable);

    vr = registerVar(onNode(right));
    c->emit(asmjit::kX86InstIdMovsd,
      asmjit::x86::ptr(variablesAddress, (int)varNode->getOffset()), vr.getOperand());
    return vr;
  }

  if (left->getNodeType() == kMPNodeVariable && right->getNodeType() == kMPNodeVariable &&
      reinterpret_cast<ASTVariable*>(left)->getOffset() == reinterpret_cast<ASTVariable*>(right)->getOffset()) {
    // vl OP vr, emit:
    //
    //   LOAD vl
    //   INST vl, vl
    vl = writableVar(onNode(node->getLeft()));
    vr = vl;
  }
  else {
    // vl OP vr, emit:
    //
    //   LOAD vl
    //   LOAD vr
    //   INST vl, vr OR vr, vl
    vl = onNode(node->getLeft());
    vr = onNode(node->getRight());

    // Commutativity (PLUS and MUL operators).
    if (operatorType == kMPOpAdd || operatorType == kMPOpMul) {
      if (vl.isRO() && !vr.isRO()) vl.swapWith(vr);
    }

    vl = writableVar(vl);
  }

  int predicate = 0;

  switch (operatorType) {
    case kMPOpEq: predicate = asmjit::kX86CmpEQ ; goto compare;
    case kMPOpNe: predicate = asmjit::kX86CmpNEQ; goto compare;
    case kMPOpGt: predicate = asmjit::kX86CmpNLE; goto compare;
    case kMPOpGe: predicate = asmjit::kX86CmpNLT; goto compare;
    case kMPOpLt: predicate = asmjit::kX86CmpLT ; goto compare;
    case kMPOpLe: predicate = asmjit::kX86CmpLE ; goto compare;
compare:
      c->emit(asmjit::kX86InstIdCmpsd, vl.getOperand(), vr.getOperand(), predicate);
      c->emit(asmjit::kX86InstIdAndpd, vl.getOperand(), getConstantD64AsPD(1.0).getOperand());
      return vl;

    case kMPOpAdd: c->emit(asmjit::kX86InstIdAddsd, vl.getOperand(), vr.getOperand()); return vl;
    case kMPOpSub: c->emit(asmjit::kX86InstIdSubsd, vl.getOperand(), vr.getOperand()); return vl;
    case kMPOpMul: c->emit(asmjit::kX86InstIdMulsd, vl.getOperand(), vr.getOperand()); return vl;
    case kMPOpDiv: c->emit(asmjit::kX86InstIdDivsd, vl.getOperand(), vr.getOperand()); return vl;

    case kMPOpMod: {
      asmjit::XmmVar args[8];
      MPJITVar result(c->newXmmSd(), MPJITVar::FLAG_NONE);

      // Create the function call.
      asmjit::X86CallNode* ctx = c->call((asmjit::Ptr)(MFunc_Ret_D_ARG2)fmod, 
        asmjit::FuncBuilder2<double, double, double>(asmjit::kCallConvHostCDecl));

      vl = registerVar(vl);
      vr = registerVar(vr);

      ctx->setArg(0, vl.getXmm());
      ctx->setArg(1, vr.getXmm());

      ctx->setRet(0, vl.getXmm());
      return vl;
    }

    default:
      MP_ASSERT_NOT_REACHED();
      return vl;
  }
}

MPJITVar MPJITCompiler::onCall(ASTCall* node) {
  const Vector<ASTNode*>& arguments = node->getArguments();
  size_t i, len = arguments.getLength();
  
  Function* fn = node->getFunction();
  int funcId = fn->getFunctionId();

  switch (funcId) {
    // Intrinsics.
    case kMPFunctionMin:
    case kMPFunctionMax:
    case kMPFunctionAvg: {
      MP_ASSERT(len == 2);

      MPJITVar vl(onNode(arguments[0]));
      MPJITVar vr(onNode(arguments[1]));

      if (vl.isRO() && !vr.isRO())
        vl.swapWith(vr);
      vl = writableVar(vl);

      switch (funcId) {
        case kMPFunctionMin:
          c->emit(asmjit::kX86InstIdMinsd, vl.getOperand(), vr.getOperand());
          break;
        case kMPFunctionMax:
          c->emit(asmjit::kX86InstIdMaxsd, vl.getOperand(), vr.getOperand());
          break;
        case kMPFunctionAvg:
          c->emit(asmjit::kX86InstIdAddsd, vl.getOperand(), vr.getOperand());
          c->emit(asmjit::kX86InstIdMulsd, vl.getOperand(), getConstantD64(0.5).getOperand());
          break;
      }

      return vl;
    }

    case kMPFunctionRecip: {
      MPJITVar r(c->newXmmSd(), MPJITVar::FLAG_NONE);
      MPJITVar u(onNode(arguments[0]));

      c->emit(asmjit::kX86InstIdMovsd, getConstantD64(1.0).getOperand());
      c->emit(asmjit::kX86InstIdDivsd, u.getOperand());

      return u;
    }

    case kMPFunctionAbs:
    case kMPFunctionSqrt: {
      MP_ASSERT(len == 1);

      MPJITVar u(writableVar(onNode(arguments[0])));
      switch (funcId) {
        case kMPFunctionAbs:
          c->emit(asmjit::kX86InstIdAndpd, u.getOperand(), registerVar(getConstantI64AsPD(ASMJIT_UINT64_C(0x8000000000000000))).getOperand());
          break;
        case kMPFunctionSqrt:
          c->emit(asmjit::kX86InstIdSqrtsd, u.getOperand(), u.getOperand());
          break;
      }

      return u;
    }

    // Function call.
    default: {
      asmjit::XmmVar args[8];
      MPJITVar result(c->newXmmSd(), MPJITVar::FLAG_NONE);

      for (i = 0; i < len; i++) {
        MPJITVar tmp = onNode(arguments[i]);

        if (tmp.isXmm()) {
          args[i] = tmp.getXmm();
        }
        else {
          args[i] = c->newXmmSd();
          c->emit(asmjit::kX86InstIdMovsd, args[i], tmp.getOperand());
        }
      }

      // Use function builder to build a function prototype.
      asmjit::FuncBuilderX builder;
      for (i = 0; i < len; i++)
        builder.addArgT<double>();
      builder.setRetT<double>();

      // Create the function call.
      asmjit::X86CallNode* ctx = c->call((asmjit::Ptr)node->getFunction()->getPtr(), builder);
      for (i = 0; i < len; i++)
        ctx->setArg(static_cast<unsigned int>(i), args[i]);
      ctx->setRet(0, result.getXmm());
      return result;
    }
  }
}

//! \internal
union DoubleBits {
  //! 64-bit signed integer value.
  int64_t i64;
  //! 64-bit double-precision floating point.
  double d64;
};

void MPJITCompiler::prepareConstPool() {
  if (!constLabel.isInitialized()) {
    constLabel = c->newLabel();

    asmjit::HLNode* prev = c->setCursor(functionBody);
    c->lea(constPtr, asmjit::x86::ptr(constLabel));
    if (prev != functionBody) c->setCursor(prev);
  }
}

MPJITVar MPJITCompiler::getConstantI64(int64_t value) {
  prepareConstPool();

  size_t offset;
  if (constPool.add(&value, sizeof(int64_t), offset) != asmjit::kErrorOk)
    return MPJITVar();

  return MPJITVar(asmjit::x86::ptr(constPtr, static_cast<int>(offset)), MPJITVar::FLAG_RO);
}

MPJITVar MPJITCompiler::getConstantI64AsPD(int64_t value) {
  prepareConstPool();

  size_t offset;
  asmjit::Vec128 vec = asmjit::Vec128::fromSq(value, 0);
  if (constPool.add(&vec, sizeof(asmjit::Vec128), offset) != asmjit::kErrorOk)
    return MPJITVar();

  return MPJITVar(asmjit::x86::ptr(constPtr, static_cast<int>(offset)), MPJITVar::FLAG_RO);
}

MPJITVar MPJITCompiler::getConstantD64(double value) {
  DoubleBits u;
  u.d64 = value;
  return getConstantI64(u.i64);
}

MPJITVar MPJITCompiler::getConstantD64AsPD(double value) {
  DoubleBits u;
  u.d64 = value;
  return getConstantI64AsPD(u.i64);
}

MPEvalFunc mpCompileFunction(WorkContext& ctx, ASTNode* tree, bool dumpJIT) {
  asmjit::StringLogger logger;
  asmjit::X86Assembler a(&jitGlobal.runtime);
  asmjit::X86Compiler c(&a);

  if (dumpJIT) {
    logger.setOption(asmjit::kLoggerOptionBinaryForm, true);
    a.setLogger(&logger);
  }

  MPJITCompiler jitCompiler(ctx, &c);
  jitCompiler.beginFunction();
  jitCompiler.onTree(tree);
  jitCompiler.endFunction();

  c.finalize();
  MPEvalFunc fn = asmjit_cast<MPEvalFunc>(a.make());

  if (dumpJIT) {
    const char* content = logger.getString();
    printf("%s\n", content);
  }

  return fn;
}

void mpFreeFunction(void* fn) {
  jitGlobal.runtime.release((void*)fn);
}

} // mathpresso namespace
