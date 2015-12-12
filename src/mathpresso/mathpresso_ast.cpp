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
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::ASTNode]
// ============================================================================

ASTNode::ASTNode(unsigned int nodeId, unsigned int nodeType)
  : _parent(NULL),
    _nodeId(nodeId),
    _nodeType(nodeType) {}
ASTNode::~ASTNode() {}

bool ASTNode::isConstant() const {
  return false;
}

bool ASTNode::replaceChild(ASTNode* child, ASTNode* node) {
  ASTNode** children = getChildNodes();
  size_t length = getChildrenCount();

  for (size_t i = 0; i < length; i++) {
    if (children[i] == child) {
      children[i] = node;
      return true;
    }
  }

  return false;
}

// ============================================================================
// [mathpresso::ASTBlock]
// ============================================================================

ASTBlock::ASTBlock(unsigned int nodeId)
  : ASTNode(nodeId, kMPNodeBlock) {}

ASTBlock::~ASTBlock() {
  for (size_t i = 0; i < _nodes.getLength(); i++)
    delete _nodes[i];
}

bool ASTBlock::isConstant() const {
  return false;
}

ASTNode** ASTBlock::getChildNodes() const {
  return const_cast<ASTNode**>(_nodes.getData());
}

size_t ASTBlock::getChildrenCount() const {
  return _nodes.getLength();
}

double ASTBlock::evaluate(void* data) const {
  double result = 0;
  for (size_t i = 0; i < _nodes.getLength(); i++)
    result = _nodes[i]->evaluate(data);
  return result;
}

// ============================================================================
// [mathpresso::ASTImmediate]
// ============================================================================

ASTImmediate::ASTImmediate(unsigned int nodeId, double value)
  : ASTNode(nodeId, kMPNodeImmediate),
    _value(value) {}
ASTImmediate::~ASTImmediate() {}

bool ASTImmediate::isConstant() const {
  return true;
}

ASTNode** ASTImmediate::getChildNodes() const {
  return NULL;
}

size_t ASTImmediate::getChildrenCount() const {
  return 0;
}

bool ASTImmediate::replaceChild(ASTNode* child, ASTNode* node) {
  return false;
}

double ASTImmediate::evaluate(void* data) const {
  return _value;
}

// ============================================================================
// [mathpresso::ASTVariable]
// ============================================================================

ASTVariable::ASTVariable(unsigned int nodeId, const Variable* variable)
  : ASTNode(nodeId, kMPNodeVariable),
    _variable(variable) {}

ASTVariable::~ASTVariable() {}

bool ASTVariable::isConstant() const {
  return false;
}

ASTNode** ASTVariable::getChildNodes() const {
  return NULL;
}

size_t ASTVariable::getChildrenCount() const {
  return 0;
}

bool ASTVariable::replaceChild(ASTNode* child, ASTNode* node) {
  return false;
}

double ASTVariable::evaluate(void* data) const {
  return reinterpret_cast<double*>((char*)data + getOffset())[0];
}

// ============================================================================
// [mathpresso::ASTUnaryOp]
// ============================================================================

ASTUnaryOp::ASTUnaryOp(unsigned int nodeId)
  : ASTNode(nodeId, kMPNodeUnaryOp),
    _child(NULL),
    _unaryType(kMPOpNone) {}

ASTUnaryOp::~ASTUnaryOp() {
  if (_child)
    delete _child;
}

bool ASTUnaryOp::isConstant() const {
  return getChild()->isConstant();
}

ASTNode** ASTUnaryOp::getChildNodes() const {
  return const_cast<ASTNode**>(&_child);
}

size_t ASTUnaryOp::getChildrenCount() const {
  return 1;
}

double ASTUnaryOp::evaluate(void* data) const {
  double value = getChild()->evaluate(data);

  switch (getUnaryType()) {
    case kMPOpNone:
      return value;
    case kMPOpNegate:
      return -value;
    default:
      MP_ASSERT_NOT_REACHED();
      return 0.0;
  }
}

// ============================================================================
// [mathpresso::ASTBinaryOp]
// ============================================================================

ASTBinaryOp::ASTBinaryOp(unsigned int nodeId, unsigned int binaryType)
  : ASTNode(nodeId, kMPNodeBinaryOp),
    _binaryType(binaryType),
    _left(NULL),
    _right(NULL) {}

