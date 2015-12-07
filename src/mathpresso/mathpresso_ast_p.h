// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _MATHPRESSO_AST_P_H
#define _MATHPRESSO_AST_P_H

#include "mathpresso.h"
#include "mathpresso_context_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::Constants]
// ============================================================================

//! \internal
//!
//! Node type.
enum MPNodeType {
  kMPNodeBlock,
  kMPNodeImmediate,
  kMPNodeVariable,
  kMPNodeUnaryOp,
  kMPNodeBinaryOp,
  kMPNodeCall
};

//! \internal
//!
//! Variable type.
enum MPVariableType {
  kMPVariableTypeConst = 0,
  kMPVariableTypeRO = 1,
  kMPVariableTypeRW = 2
};

//! \internal
//!
//! Unary operator type.
enum MPUnaryOpType {
  kMPUnaryOpNone,
  kMPUnaryOpNegate
};

//! \internal
//!
//! Binary operator type.
enum MPBinaryOpType {
  kMPBinaryOpNone,
  kMPBinaryOpAssign,
  kMPBinaryOpAdd,
  kMPBinaryOpSub,
  kMPBinaryOpMul,
  kMPBinaryOpDiv,
  kMPBinaryOpMod
};

//! \internal
//!
//! Function type.
enum MPFunctionType {
  kMPFunctionCustom = 0,

  kMPFunctionRound,
  kMPFunctionFloor,
  kMPFunctionCeil,

  kMPFunctionAbs,
  kMPFunctionSqrt,
  kMPFunctionRecip,

  kMPFunctionLog,
  kMPFunctionLog10,
  kMPFunctionSin,
  kMPFunctionCos,
  kMPFunctionTan,
  kMPFunctionSinh,
  kMPFunctionCosh,
  kMPFunctionTanh,
  kMPFunctionAsin,
  kMPFunctionAcos,
  kMPFunctionAtan,
  kMPFunctionAtan2,

  kMPFunctionAvg,
  kMPFunctionMin,
  kMPFunctionMax,
  kMPFunctionPow
};

// ============================================================================
// [mathpresso::ASTNode]
// ============================================================================

struct MATHPRESSO_NOAPI ASTNode {
  ASTNode(unsigned int nodeId, unsigned int nodeType);
  virtual ~ASTNode() = 0;

  //! Get whether this node is a constant expression.
  //!
  //! Used by optimizer to evaluate all constant expressions first.
  virtual bool isConstant() const = 0;

  //! Get array of child nodes.
  virtual ASTNode** getChildNodes() const = 0;

  //! Get count of child nodes.
  virtual size_t getChildrenCount() const = 0;

  //! Replace child node `child` by `node`.
  virtual bool replaceChild(ASTNode* child, ASTNode* node);

  //! Evaluate this node.
  virtual double evaluate(void* data) const = 0;

  //! Get the parent node.
  inline ASTNode* getParent() const { return _parent; }

  //! Get the node type.
  inline unsigned int getNodeType() const { return _nodeType; }
  //! Get if the node is `kMPNodeBlock`.
  inline bool isBlock() const { return _nodeType == kMPNodeBlock; }
  //! Get if the node is `kMPNodeImmediate`.
  inline bool isImmediate() const { return _nodeType == kMPNodeImmediate; }
  //! Get if the node is `kMPNodeVariable`.
  inline bool isVariable() const { return _nodeType == kMPNodeVariable; }
  //! Get if the node is `kMPNodeUnaryOp`.
  inline bool isUnaryOp() const { return _nodeType == kMPNodeUnaryOp; }
  //! Get if the node is `kMPNodeBinaryOp`.
  inline bool isBinaryOp() const { return _nodeType == kMPNodeBinaryOp; }
  //! Get if the node is `kMPNodeCall`.
  inline bool isCall() const { return _nodeType == kMPNodeCall; }

  //! Get the node ID.
  inline unsigned int getNodeId() const { return _nodeId; }

  //! Parent node.
  ASTNode* _parent;
  //! Node type.
  unsigned int _nodeType;
  //! Node id, unique per `Expression`.
  unsigned int _nodeId;
};

// ============================================================================
// [mathpresso::ASTBlock]
// ============================================================================

struct MATHPRESSO_NOAPI ASTBlock : public ASTNode {
  ASTBlock(unsigned int nodeId);
  virtual ~ASTBlock();

