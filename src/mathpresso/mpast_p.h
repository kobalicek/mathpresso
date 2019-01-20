// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPAST_P_H
#define _MATHPRESSO_MPAST_P_H

// [Dependencies]
#include "./mphash_p.h"

namespace mathpresso {

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct AstBuilder;
struct AstScope;
struct AstSymbol;

struct AstNode;
struct AstProgram;
struct AstUnary;

// ============================================================================
// [mathpresso::AstScopeType]
// ============================================================================

enum AstScopeType {
  //! Global scope.
  kAstScopeGlobal = 0,

  //! Shadow scope acts like a global scope, however, it's mutable and can be
  //! modified by the optimizer. Shadow scope is never used to store locals.
  kAstScopeShadow = 1,

  //! Local scope.
  kAstScopeLocal = 2,

  //! Nested scope.
  //!
  //! Always statically allocated and merged with the local scope before the
  //! scope is destroyed.
  kAstScopeNested = 3
};

// ============================================================================
// [mathpresso::AstSymbolType]
// ============================================================================

//! \internal
//!
//! Symbol type.
enum AstSymbolType {
  //! Not used.
  kAstSymbolNone = 0,
  //! Symbol is an intrinsic (converted to operator internally).
  kAstSymbolIntrinsic,
  //! Symbol is a variable.
  kAstSymbolVariable,
  //! Symbol is a function.
  kAstSymbolFunction
};

// ============================================================================
// [mathpresso::AstSymbolFlags]
// ============================================================================

enum AstSymbolFlags {
  //! The symbol was declared in global scope.
  kAstSymbolIsGlobal   = 0x0001,

  //! The symbol was declared and can be used.
  //!
  //! If this flag is not set it means that the parser is parsing its assignment
  //! (code like "int x = ...") and the symbol can't be used at this time. It's
  //! for parser to make sure that the symbol is declared before it's used.
  kAstSymbolIsDeclared = 0x0002,

  //! Used during optimizing phase and to create global constants.
  kAstSymbolIsAssigned = 0x0004,

  //! The symbol (variable) is read-only.
  kAstSymbolIsReadOnly = 0x0008,

  //! The variable has been altered (written), at least once.
  //!
  //! Currently only useful for global variables so the JIT compiler can
  //! perform write operation at the end of the generated function.
  kAstSymbolIsAltered = 0x0010
};

// ============================================================================
// [mathpresso::AstNodeType]
// ============================================================================

//! \internal
//!
//! `AstNode` type.
enum AstNodeType {
  //! Not used.
  kAstNodeNone = 0,

  // --------------------------------------------------------------------------
  // [Block]
  // --------------------------------------------------------------------------

  //! Node is `AstProgram`.
  kAstNodeProgram,
  //! Node is `AstBlock`.
  kAstNodeBlock,

  // --------------------------------------------------------------------------
  // [Variable, Immediate]
  // --------------------------------------------------------------------------

  //! Node is `AstVarDecl`.
  kAstNodeVarDecl,
  //! Node is `AstVar`.
  kAstNodeVar,
  //! Node is `AstImm`.
  kAstNodeImm,

  // --------------------------------------------------------------------------
  // [Op]
  // --------------------------------------------------------------------------

  //! Node is `AstUnaryOp`.
  kAstNodeUnaryOp,
  //! Node is `AstBinaryOp`.
  kAstNodeBinaryOp,
  //! Node is `AstCall`.
  kAstNodeCall
};

// ============================================================================
// [mathpresso::AstNodeFlags]
// ============================================================================

//! \internal
//!
//! `AstNode` flags.
enum AstNodeFlags {
  kAstNodeHasSideEffect = 0x01
};

// ============================================================================
// [mathpresso::AstBuilder]
// ============================================================================

//! \internal
struct AstBuilder {
  MATHPRESSO_NONCOPYABLE(AstBuilder)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstBuilder(ZoneAllocator* allocator);
  ~AstBuilder();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE ZoneAllocator* allocator() const { return _allocator; }
  MATHPRESSO_INLINE AstScope* rootScope() const { return _rootScope; }
  MATHPRESSO_INLINE AstProgram* programNode() const { return _programNode; }

  // --------------------------------------------------------------------------
  // [Factory]
  // --------------------------------------------------------------------------

