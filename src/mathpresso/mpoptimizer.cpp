// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mpast_p.h"
#include "./mpeval_p.h"
#include "./mpoptimizer_p.h"

namespace mathpresso {

// ============================================================================
// [mpsl::AstOptimizer - Construction / Destruction]
// ============================================================================

AstOptimizer::AstOptimizer(AstBuilder* ast, ErrorReporter* errorReporter)
  : AstVisitor(ast),
    _errorReporter(errorReporter) {}
AstOptimizer::~AstOptimizer() {}

// ============================================================================
// [mpsl::AstOptimizer - OnNode]
// ============================================================================

Error AstOptimizer::onBlock(AstBlock* node) {
  // Prevent removing nodes that are not stored in pure `AstBlock`. For example
  // function call inherits from `AstBlock`, but it needs each expression passed.
  bool alterable = node->nodeType() == kAstNodeBlock;

  uint32_t i = 0;
  uint32_t curCount = node->size();
  uint32_t oldCount;

  while (i < curCount) {
    MATHPRESSO_PROPAGATE(onNode(node->childAt(i)));

    oldCount = curCount;
    curCount = node->size();

    if (curCount < oldCount) {
      if (!alterable)
        return MATHPRESSO_TRACE_ERROR(kErrorInvalidState);
      continue;
    }

    if (alterable && (node->childAt(i)->isImm())) {
      _ast->deleteNode(node->removeAt(i));
      curCount--;
      continue;
    }

    i++;
  }

  return kErrorOk;
}

Error AstOptimizer::onVarDecl(AstVarDecl* node) {
  AstSymbol* sym = node->symbol();

  if (node->child()) {
    MATHPRESSO_PROPAGATE(onNode(node->child()));
    AstNode* child = node->child();

    if (child->isImm())
      sym->setValue(static_cast<AstImm*>(child)->value());
  }

  return kErrorOk;
}

Error AstOptimizer::onVar(AstVar* node) {
  AstSymbol* sym = node->symbol();

  if (sym->isAssigned() && !node->hasNodeFlag(kAstNodeHasSideEffect)) {
    AstImm* imm = _ast->newNode<AstImm>(sym->value());
    _ast->deleteNode(node->parent()->replaceNode(node, imm));
  }

  return kErrorOk;
}

Error AstOptimizer::onImm(AstImm* node) {
  return kErrorOk;
}

Error AstOptimizer::onUnaryOp(AstUnaryOp* node) {
  const OpInfo& op = OpInfo::get(node->opType());

  MATHPRESSO_PROPAGATE(onNode(node->child()));
  AstNode* child = node->child();

  if (child->isImm()) {
    AstImm* child = static_cast<AstImm*>(node->child());
    double value = child->value();

    switch (node->opType()) {
      case kOpNeg      : value = -value; break;
      case kOpNot      : value = (value == 0); break;

      case kOpIsNan    : value = mpIsNan(value); break;
      case kOpIsInf    : value = mpIsInf(value); break;
      case kOpIsFinite : value = mpIsFinite(value); break;
      case kOpSignBit  : value = mpSignBit(value); break;

      case kOpRound    : value = mpRound(value); break;
      case kOpRoundEven: value = mpRoundEven(value); break;
      case kOpTrunc    : value = mpTrunc(value); break;
      case kOpFloor    : value = mpFloor(value); break;
      case kOpCeil     : value = mpCeil(value); break;

      case kOpAbs      : value = mpAbs(value); break;
      case kOpExp      : value = mpExp(value); break;
      case kOpLog      : value = mpLog(value); break;
      case kOpLog2     : value = mpLog2(value); break;
      case kOpLog10    : value = mpLog10(value); break;
      case kOpSqrt     : value = mpSqrt(value); break;
      case kOpFrac     : value = mpFrac(value); break;
      case kOpRecip    : value = mpRecip(value); break;

      case kOpSin      : value = mpSin(value); break;
      case kOpCos      : value = mpCos(value); break;
      case kOpTan      : value = mpTan(value); break;

      case kOpSinh     : value = mpSinh(value); break;
      case kOpCosh     : value = mpCosh(value); break;
      case kOpTanh     : value = mpTanh(value); break;

      case kOpAsin     : value = mpAsin(value); break;
      case kOpAcos     : value = mpAcos(value); break;
      case kOpAtan     : value = mpAtan(value); break;

      default:
        return _errorReporter->onError(kErrorInvalidState, node->position(),
          "Invalid unary operation '%s'.", op.name);
    }

    child->setValue(value);

    node->unlinkChild();
    node->parent()->replaceNode(node, child);

    _ast->deleteNode(node);
  }
  else if (child->nodeType() == kAstNodeUnaryOp && node->opType() == child->opType()) {
    // Simplify `-(-(x))` -> `x`.
    if (node->opType() == kOpNeg) {
      AstNode* childOfChild = static_cast<AstUnaryOp*>(child)->unlinkChild();
      node->parent()->replaceNode(node, childOfChild);
      _ast->deleteNode(node);
    }
  }

  return kErrorOk;
}

Error AstOptimizer::onBinaryOp(AstBinaryOp* node) {
  const OpInfo& op = OpInfo::get(node->opType());

  AstNode* left = node->left();
  AstNode* right = node->right();

  if (op.isAssignment())
    left->addNodeFlags(kAstNodeHasSideEffect);

  MATHPRESSO_PROPAGATE(onNode(left));
  left = node->left();

  MATHPRESSO_PROPAGATE(onNode(right));
  right = node->right();

  bool lIsImm = left->isImm();
  bool rIsImm = right->isImm();

  // If both nodes are values it's easy, just fold them into a single one.
  if (lIsImm && rIsImm) {
    AstImm* lNode = static_cast<AstImm*>(left);
    AstImm* rNode = static_cast<AstImm*>(right);

    double lVal = lNode->value();
    double rVal = rNode->value();
    double result = 0.0;

    switch (node->opType()) {
      case kOpEq      : result = lVal == rVal; break;
      case kOpNe      : result = lVal != rVal; break;
      case kOpLt      : result = lVal < rVal; break;
      case kOpLe      : result = lVal <= rVal; break;
      case kOpGt      : result = lVal > rVal; break;
      case kOpGe      : result = lVal >= rVal; break;
      case kOpAdd     : result = lVal + rVal; break;
      case kOpSub     : result = lVal - rVal; break;
      case kOpMul     : result = lVal * rVal; break;
      case kOpDiv     : result = lVal / rVal; break;
      case kOpMod     : result = mpMod(lVal, rVal); break;
      case kOpAvg     : result = mpAvg(lVal, rVal); break;
      case kOpMin     : result = mpMin(lVal, rVal); break;
      case kOpMax     : result = mpMax(lVal, rVal); break;
      case kOpPow     : result = mpPow(lVal, rVal); break;
      case kOpAtan2   : result = mpAtan2(lVal, rVal); break;
      case kOpHypot   : result = mpHypot(lVal, rVal); break;
      case kOpCopySign: result = mpCopySign(lVal, rVal); break;

      default:
        return _errorReporter->onError(kErrorInvalidState, node->position(),
          "Invalid binary operation '%s'.", op.name);
    }

    lNode->setValue(result);
    node->unlinkLeft();
    node->parent()->replaceNode(node, lNode);

    _ast->deleteNode(node);
  }
  // There is still a little optimization opportunity.
  else if (lIsImm) {
    AstImm* lNode = static_cast<AstImm*>(left);
    double val = lNode->value();

    if ((val == 0.0 && (op.flags & kOpFlagNopIfLZero)) ||
        (val == 1.0 && (op.flags & kOpFlagNopIfLOne))) {
      node->unlinkRight();
      node->parent()->replaceNode(node, right);

      _ast->deleteNode(node);
    }
  }
  else if (rIsImm) {
    AstImm* rNode = static_cast<AstImm*>(right);
    double val = rNode->value();

    // Evaluate an assignment.
    if (op.isAssignment() && left->isVar()) {
      AstSymbol* sym = static_cast<AstVar*>(left)->symbol();
      if (op.type == kOpAssign || sym->isAssigned()) {
        sym->setValue(val);
        sym->setAssigned();
      }
    }
    else {
      if ((val == 0.0 && (op.flags & kOpFlagNopIfRZero)) ||
          (val == 1.0 && (op.flags & kOpFlagNopIfROne))) {
        node->unlinkLeft();
        node->parent()->replaceNode(node, left);

        _ast->deleteNode(node);
      }
    }
  }

  return kErrorOk;
}

Error AstOptimizer::onCall(AstCall* node) {
  AstSymbol* sym = node->symbol();
  uint32_t i, count = node->size();

  bool allConst = true;
  for (i = 0; i < count; i++) {
    MATHPRESSO_PROPAGATE(onNode(node->childAt(i)));
    allConst &= node->childAt(i)->isImm();
  }

  if (allConst && count <= 8) {
    AstImm** args = reinterpret_cast<AstImm**>(node->children());

    void* fn = sym->funcPtr();
    double result = 0.0;

    #define ARG(n) args[n]->value()
    switch (count) {
      case 0: result = ((Arg0Func)fn)(); break;
      case 1: result = ((Arg1Func)fn)(ARG(0)); break;
      case 2: result = ((Arg2Func)fn)(ARG(0), ARG(1)); break;
      case 3: result = ((Arg3Func)fn)(ARG(0), ARG(1), ARG(2)); break;
      case 4: result = ((Arg4Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3)); break;
      case 5: result = ((Arg5Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4)); break;
      case 6: result = ((Arg6Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); break;
      case 7: result = ((Arg7Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6)); break;
      case 8: result = ((Arg8Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7)); break;
    }
    #undef ARG

    AstNode* replacement = _ast->newNode<AstImm>(result);
    node->parent()->replaceNode(node, replacement);
    _ast->deleteNode(node);
  }

  return kErrorOk;
}

} // mathpresso namespace