ASTBinaryOp::~ASTBinaryOp() {
  if (_left) delete _left;
  if (_right) delete _right;
}

bool ASTBinaryOp::isConstant() const {
  return getLeft()->isConstant() && getRight()->isConstant();
}

ASTNode** ASTBinaryOp::getChildNodes() const {
  return const_cast<ASTNode**>(_nodes);
}

size_t ASTBinaryOp::getChildrenCount() const {
  return 2;
}

double ASTBinaryOp::evaluate(void* data) const {
  double result;

  switch (getBinaryType()) {
    case kMPOpAssign: {
      MP_ASSERT(_left->getNodeType() == kMPNodeVariable);
      result = _right->evaluate(data);
      reinterpret_cast<double*>((char*)data +
        reinterpret_cast<ASTVariable*>(_left)->getOffset())[0] = result;
      break;
    }

    case kMPOpEq : result = static_cast<double>(_left->evaluate(data) == _right->evaluate(data)); break;
    case kMPOpNe : result = static_cast<double>(_left->evaluate(data) != _right->evaluate(data)); break;
    case kMPOpGt : result = static_cast<double>(_left->evaluate(data) >  _right->evaluate(data)); break;
    case kMPOpGe : result = static_cast<double>(_left->evaluate(data) >= _right->evaluate(data)); break;
    case kMPOpLt : result = static_cast<double>(_left->evaluate(data) <  _right->evaluate(data)); break;
    case kMPOpLe : result = static_cast<double>(_left->evaluate(data) <= _right->evaluate(data)); break;

    case kMPOpAdd: result = _left->evaluate(data) + _right->evaluate(data); break;
    case kMPOpSub: result = _left->evaluate(data) - _right->evaluate(data); break;
    case kMPOpMul: result = _left->evaluate(data) * _right->evaluate(data); break;
    case kMPOpDiv: result = _left->evaluate(data) / _right->evaluate(data); break;

    case kMPOpMod: {
      double vl = _left->evaluate(data);
      double vr = _right->evaluate(data);
      result = fmod(vl, vr);
      break;
    }
  }

  return result;
}

// ============================================================================
// [mathpresso::ASTCall]
// ============================================================================

ASTCall::ASTCall(unsigned int nodeId, Function* function)
  : ASTNode(nodeId, kMPNodeCall),
    _function(function) {}

ASTCall::~ASTCall() {
  mpDeleteAll(_arguments);
}

bool ASTCall::isConstant() const {
  size_t i, len = _arguments.getLength();

  for (i = 0; i < len; i++) {
    if (!_arguments[i]->isConstant())
      return false;
  }

  return (getFunction()->getPrototype() & kMPFuncSafe) != 0;
}

ASTNode** ASTCall::getChildNodes() const {
  return const_cast<ASTNode**>(_arguments.getData());
}

size_t ASTCall::getChildrenCount() const {
  return _arguments.getLength();
}

double ASTCall::evaluate(void* data) const {
  double result = 0.0f;
  double t[10];
  size_t i, len = _arguments.getLength();

  for (i = 0; i < len; i++) {
    t[i] = _arguments[i]->evaluate(data);
  }

  void* fn = getFunction()->getPtr();
  MP_ASSERT(getFunction()->getArguments() == len);

  switch (len) {
    case 0: result = ((MFunc_Ret_D_ARG0)fn)(); break;
    case 1: result = ((MFunc_Ret_D_ARG1)fn)(t[0]); break;
    case 2: result = ((MFunc_Ret_D_ARG2)fn)(t[0], t[1]); break;
    case 3: result = ((MFunc_Ret_D_ARG3)fn)(t[0], t[1], t[2]); break;
    case 4: result = ((MFunc_Ret_D_ARG4)fn)(t[0], t[1], t[2], t[3]); break;
    case 5: result = ((MFunc_Ret_D_ARG5)fn)(t[0], t[1], t[2], t[3], t[4]); break;
    case 6: result = ((MFunc_Ret_D_ARG6)fn)(t[0], t[1], t[2], t[3], t[4], t[5]); break;
    case 7: result = ((MFunc_Ret_D_ARG7)fn)(t[0], t[1], t[2], t[3], t[4], t[5], t[6]); break;
    case 8: result = ((MFunc_Ret_D_ARG8)fn)(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]); break;
  }

  return result;
}

} // mathpresso namespace