  AstScope* newScope(AstScope* parent, uint32_t scopeType);
  void deleteScope(AstScope* scope);

  AstSymbol* newSymbol(const StringRef& key, uint32_t hashCode, uint32_t symbolType, uint32_t scopeType);
  AstSymbol* shadowSymbol(const AstSymbol* other);
  void deleteSymbol(AstSymbol* symbol);

#define MATHPRESSO_ALLOC_AST_OBJECT(_Size_) \
  void* obj = _allocator->alloc(_Size_); \
  if (MATHPRESSO_UNLIKELY(obj == NULL)) return NULL

  template<typename T>
  MATHPRESSO_INLINE T* newNode() {
    MATHPRESSO_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this);
  }

  template<typename T, typename P0>
  MATHPRESSO_INLINE T* newNode(P0 p0) {
    MATHPRESSO_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this, p0);
  }

  template<typename T, typename P0, typename P1>
  MATHPRESSO_INLINE T* newNode(P0 p0, P1 p1) {
    MATHPRESSO_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this, p0, p1);
  }

#undef MATHPRESSO_ALLOC_AST_OBJECT

  void deleteNode(AstNode* node);

  MATHPRESSO_INLINE uint32_t newSlotId() { return _numSlots++; }

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  Error initProgramScope();

  // --------------------------------------------------------------------------
  // [Dump]
  // --------------------------------------------------------------------------

  Error dump(String& sb);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Zone allocator.
  ZoneAllocator* _allocator;
  //! String builder to build possible output messages.
  String _sb;

  //! Root scope.
  AstScope* _rootScope;
  //! Root node.
  AstProgram* _programNode;

  //! Number of variable slots used.
  uint32_t _numSlots;
};

// ============================================================================
// [mathpresso::AstSymbol]
// ============================================================================