  virtual bool isConstant() const;
  virtual ASTNode** getChildNodes() const;
  virtual size_t getChildrenCount() const;
  virtual double evaluate(void* data) const;

  Vector<ASTNode*> _nodes;
};

// ============================================================================
// [mathpresso::ASTImmediate]
// ============================================================================

struct MATHPRESSO_NOAPI ASTImmediate : public ASTNode {
  ASTImmediate(unsigned int nodeId, double val);
  virtual ~ASTImmediate();

  virtual bool isConstant() const;
  virtual ASTNode** getChildNodes() const;
  virtual size_t getChildrenCount() const;
  virtual bool replaceChild(ASTNode* child, ASTNode* node);
  virtual double evaluate(void* data) const;

  inline double getValue() const { return _value; }
  inline void setValue(double value) { _value = value; }

  double _value;
};

// ============================================================================
// [mathpresso::ASTVariable]
// ============================================================================

struct MATHPRESSO_NOAPI ASTVariable : public ASTNode {
  ASTVariable(unsigned int nodeId, const Variable* variable);
  virtual ~ASTVariable();

  virtual bool isConstant() const;
  virtual ASTNode** getChildNodes() const;
  virtual size_t getChildrenCount() const;
  virtual bool replaceChild(ASTNode* child, ASTNode* node);
  virtual double evaluate(void* data) const;

  inline const Variable* getVariable() const { return _variable; }
  inline int getOffset() const { return _variable->v.offset; }

  const Variable* _variable;
};

// ============================================================================
// [mathpresso::ASTUnaryOp]
// ============================================================================

struct MATHPRESSO_NOAPI ASTUnaryOp : public ASTNode {
  ASTUnaryOp(unsigned int nodeId);
  virtual ~ASTUnaryOp();

  virtual bool isConstant() const;
  virtual ASTNode** getChildNodes() const;
  virtual size_t getChildrenCount() const;
  virtual double evaluate(void* data) const;

  inline ASTNode* getChild() const { return _child; }
  inline void setChild(ASTNode* child) { _child = child; child->_parent = this; }

  inline void resetChild() {
    MP_ASSERT(_child != NULL);
    _child->_parent = NULL;
    _child = NULL;
  }

  inline unsigned int getUnaryType() const { return _unaryType; }
  inline void setUnaryType(unsigned int transformType) { _unaryType = transformType; }

  unsigned int _unaryType;
  ASTNode* _child;
};

// ============================================================================
// [mathpresso::ASTBinaryOp]
// ============================================================================

struct MATHPRESSO_NOAPI ASTBinaryOp : public ASTNode {
  ASTBinaryOp(unsigned int nodeId, unsigned int binaryType);
  virtual ~ASTBinaryOp();

  virtual bool isConstant() const;
  virtual ASTNode** getChildNodes() const;
  virtual size_t getChildrenCount() const;
  virtual double evaluate(void* data) const;

  inline unsigned int getBinaryType() const { return _binaryType; }
  inline void setBinaryType(int value) { _binaryType = value; }

  inline ASTNode* getLeft() const { return _left; }
  inline ASTNode* getRight() const { return _right; }

  inline void setLeft(ASTNode* node) { _left = node; node->_parent = this; }
  inline void setRight(ASTNode* node) { _right = node; node->_parent = this; }

  unsigned int _binaryType;

  union {
    struct {
      ASTNode* _left;
      ASTNode* _right;
    };
    struct {
      ASTNode* _nodes[2];
    };
  };
};

// ============================================================================
// [mathpresso::ASTCall]
// ============================================================================

struct MATHPRESSO_NOAPI ASTCall : public ASTNode {
  ASTCall(unsigned int nodeId, Function* function);
  virtual ~ASTCall();

  virtual bool isConstant() const;
  virtual ASTNode** getChildNodes() const;
  virtual size_t getChildrenCount() const;
  virtual double evaluate(void* data) const;

  inline Function* getFunction() const { return _function; }

  inline Vector<ASTNode*>& getArguments() { return _arguments; }
  inline const Vector<ASTNode*>& getArguments() const { return _arguments; }

  inline bool swapArguments(Vector<ASTNode*>& other) { return _arguments.swap(other); }

protected:
  Function* _function;
  Vector<ASTNode*> _arguments;
};

} // mathpresso namespace

#endif // _MATHPRESSO_AST_P_H
