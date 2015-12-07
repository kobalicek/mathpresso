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
#include "mathpresso_optimizer_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

MPOptimizer::MPOptimizer(WorkContext& ctx)
  : _ctx(ctx) {}
MPOptimizer::~MPOptimizer() {}

ASTNode* MPOptimizer::onNode(ASTNode* node) {
  switch (node->getNodeType()) {
    case kMPNodeBlock   : return onBlock(static_cast<ASTBlock*>(node));
    case kMPNodeUnaryOp : return onUnaryOp(static_cast<ASTUnaryOp*>(node));
    case kMPNodeBinaryOp: return onBinaryOp(static_cast<ASTBinaryOp*>(node));
    case kMPNodeCall    : return onCall(static_cast<ASTCall*>(node));

    default:
      return node;
  }
}

ASTNode* MPOptimizer::onBlock(ASTBlock* node) {
  ASTNode** children = node->getChildNodes();
  size_t count = node->getChildrenCount();

  for (size_t i = 0; i < count; i++)
    children[i] = onNode(children[i]);

  return node;
}

ASTNode* MPOptimizer::onUnaryOp(ASTUnaryOp* node) {
  ASTNode* child = node->_child = onNode(node->getChild());
  bool childConst = child->isConstant();

  if (childConst) {
    // Child is constant, simplify it.
    double result = node->evaluate(NULL);

    ASTImmediate* replacement = new ASTImmediate(_ctx.genId(), result);
    replacement->_parent = node->getParent();
    delete node;

    return replacement;
  }
  else {
    // Simplify "-(-(x))" to "x".
    if (child->isUnaryOp()) {
      ASTUnaryOp* u = static_cast<ASTUnaryOp*>(child);
      if (node->getUnaryType() == kMPUnaryOpNegate && u->getUnaryType() == kMPUnaryOpNegate) {
        ASTNode* replacement = u->getChild();
        u->resetChild();
        replacement->_parent = node->getParent();
        delete node;

        return replacement;
      }
    }
  }

  return node;
}

ASTNode* MPOptimizer::onBinaryOp(ASTBinaryOp* node) {
  ASTNode* left;
  ASTNode* right;

  left = node->_left = onNode(node->getLeft());
  right = node->_right = onNode(node->getRight());

  bool leftConst = left->isConstant();
  bool rightConst = right->isConstant();

  if (leftConst && rightConst) {
    // Both are constants, simplify them.
    double result = node->evaluate(NULL);

    ASTImmediate* replacement = new ASTImmediate(_ctx.genId(), result);
    replacement->_parent = node->getParent();
    delete node;
    return replacement;
  }
  else if (leftConst || rightConst) {
    // Left or right is constant, we try to find a deeper one that can be
    // joined.
    ASTNode* c;
    ASTNode* x;

    if (leftConst) {
      c = left;
      x = right;
    }
    else {
      c = right;
      x = left;
    }

    ASTNode* y = findConstNode(x, node->getBinaryType());
    if (y != NULL) {
      ASTBinaryOp* p = reinterpret_cast<ASTBinaryOp*>(y->getParent());
      MP_ASSERT(p->getNodeType() == kMPNodeBinaryOp);

      ASTNode* keep;
      double result;

      // Hardcoded evaluator (we accept only ADD and MUL operators).
      switch (node->getBinaryType()) {
        case kMPBinaryOpAdd:
          result = c->evaluate(NULL) + y->evaluate(NULL);
          break;
        case kMPBinaryOpMul:
          result = c->evaluate(NULL) * y->evaluate(NULL);
          break;
        default:
          MP_ASSERT_NOT_REACHED();
      }

      if (p->getRight() == y) {
        keep = p->getLeft();
        p->_left = NULL;
      }
      else {
        keep = p->getRight();
        p->_right = NULL;
      }
      p->getParent()->replaceChild(p, keep);
      delete p;

      reinterpret_cast<ASTImmediate*>(c)->setValue(result);
    }
  }

  return node;
}

ASTNode* MPOptimizer::onCall(ASTCall* node) {
  ASTNode** children = node->getChildNodes();
  size_t count = node->getChildrenCount();

  bool allConst = true;
  for (size_t i = 0; i < count; i++) {
    ASTNode* arg = onNode(children[i]);
    children[i] = arg;
    allConst &= arg->isConstant();
  }

  if (allConst) {
    double result = node->evaluate(NULL);

    ASTImmediate* replacement = new ASTImmediate(_ctx.genId(), result);
    replacement->_parent = node->getParent();
    delete node;

    return replacement;
  }

  return node;
}

ASTNode* MPOptimizer::findConstNode(ASTNode* _node, int op) {
  if (_node->getNodeType() != kMPNodeBinaryOp) return NULL;
  ASTBinaryOp* node = reinterpret_cast<ASTBinaryOp*>(_node);

  int op0 = op;
  int op1 = node->getBinaryType();

  if ((op0 == kMPBinaryOpAdd && op1 == kMPBinaryOpAdd) ||
      (op0 == kMPBinaryOpMul && op1 == kMPBinaryOpMul)) {
    ASTNode* left = node->getLeft();
    ASTNode* right = node->getRight();

    if (left->isConstant()) return left;
    if (right->isConstant()) return right;

    if ((left = findConstNode(left, op)) != NULL) return left;
    if ((right = findConstNode(right, op)) != NULL) return right;
  }

  return NULL;
}

} // mathpresso namespace