struct AstSymbol : public HashNode {
  MATHPRESSO_NONCOPYABLE(AstSymbol)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstSymbol(const char* name, uint32_t nameSize, uint32_t hashCode, uint32_t symbolType, uint32_t scopeType)
    : HashNode(hashCode),
      _name(name),
      _nameSize(nameSize),
      _symbolType(static_cast<uint8_t>(symbolType)),
      _opType(kOpNone),
      _symbolFlags(scopeType == kAstScopeGlobal ? (int)kAstSymbolIsGlobal : 0),
      _usedCount(0),
      _writeCount(0),
      _node(nullptr),
      _value() {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE bool eq(const StringRef& s) const {
    return eq(s.data(), s.size());
  }

  //! Get whether the symbol name is equal to string `data` of size `size`.
  MATHPRESSO_INLINE bool eq(const char* data, size_t size) const {
    return static_cast<size_t>(_nameSize) == size && ::memcmp(_name, data, size) == 0;
  }

  //! Get symbol name.
  MATHPRESSO_INLINE const char* name() const { return _name; }
  //! Get symbol name size.
  MATHPRESSO_INLINE uint32_t nameSize() const { return _nameSize; }

  //! Check if the symbol has associated node with it.
  MATHPRESSO_INLINE bool hasNode() const { return _node != NULL; }
  //! Get node associated with the symbol (can be `NULL` for built-ins).
  MATHPRESSO_INLINE AstNode* node() const { return _node; }
  //! Associate node with the symbol (basically the node that declares it).
  MATHPRESSO_INLINE void setNode(AstNode* node) { _node = node; }

  //! Get hash value of the symbol name.
  MATHPRESSO_INLINE uint32_t hashCode() const { return _hashCode; }

  //! Get symbol type, see \ref AstSymbolType.
  MATHPRESSO_INLINE uint32_t symbolType() const { return _symbolType; }

  //! Get symbol flags, see \ref AstSymbolFlags.
  MATHPRESSO_INLINE uint32_t symbolFlags() const { return _symbolFlags; }

  MATHPRESSO_INLINE bool hasSymbolFlag(uint32_t flag) const { return (_symbolFlags & flag) != 0; }
  MATHPRESSO_INLINE void setSymbolFlag(uint32_t flag) { _symbolFlags |= static_cast<uint16_t>(flag); }
  MATHPRESSO_INLINE void clearSymbolFlag(uint32_t flag) { _symbolFlags &= ~static_cast<uint16_t>(flag); }

  //! Check if the symbol is global (i.e. it was declared in a global scope).
  MATHPRESSO_INLINE bool isGlobal() const { return hasSymbolFlag(kAstSymbolIsGlobal); }
  //! Check if the symbol was declared.
  MATHPRESSO_INLINE bool isDeclared() const { return hasSymbolFlag(kAstSymbolIsDeclared); }
  //! Set the symbol to be declared (\ref kAstSymbolIsDeclared flag).
  MATHPRESSO_INLINE void setDeclared() { setSymbolFlag(kAstSymbolIsDeclared); }

  //! Get operator type, see \ref OpType.
  //!
  //! Only valid if symbol type is \ref kAstSymbolIntrinsic.
  MATHPRESSO_INLINE uint32_t opType() const { return _opType; }
  //! Set operator type to `opType`.
  MATHPRESSO_INLINE void setOpType(uint32_t opType) {
    MATHPRESSO_ASSERT(_symbolType == kAstSymbolIntrinsic);
    _opType = static_cast<uint8_t>(opType);
  }

  MATHPRESSO_INLINE uint32_t varSlotId() const { return _varSlotId; }
  MATHPRESSO_INLINE void setVarSlotId(uint32_t slotId) { _varSlotId = slotId; }

  MATHPRESSO_INLINE int32_t varOffset() const { return _varOffset; }
  MATHPRESSO_INLINE void setVarOffset(int32_t offset) { _varOffset = offset; }

  MATHPRESSO_INLINE void* funcPtr() const { return _funcPtr; }
  MATHPRESSO_INLINE void setFuncPtr(void* ptr) { _funcPtr = ptr; }

  //! Get the number of function/intrinsic arguments
  MATHPRESSO_INLINE uint32_t funcArgs() const { return _funcArgs; }
  //! Set the number of function/intrinsic arguments
  MATHPRESSO_INLINE void setFuncArgs(uint32_t n) {
    MATHPRESSO_ASSERT(_symbolType == kAstSymbolIntrinsic || _symbolType == kAstSymbolFunction);
    _funcArgs = n;
  }

  //! Get whether the variable has assigned a constant value at the moment.
  //!
  //! If true, the `_value` is a valid constant that can be used to replace
  //! the variable node by a constant value. The value can change during AST
  //! traversal in case that the variable is mutable.
  MATHPRESSO_INLINE bool isAssigned() const { return hasSymbolFlag(kAstSymbolIsAssigned); }
  //! Set symbol to be assigned (sets the \ref kAstSymbolIsAssigned flag).
  MATHPRESSO_INLINE void setAssigned() { setSymbolFlag(kAstSymbolIsAssigned); }
  //! Set symbol to not be assigned (clears the \ref kAstSymbolIsAssigned flag).
  MATHPRESSO_INLINE void clearAssigned() { clearSymbolFlag(kAstSymbolIsAssigned); }

  //! Get whether the global symbol has been altered.
  MATHPRESSO_INLINE bool isAltered() const { return hasSymbolFlag(kAstSymbolIsAltered); }
  //! Make a global symbol altered.
  MATHPRESSO_INLINE void setAltered() { setSymbolFlag(kAstSymbolIsAltered); }

  //! Get the constant value, see `isAssigned()`.
  MATHPRESSO_INLINE double value() const { return _value; }
  //! Set `_isAssigned` to true and `_value` to `value`.
  MATHPRESSO_INLINE void setValue(double value) { _value = value; setAssigned(); }

  MATHPRESSO_INLINE uint32_t usedCount() const { return _usedCount; }
  MATHPRESSO_INLINE uint32_t readCount() const { return _usedCount - _writeCount; }
  MATHPRESSO_INLINE uint32_t writeCount() const { return _writeCount; }

  MATHPRESSO_INLINE void incUsedCount(uint32_t n = 1) { _usedCount += n; }
  MATHPRESSO_INLINE void incWriteCount(uint32_t n = 1) { _writeCount += n; }

  MATHPRESSO_INLINE void decUsedCount(uint32_t n = 1) { _usedCount -= n; }
  MATHPRESSO_INLINE void decWriteCount(uint32_t n = 1) { _writeCount -= n; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Symbol name (key).
  const char* _name;
  //! Symbol name size.
  uint32_t _nameSize;

  //! Type of the symbol, see \ref AstSymbolType.
  uint8_t _symbolType;
  //! Operator type (if the symbol is \ref kAstSymbolIntrinsic).
  uint8_t _opType;
  //! Flags, see \ref AstSymbolFlags.
  uint16_t _symbolFlags;

  //! Number of times the variable is used (both read and write count).
  uint32_t _usedCount;
  //! Number of times the variable is written.
  uint32_t _writeCount;

  //! Node where the symbol is defined.
  AstNode* _node;

  union {
    struct {
      //! Variable slot id.
      uint32_t _varSlotId;
      //! Variable offset in data structure (in case the symbol is a global variable).
      int32_t _varOffset;
      //! The current value of the symbol (in case the symbol is an immediate).
      double _value;
    };

    struct {
      //! Function pointer (in case the symbol is a function).
      void* _funcPtr;
      //! Number of function arguments (in case the symbol is a function).
      uint32_t _funcArgs;
    };
  };
};

typedef Hash<StringRef, AstSymbol> AstSymbolHash;
typedef HashIterator<StringRef, AstSymbol> AstSymbolHashIterator;

// ============================================================================
// [mathpresso::AstScope]
// ============================================================================

struct AstScope {
  MATHPRESSO_NONCOPYABLE(AstScope)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_NOAPI AstScope(AstBuilder* ast, AstScope* parent, uint32_t scopeType);
  MATHPRESSO_NOAPI ~AstScope();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the scope context.
  MATHPRESSO_INLINE AstBuilder* ast() const { return _ast; }
  //! Get the parent scope (or NULL).
  MATHPRESSO_INLINE AstScope* parent() const { return _parent; }
  //! Get symbols hash-table.
  MATHPRESSO_INLINE const AstSymbolHash& symbols() const { return _symbols; }
  //! Get scope type, see \ref AstScopeType.
  MATHPRESSO_INLINE uint32_t scopeType() const { return _scopeType; }

  //! Get whether the scope type is `kAstScopeGlobal`.
  MATHPRESSO_INLINE bool isGlobal() const { return _scopeType == kAstScopeGlobal; }

  //! Make this scope a shadow of `ctxScope`.
  MATHPRESSO_INLINE void shadowContextScope(AstScope* ctxScope) {
    _parent = ctxScope;
    _scopeType = kAstScopeShadow;
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Get the symbol defined only in this scope.
  MATHPRESSO_INLINE AstSymbol* getSymbol(const StringRef& name, uint32_t hashCode) {
    return _symbols.get(name, hashCode);
  }

  //! Put a given symbol to this scope.
  //!
  //! \note The function doesn't care about duplicates. The correct flow is
  //! to call `resolveSymbol()` or `getSymbol()` and then `putSymbol()` based
  //! on the result. You should never call `putSymbol()` without checking if
  //! the symbol is already there.
  MATHPRESSO_INLINE void putSymbol(AstSymbol* symbol) {
    _symbols.put(symbol);
  }

  //! Resolve the symbol by traversing all parent scopes if not found in this
  //! one. An optional `scopeOut` argument can be used to get scope where the
  //! `name` has been found.
  MATHPRESSO_NOAPI AstSymbol* resolveSymbol(const StringRef& name, uint32_t hashCode, AstScope** scopeOut = NULL);

  MATHPRESSO_INLINE AstSymbol* resolveSymbol(const StringRef& name) {
    return resolveSymbol(name, HashUtils::hashString(name.data(), name.size()));
  }

  MATHPRESSO_NOAPI AstSymbol* removeSymbol(AstSymbol* symbol) {
    return _symbols.del(symbol);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Context.
  AstBuilder* _ast;
  //! Parent scope.
  AstScope* _parent;

  //! Symbols defined in this scope.
  AstSymbolHash _symbols;

  //! Scope type, see \ref AstScopeType.
  uint32_t _scopeType;
};

// ============================================================================
// [mathpresso::AstNode]
// ============================================================================

#define MATHPRESSO_AST_CHILD(INDEX, NODE_T, NAME, NAME_UP) \
  MATHPRESSO_INLINE NODE_T* NAME() const { return _##NAME; } \
  MATHPRESSO_INLINE NODE_T* set##NAME_UP(NODE_T* node) { return static_cast<NODE_T*>(replaceAt(INDEX, node)); } \
  \
  MATHPRESSO_INLINE NODE_T* unlink##NAME_UP() { \
    NODE_T* node = _##NAME; \
    \
    MATHPRESSO_ASSERT(node != NULL); \
    MATHPRESSO_ASSERT(node->_parent == this); \
    \
    node->_parent = NULL; \
    _##NAME = NULL; \
    \
    return node; \
  } \
  \
  NODE_T* _##NAME

struct AstNode {
  MATHPRESSO_NONCOPYABLE(AstNode)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstNode(AstBuilder* ast, uint32_t nodeType, AstNode** children = NULL, uint32_t size = 0)
    : _ast(ast),
      _parent(NULL),
      _children(children),
      _nodeType(static_cast<uint8_t>(nodeType)),
      _nodeFlags(0),
      _nodeSize(0),
      _opType(kOpNone),
      _position(~static_cast<uint32_t>(0)),
      _size(size) {}

  MATHPRESSO_INLINE void destroy(AstBuilder*) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the `AstBuilder` instance that created this node.
  MATHPRESSO_INLINE AstBuilder* ast() const { return _ast; }

  //! Check if the node has a parent.
  MATHPRESSO_INLINE bool hasParent() const { return _parent != NULL; }
  //! Get the parent node.
  MATHPRESSO_INLINE AstNode* parent() const { return _parent; }

  //! Get whether the node is empty (has no children).
  MATHPRESSO_INLINE bool empty() const { return !_size; }
  //! Get children array.
  MATHPRESSO_INLINE AstNode** children() const { return reinterpret_cast<AstNode**>(_children); }
  //! Get size of children array.
  MATHPRESSO_INLINE uint32_t size() const { return _size; }

  //! Get node type.
  MATHPRESSO_INLINE uint32_t nodeType() const { return _nodeType; }
  //! Get whether the node is `AstVar`.
  MATHPRESSO_INLINE bool isVar() const { return _nodeType == kAstNodeVar; }
  //! Get whether the node is `AstImm`.
  MATHPRESSO_INLINE bool isImm() const { return _nodeType == kAstNodeImm; }

  //! Get whether the node has flag `flag`.
  MATHPRESSO_INLINE bool hasNodeFlag(uint32_t flag) const { return (static_cast<uint32_t>(_nodeFlags) & flag) != 0; }
  //! Get node flags.
  MATHPRESSO_INLINE uint32_t nodeFlags() const { return _nodeFlags; }
  //! Set node flags.
  MATHPRESSO_INLINE void setNodeFlags(uint32_t flags) { _nodeFlags = static_cast<uint8_t>(flags); }
  //! Add node flags.
  MATHPRESSO_INLINE void addNodeFlags(uint32_t flags) { _nodeFlags |= static_cast<uint8_t>(flags); }

  //! Get node size (in bytes).
  MATHPRESSO_INLINE uint32_t nodeSize() const { return _nodeSize; }

  //! Get op.
  MATHPRESSO_INLINE uint32_t opType() const { return _opType; }
  //! Set op.
  MATHPRESSO_INLINE void setOpType(uint32_t opType) { _opType = static_cast<uint8_t>(opType); }

  //! Get whether the node has associated position in source code.
  MATHPRESSO_INLINE bool hasPosition() const { return _position != ~static_cast<uint32_t>(0); }
  //! Get source code position of the node.
  MATHPRESSO_INLINE uint32_t position() const { return _position; }
  //! Set source code position of the node.
  MATHPRESSO_INLINE void setPosition(uint32_t position) { _position = position; }
  //! Reset source code position of the node.
  MATHPRESSO_INLINE void resetPosition() { _position = ~static_cast<uint32_t>(0); }

  // --------------------------------------------------------------------------
  // [Children]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstNode* childAt(uint32_t index) const {
    MATHPRESSO_ASSERT(index < _size);
    return _children[index];
  }

  //! Replace `refNode` by `node`.
  AstNode* replaceNode(AstNode* refNode, AstNode* node);
  //! Replace node at index `index` by `node`.
  AstNode* replaceAt(uint32_t index, AstNode* node);

  //! Inject `node` between this node and `refNode`.
  AstNode* injectNode(AstNode* refNode, AstUnary* node);
  //! Inject `node` between this node and node at index `index`.
  AstNode* injectAt(uint32_t index, AstUnary* node);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! AST builder.
  AstBuilder* _ast;
  //! Parent node.
  AstNode* _parent;
  //! Child nodes.
  AstNode** _children;

  //! Node type, see `AstNodeType`.
  uint8_t _nodeType;
  //! Node flags, see `AstNodeFlags`.
  uint8_t _nodeFlags;
  //! Node size in bytes for `ZoneAllocator`.
  uint8_t _nodeSize;
  //! Operator type, see `OpType`.
  uint8_t _opType;

  //! Position (in characters) to the beginning of the program (default -1).
  uint32_t _position;

  //! Count of child-nodes.
  uint32_t _size;
};

// ============================================================================
// [mathpresso::AstBlock]
// ============================================================================

struct AstBlock : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstBlock)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstBlock(AstBuilder* ast, uint32_t nodeType = kAstNodeBlock)
    : AstNode(ast, nodeType),
      _capacity(0) {}

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Reserve the capacity of the AstBlock so one more node can be added into it.
  //!
  //! \note This has to be called before you use `appendNode()` or `insertAt()`.
  //! The reason is that it's easier to deal with possible allocation failure
  //! here (before the node to be added is created) than after the node is
  //! created, but failed to add into the block.
  Error willAdd();

  //! Append the given `node` to the block.
  //!
  //! \note You have to call `willAdd()` before you use `appendNode()` for every
  //! node you want to add to the block.
  MATHPRESSO_INLINE void appendNode(AstNode* node) {
    MATHPRESSO_ASSERT(node != NULL);
    MATHPRESSO_ASSERT(node->parent() == NULL);

    // We expect `willAdd()` to be called before `appendNode()`.
    MATHPRESSO_ASSERT(_size < _capacity);

    node->_parent = this;

    _children[_size] = node;
    _size++;
  }

  //! Insert the given `node` to the block at index `i`.
  //!
  //! \note You have to call `willAdd()` before you use `insertAt()` for every
  //! node you want to add to the block.
  MATHPRESSO_INLINE void insertAt(uint32_t i, AstNode* node) {
    MATHPRESSO_ASSERT(node != NULL);
    MATHPRESSO_ASSERT(node->parent() == NULL);

    // We expect `willAdd()` to be called before `insertAt()`.
    MATHPRESSO_ASSERT(_size < _capacity);

    AstNode** p = children();
    node->_parent = this;

    uint32_t j = _size;
    while (i < j) {
      p[j] = p[j - 1];
      j--;
    }

    p[j] = node;
    _size++;
  }

  //! Remove the given `node`.
  AstNode* removeNode(AstNode* node);
  //! Remove the node at index `index`.
  AstNode* removeAt(uint32_t index);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _capacity;
};

// ============================================================================
// [mathpresso::AstUnary]
// ============================================================================

struct AstUnary : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstUnary)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstUnary(AstBuilder* ast, uint32_t nodeType)
    : AstNode(ast, nodeType, &_child, 1),
      _child(NULL) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstNode** children() const { return (AstNode**)&_child; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MATHPRESSO_AST_CHILD(0, AstNode, child, Child);
};

// ============================================================================
// [mathpresso::AstBinary]
// ============================================================================

struct AstBinary : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstBinary)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstBinary(AstBuilder* ast, uint32_t nodeType)
    : AstNode(ast, nodeType, &_left, 2),
      _left(NULL),
      _right(NULL) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstNode** children() const { return (AstNode**)&_left; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MATHPRESSO_AST_CHILD(0, AstNode, left, Left);
  MATHPRESSO_AST_CHILD(1, AstNode, right, Right);
};

// ============================================================================
// [mathpresso::AstProgram]
// ============================================================================

struct AstProgram : public AstBlock {
  MATHPRESSO_NONCOPYABLE(AstProgram)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstProgram(AstBuilder* ast)
    : AstBlock(ast, kAstNodeProgram) {}
};

// ============================================================================
// [mathpresso::AstVarDecl]
// ============================================================================

struct AstVarDecl : public AstUnary {
  MATHPRESSO_NONCOPYABLE(AstVarDecl)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstVarDecl(AstBuilder* ast)
    : AstUnary(ast, kAstNodeVarDecl),
      _symbol(NULL) {}

  MATHPRESSO_INLINE void destroy(AstBuilder*) {
    if (symbol())
      symbol()->decUsedCount();
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstSymbol* symbol() const { return _symbol; }
  MATHPRESSO_INLINE void setSymbol(AstSymbol* symbol) { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mathpresso::AstVar]
// ============================================================================

struct AstVar : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstVar)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstVar(AstBuilder* ast)
    : AstNode(ast, kAstNodeVar),
      _symbol(NULL) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstSymbol* symbol() const { return _symbol; }
  MATHPRESSO_INLINE void setSymbol(AstSymbol* symbol) { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mathpresso::AstImm]
// ============================================================================

struct AstImm : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstImm)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstImm(AstBuilder* ast, double value = 0.0)
    : AstNode(ast, kAstNodeImm),
      _value(value) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE double value() const { return _value; }
  MATHPRESSO_INLINE void setValue(double value) { _value = value; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  double _value;
};

// ============================================================================
// [mathpresso::AstUnaryOp]
// ============================================================================

struct AstUnaryOp : public AstUnary {
  MATHPRESSO_NONCOPYABLE(AstUnaryOp)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstUnaryOp(AstBuilder* ast, uint32_t opType)
    : AstUnary(ast, kAstNodeUnaryOp) { setOpType(opType); }
};

// ============================================================================
// [mathpresso::AstBinaryOp]
// ============================================================================

struct AstBinaryOp : public AstBinary {
  MATHPRESSO_NONCOPYABLE(AstBinaryOp)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstBinaryOp(AstBuilder* ast, uint32_t opType)
    : AstBinary(ast, kAstNodeBinaryOp) { setOpType(opType); }

  MATHPRESSO_INLINE void destroy(AstBuilder*) {
    if (mpOpInfo[opType()].isAssignment() && left()) {
      AstVar* var = static_cast<AstVar*>(left());
      if (var->symbol())
        var->symbol()->decWriteCount();
    }
  }
};

// ============================================================================
// [mathpresso::AstCall]
// ============================================================================

struct AstCall : public AstBlock {
  MATHPRESSO_NONCOPYABLE(AstCall)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstCall(AstBuilder* ast)
    : AstBlock(ast, kAstNodeCall),
      _symbol(NULL) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstSymbol* symbol() const { return _symbol; }
  MATHPRESSO_INLINE void setSymbol(AstSymbol* symbol) { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mathpresso::AstVisitor]
// ============================================================================

struct AstVisitor {
  MATHPRESSO_NONCOPYABLE(AstVisitor)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstVisitor(AstBuilder* ast);
  virtual ~AstVisitor();

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstBuilder* ast() const { return _ast; }

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  virtual Error onNode(AstNode* node);

  virtual Error onProgram(AstProgram* node);
  virtual Error onBlock(AstBlock* node) = 0;
  virtual Error onVarDecl(AstVarDecl* node) = 0;
  virtual Error onVar(AstVar* node) = 0;
  virtual Error onImm(AstImm* node) = 0;
  virtual Error onUnaryOp(AstUnaryOp* node) = 0;
  virtual Error onBinaryOp(AstBinaryOp* node) = 0;
  virtual Error onCall(AstCall* node) = 0;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstBuilder* _ast;
};

// ============================================================================
// [mathpresso::AstDump]
// ============================================================================

struct AstDump : public AstVisitor {
  MATHPRESSO_NONCOPYABLE(AstDump)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstDump(AstBuilder* ast, String& sb);
  virtual ~AstDump();

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  virtual Error onBlock(AstBlock* node);
  virtual Error onVarDecl(AstVarDecl* node);
  virtual Error onVar(AstVar* node);
  virtual Error onImm(AstImm* node);
  virtual Error onUnaryOp(AstUnaryOp* node);
  virtual Error onBinaryOp(AstBinaryOp* node);
  virtual Error onCall(AstCall* node);

  // --------------------------------------------------------------------------
  // [Helpers]
  // --------------------------------------------------------------------------

  Error info(const char* fmt, ...);
  Error nest(const char* fmt, ...);
  Error denest();

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  String& _sb;
  uint32_t _level;
};

} // mathpresso namespace

// [Guard]
#endif // _MATHPRESSO_MPAST_P_H
