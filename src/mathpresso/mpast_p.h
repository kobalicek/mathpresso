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

struct AstBuilder;
struct AstScope;
struct AstSymbol;

struct AstNode;
struct AstProgram;
struct AstUnary;

// MathPresso -AstScopeType
// ========================

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

// MathPresso - AstSymbolType
// ==========================

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

// MathPresso - AstSymbolFlags
// ===========================

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

// MathPresso - AstNodeType
// ========================

//! \internal
//!
//! `AstNode` type.
enum AstNodeType {
  //! Not used.
  kAstNodeNone = 0,

  // Block
  // -----

  //! Node is `AstProgram`.
  kAstNodeProgram,
  //! Node is `AstBlock`.
  kAstNodeBlock,

  // Variable & Immediate
  // --------------------

  //! Node is `AstVarDecl`.
  kAstNodeVarDecl,
  //! Node is `AstVar`.
  kAstNodeVar,
  //! Node is `AstImm`.
  kAstNodeImm,

  // Operator & Call
  // ---------------

  //! Node is `AstUnaryOp`.
  kAstNodeUnaryOp,
  //! Node is `AstBinaryOp`.
  kAstNodeBinaryOp,
  //! Node is `AstCall`.
  kAstNodeCall
};

// MathPresso - AstNodeFlags
// =========================

//! \internal
//!
//! `AstNode` flags.
enum AstNodeFlags {
  kAstNodeHasSideEffect = 0x01
};

// MathPresso - AstBuilder
// =======================

//! \internal
struct AstBuilder {
  MATHPRESSO_NONCOPYABLE(AstBuilder)

  // Members
  // -------

  //! Arena allocator.
  Arena& _arena;
  //! String builder to build possible output messages.
  String _sb;

  //! Root scope.
  AstScope* _root_scope {};
  //! Root node.
  AstProgram* _program_node {};

  //! Number of variable slots used.
  uint32_t _num_slots {};

  // Construction & Destruction
  // --------------------------

  AstBuilder(Arena& arena);
  ~AstBuilder();

  // Accessors
  // ---------

  MATHPRESSO_INLINE Arena& arena() const { return _arena; }
  MATHPRESSO_INLINE AstScope* root_scope() const { return _root_scope; }
  MATHPRESSO_INLINE AstProgram* program_node() const { return _program_node; }

  // Factory
  // -------

  AstScope* new_scope(AstScope* parent, uint32_t scope_type);
  void delete_scope(AstScope* scope);

  AstSymbol* new_symbol(const StringRef& key, uint32_t hash_code, uint32_t symbol_type, uint32_t scope_type);
  AstSymbol* shadow_symbol(const AstSymbol* other);
  void delete_symbol(AstSymbol* symbol);

#define MATHPRESSO_ALLOC_AST_OBJECT(_Size_) \
  void* obj = _arena.alloc_reusable(_Size_); \
  if (MATHPRESSO_UNLIKELY(obj == nullptr)) return nullptr

  template<typename T>
  MATHPRESSO_INLINE T* new_node() {
    MATHPRESSO_ALLOC_AST_OBJECT(Arena::aligned_size_of<T>());
    return new(obj) T(this);
  }

  template<typename T, typename... Args>
  MATHPRESSO_INLINE T* new_node(Args&&... args) {
    MATHPRESSO_ALLOC_AST_OBJECT(Arena::aligned_size_of<T>());
    return new(obj) T(this, std::forward<Args>(args)...);
  }

#undef MATHPRESSO_ALLOC_AST_OBJECT

  void delete_node(AstNode* node);

  MATHPRESSO_INLINE uint32_t new_slot_id() { return _num_slots++; }

  // Init
  // ----

  Error init_program_scope();

  // Dump
  // ----

  Error dump(String& sb);
};

// MathPresso - AstSymbol
// ======================

struct AstSymbol : public HashNode {
  MATHPRESSO_NONCOPYABLE(AstSymbol)

  // Members
  // -------

  //! Symbol name (key).
  const char* _name;
  //! Symbol name size.
  uint32_t _name_size;

  //! Type of the symbol, see \ref AstSymbolType.
  uint8_t _symbol_type;
  //! Operator type (if the symbol is \ref kAstSymbolIntrinsic).
  uint8_t _op_type;
  //! Flags, see \ref AstSymbolFlags.
  uint16_t _symbol_flags;

  //! Number of times the variable is used (both read and write count).
  uint32_t _used_count;
  //! Number of times the variable is written.
  uint32_t _write_count;

  //! Node where the symbol is defined.
  AstNode* _node;

  union {
    struct {
      //! Variable slot id.
      uint32_t _var_slot_id;
      //! Variable offset in data structure (in case the symbol is a global variable).
      int32_t _var_offset;
      //! The current value of the symbol (in case the symbol is an immediate).
      double _value;
    };

    struct {
      //! Function pointer (in case the symbol is a function).
      void* _func_ptr;
      //! Number of function arguments (in case the symbol is a function).
      uint32_t _func_args;
    };
  };

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstSymbol(const char* name, uint32_t name_size, uint32_t hash_code, uint32_t symbol_type, uint32_t scope_type)
    : HashNode(hash_code),
      _name(name),
      _name_size(name_size),
      _symbol_type(static_cast<uint8_t>(symbol_type)),
      _op_type(kOpNone),
      _symbol_flags(scope_type == kAstScopeGlobal ? (int)kAstSymbolIsGlobal : 0),
      _used_count(0),
      _write_count(0),
      _node(nullptr),
      _value() {}

  // Accessors
  // ---------

  MATHPRESSO_INLINE bool eq(const StringRef& s) const {
    return eq(s.data(), s.size());
  }

  //! Get whether the symbol name is equal to string `data` of size `size`.
  MATHPRESSO_INLINE bool eq(const char* data, size_t size) const {
    return static_cast<size_t>(_name_size) == size && ::memcmp(_name, data, size) == 0;
  }

  //! Get symbol name.
  MATHPRESSO_INLINE const char* name() const { return _name; }
  //! Get symbol name size.
  MATHPRESSO_INLINE uint32_t name_size() const { return _name_size; }

  //! Check if the symbol has associated node with it.
  MATHPRESSO_INLINE bool has_node() const { return _node != nullptr; }
  //! Get node associated with the symbol (can be `nullptr` for built-ins).
  MATHPRESSO_INLINE AstNode* node() const { return _node; }
  //! Associate node with the symbol (basically the node that declares it).
  MATHPRESSO_INLINE void set_node(AstNode* node) { _node = node; }

  //! Get hash value of the symbol name.
  MATHPRESSO_INLINE uint32_t hash_code() const { return _hash_code; }

  //! Get symbol type, see \ref AstSymbolType.
  MATHPRESSO_INLINE uint32_t symbol_type() const { return _symbol_type; }

  //! Get symbol flags, see \ref AstSymbolFlags.
  MATHPRESSO_INLINE uint32_t symbol_flags() const { return _symbol_flags; }

  MATHPRESSO_INLINE bool has_symbol_flag(uint32_t flag) const { return (_symbol_flags & flag) != 0; }
  MATHPRESSO_INLINE void add_symbol_flags(uint32_t flag) { _symbol_flags |= static_cast<uint16_t>(flag); }
  MATHPRESSO_INLINE void clear_symbol_flags(uint32_t flag) { _symbol_flags &= ~static_cast<uint16_t>(flag); }

  //! Check if the symbol is global (i.e. it was declared in a global scope).
  MATHPRESSO_INLINE bool is_global() const { return has_symbol_flag(kAstSymbolIsGlobal); }
  //! Check if the symbol was declared.
  MATHPRESSO_INLINE bool is_declared() const { return has_symbol_flag(kAstSymbolIsDeclared); }
  //! Set the symbol to be declared (\ref kAstSymbolIsDeclared flag).
  MATHPRESSO_INLINE void mark_declared() { add_symbol_flags(kAstSymbolIsDeclared); }

  //! Get operator type, see \ref OpType.
  //!
  //! Only valid if symbol type is \ref kAstSymbolIntrinsic.
  MATHPRESSO_INLINE uint32_t op_type() const { return _op_type; }
  //! Set operator type to `op_type`.
  MATHPRESSO_INLINE void set_op_type(uint32_t op_type) {
    MATHPRESSO_ASSERT(_symbol_type == kAstSymbolIntrinsic);
    _op_type = static_cast<uint8_t>(op_type);
  }

  MATHPRESSO_INLINE uint32_t var_slot_id() const { return _var_slot_id; }
  MATHPRESSO_INLINE void set_var_slot_id(uint32_t slot_id) { _var_slot_id = slot_id; }

  MATHPRESSO_INLINE int32_t var_offset() const { return _var_offset; }
  MATHPRESSO_INLINE void set_var_offset(int32_t offset) { _var_offset = offset; }

  MATHPRESSO_INLINE void* func_ptr() const { return _func_ptr; }
  MATHPRESSO_INLINE void set_func_ptr(void* ptr) { _func_ptr = ptr; }

  //! Get the number of function/intrinsic arguments
  MATHPRESSO_INLINE uint32_t func_args() const { return _func_args; }
  //! Set the number of function/intrinsic arguments
  MATHPRESSO_INLINE void set_func_args(uint32_t n) {
    MATHPRESSO_ASSERT(_symbol_type == kAstSymbolIntrinsic || _symbol_type == kAstSymbolFunction);
    _func_args = n;
  }

  //! Get whether the variable has assigned a constant value at the moment.
  //!
  //! If true, the `_value` is a valid constant that can be used to replace
  //! the variable node by a constant value. The value can change during AST
  //! traversal in case that the variable is mutable.
  MATHPRESSO_INLINE bool is_assigned() const { return has_symbol_flag(kAstSymbolIsAssigned); }
  //! Set symbol to be assigned (sets the \ref kAstSymbolIsAssigned flag).
  MATHPRESSO_INLINE void mark_assigned() { add_symbol_flags(kAstSymbolIsAssigned); }
  //! Set symbol to not be assigned (clears the \ref kAstSymbolIsAssigned flag).
  MATHPRESSO_INLINE void clear_assigned() { clear_symbol_flags(kAstSymbolIsAssigned); }

  //! Get whether the global symbol has been altered.
  MATHPRESSO_INLINE bool is_altered() const { return has_symbol_flag(kAstSymbolIsAltered); }
  //! Make a global symbol altered.
  MATHPRESSO_INLINE void mark_altered() { add_symbol_flags(kAstSymbolIsAltered); }

  //! Get the constant value, see `is_assigned()`.
  MATHPRESSO_INLINE double value() const { return _value; }
  //! Set `_isAssigned` to true and `_value` to `value`.
  MATHPRESSO_INLINE void set_value(double value) { _value = value; mark_assigned(); }

  MATHPRESSO_INLINE uint32_t used_count() const { return _used_count; }
  MATHPRESSO_INLINE uint32_t read_count() const { return _used_count - _write_count; }
  MATHPRESSO_INLINE uint32_t write_count() const { return _write_count; }

  MATHPRESSO_INLINE void increment_used_count(uint32_t n = 1) { _used_count += n; }
  MATHPRESSO_INLINE void increment_write_count(uint32_t n = 1) { _write_count += n; }

  MATHPRESSO_INLINE void decrement_used_count(uint32_t n = 1) { _used_count -= n; }
  MATHPRESSO_INLINE void decrement_write_count(uint32_t n = 1) { _write_count -= n; }
};

typedef Hash<StringRef, AstSymbol> AstSymbolHash;
typedef HashIterator<StringRef, AstSymbol> AstSymbolHashIterator;

// MathPresso - AstScope
// =====================

struct AstScope {
  MATHPRESSO_NONCOPYABLE(AstScope)

  // Members
  // -------

  //! Context.
  AstBuilder* _ast;
  //! Parent scope.
  AstScope* _parent;

  //! Symbols defined in this scope.
  AstSymbolHash _symbols;

  //! Scope type, see \ref AstScopeType.
  uint32_t _scope_type;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_NOAPI AstScope(AstBuilder* ast, AstScope* parent, uint32_t scope_type);
  MATHPRESSO_NOAPI ~AstScope();

  // Accessors
  // ---------

  //! Get the scope context.
  MATHPRESSO_INLINE AstBuilder* ast() const { return _ast; }
  //! Get the parent scope (or nullptr).
  MATHPRESSO_INLINE AstScope* parent() const { return _parent; }
  //! Get symbols hash-table.
  MATHPRESSO_INLINE const AstSymbolHash& symbols() const { return _symbols; }
  //! Get scope type, see \ref AstScopeType.
  MATHPRESSO_INLINE uint32_t scope_type() const { return _scope_type; }

  //! Get whether the scope type is `kAstScopeGlobal`.
  MATHPRESSO_INLINE bool is_global() const { return _scope_type == kAstScopeGlobal; }

  //! Make this scope a shadow of `ctx_scope`.
  MATHPRESSO_INLINE void shadow_context_scope(AstScope* ctx_scope) {
    _parent = ctx_scope;
    _scope_type = kAstScopeShadow;
  }

  // Operations
  // ----------

  //! Get the symbol defined only in this scope.
  MATHPRESSO_INLINE AstSymbol* get_symbol(const StringRef& name, uint32_t hash_code) {
    return _symbols.get(name, hash_code);
  }

  //! Put a given symbol to this scope.
  //!
  //! \note The function doesn't care about duplicates. The correct flow is
  //! to call `resolve_symbol()` or `get_symbol()` and then `put_symbol()` based
  //! on the result. You should never call `put_symbol()` without checking if
  //! the symbol is already there.
  MATHPRESSO_INLINE void put_symbol(AstSymbol* symbol) {
    _symbols.put(symbol);
  }

  //! Resolve the symbol by traversing all parent scopes if not found in this
  //! one. An optional `scope_out` argument can be used to get scope where the
  //! `name` has been found.
  MATHPRESSO_NOAPI AstSymbol* resolve_symbol(const StringRef& name, uint32_t hash_code, AstScope** scope_out = nullptr);

  MATHPRESSO_INLINE AstSymbol* resolve_symbol(const StringRef& name) {
    return resolve_symbol(name, HashUtils::hash_string(name.data(), name.size()));
  }

  MATHPRESSO_NOAPI AstSymbol* remove_symbol(AstSymbol* symbol) {
    return _symbols.del(symbol);
  }
};

// MathPresso - AstNode
// ====================

#define MATHPRESSO_AST_CHILD(INDEX, NODE_T, NAME) \
  MATHPRESSO_INLINE NODE_T* NAME() const { return _##NAME; } \
  MATHPRESSO_INLINE NODE_T* set_##NAME(NODE_T* node) { return static_cast<NODE_T*>(replace_at(INDEX, node)); } \
  \
  MATHPRESSO_INLINE NODE_T* unlink_##NAME() { \
    NODE_T* node = _##NAME; \
    \
    MATHPRESSO_ASSERT(node != nullptr); \
    MATHPRESSO_ASSERT(node->_parent == this); \
    \
    node->_parent = nullptr; \
    _##NAME = nullptr; \
    \
    return node; \
  } \
  \
  NODE_T* _##NAME

struct AstNode {
  MATHPRESSO_NONCOPYABLE(AstNode)

  // Members
  // -------

  //! AST builder.
  AstBuilder* _ast;
  //! Parent node.
  AstNode* _parent;
  //! Child nodes.
  AstNode** _children;

  //! Node type, see `AstNodeType`.
  uint8_t _node_type;
  //! Node flags, see `AstNodeFlags`.
  uint8_t _node_flags;
  //! Node size in bytes for `Arena`.
  uint8_t _node_size;
  //! Operator type, see `OpType`.
  uint8_t _op_type;

  //! Position (in characters) to the beginning of the program (default -1).
  uint32_t _position;

  //! Count of child-nodes.
  uint32_t _size;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstNode(AstBuilder* ast, uint32_t node_type, AstNode** children = nullptr, uint32_t size = 0)
    : _ast(ast),
      _parent(nullptr),
      _children(children),
      _node_type(static_cast<uint8_t>(node_type)),
      _node_flags(0),
      _node_size(0),
      _op_type(kOpNone),
      _position(~static_cast<uint32_t>(0)),
      _size(size) {}

  MATHPRESSO_INLINE void destroy(AstBuilder*) {}

  // Accessors
  // ---------

  //! Get the `AstBuilder` instance that created this node.
  MATHPRESSO_INLINE AstBuilder* ast() const { return _ast; }

  //! Check if the node has a parent.
  MATHPRESSO_INLINE bool has_parent() const { return _parent != nullptr; }
  //! Get the parent node.
  MATHPRESSO_INLINE AstNode* parent() const { return _parent; }

  //! Get whether the node is empty (has no children).
  MATHPRESSO_INLINE bool empty() const { return !_size; }
  //! Get children array.
  MATHPRESSO_INLINE AstNode** children() const { return reinterpret_cast<AstNode**>(_children); }
  //! Get size of children array.
  MATHPRESSO_INLINE uint32_t size() const { return _size; }

  //! Get node type.
  MATHPRESSO_INLINE uint32_t node_type() const { return _node_type; }
  //! Get whether the node is `AstVar`.
  MATHPRESSO_INLINE bool is_var() const { return _node_type == kAstNodeVar; }
  //! Get whether the node is `AstImm`.
  MATHPRESSO_INLINE bool is_imm() const { return _node_type == kAstNodeImm; }

  //! Get whether the node has flag `flag`.
  MATHPRESSO_INLINE bool has_node_flag(uint32_t flag) const { return (static_cast<uint32_t>(_node_flags) & flag) != 0; }
  //! Get node flags.
  MATHPRESSO_INLINE uint32_t node_flags() const { return _node_flags; }
  //! Set node flags.
  MATHPRESSO_INLINE void set_node_flags(uint32_t flags) { _node_flags = static_cast<uint8_t>(flags); }
  //! Add node flags.
  MATHPRESSO_INLINE void add_node_flags(uint32_t flags) { _node_flags |= static_cast<uint8_t>(flags); }

  //! Get node size (in bytes).
  MATHPRESSO_INLINE uint32_t node_size() const { return _node_size; }

  //! Get op.
  MATHPRESSO_INLINE uint32_t op_type() const { return _op_type; }
  //! Set op.
  MATHPRESSO_INLINE void set_op_type(uint32_t op_type) { _op_type = static_cast<uint8_t>(op_type); }

  //! Get whether the node has associated position in source code.
  MATHPRESSO_INLINE bool has_position() const { return _position != ~static_cast<uint32_t>(0); }
  //! Get source code position of the node.
  MATHPRESSO_INLINE uint32_t position() const { return _position; }
  //! Set source code position of the node.
  MATHPRESSO_INLINE void set_position(uint32_t position) { _position = position; }
  //! Reset source code position of the node.
  MATHPRESSO_INLINE void reset_position() { _position = ~static_cast<uint32_t>(0); }

  // Children
  // --------

  MATHPRESSO_INLINE AstNode* child_at(uint32_t index) const {
    MATHPRESSO_ASSERT(index < _size);
    return _children[index];
  }

  //! Replace `ref_node` by `node`.
  AstNode* replace_node(AstNode* ref_node, AstNode* node);
  //! Replace node at index `index` by `node`.
  AstNode* replace_at(uint32_t index, AstNode* node);

  //! Inject `node` between this node and `ref_node`.
  AstNode* inject_node(AstNode* ref_node, AstUnary* node);
  //! Inject `node` between this node and node at index `index`.
  AstNode* inject_at(uint32_t index, AstUnary* node);
};

// MathPresso - AstBlock
// =====================

struct AstBlock : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstBlock)

  // Members
  // -------

  uint32_t _capacity = 0u;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstBlock(AstBuilder* ast, uint32_t node_type = kAstNodeBlock)
    : AstNode(ast, node_type) {}

  // Operations
  // ----------

  //! Reserve the capacity of the AstBlock so one more node can be added into it.
  //!
  //! \note This has to be called before you use `append_node()` or `insert_at()`.
  //! The reason is that it's easier to deal with possible allocation failure
  //! here (before the node to be added is created) than after the node is
  //! created, but failed to add into the block.
  Error will_add();

  //! Append the given `node` to the block.
  //!
  //! \note You have to call `will_add()` before you use `append_node()` for every
  //! node you want to add to the block.
  MATHPRESSO_INLINE void append_node(AstNode* node) {
    MATHPRESSO_ASSERT(node != nullptr);
    MATHPRESSO_ASSERT(node->parent() == nullptr);

    // We expect `will_add()` to be called before `append_node()`.
    MATHPRESSO_ASSERT(_size < _capacity);

    node->_parent = this;

    _children[_size] = node;
    _size++;
  }

  //! Insert the given `node` to the block at index `i`.
  //!
  //! \note You have to call `will_add()` before you use `insert_at()` for every
  //! node you want to add to the block.
  MATHPRESSO_INLINE void insert_at(uint32_t i, AstNode* node) {
    MATHPRESSO_ASSERT(node != nullptr);
    MATHPRESSO_ASSERT(node->parent() == nullptr);

    // We expect `will_add()` to be called before `insert_at()`.
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
  AstNode* remove_node(AstNode* node);
  //! Remove the node at index `index`.
  AstNode* remove_at(uint32_t index);
};

// MathPresso - AstUnary
// =====================

struct AstUnary : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstUnary)

  // Members
  // -------

  MATHPRESSO_AST_CHILD(0, AstNode, child);

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstUnary(AstBuilder* ast, uint32_t node_type)
    : AstNode(ast, node_type, &_child, 1),
      _child(nullptr) {}

  // Accessors
  // ---------

  MATHPRESSO_INLINE AstNode** children() const { return (AstNode**)&_child; }
};

// MathPresso - AstBinary
// ======================

struct AstBinary : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstBinary)

  // Members
  // -------

  MATHPRESSO_AST_CHILD(0, AstNode, left);
  MATHPRESSO_AST_CHILD(1, AstNode, right);

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstBinary(AstBuilder* ast, uint32_t node_type)
    : AstNode(ast, node_type, &_left, 2),
      _left(nullptr),
      _right(nullptr) {}

  // Accessors
  // ---------

  MATHPRESSO_INLINE AstNode** children() const { return (AstNode**)&_left; }
};

// MathPresso - AstProgram
// =======================

struct AstProgram : public AstBlock {
  MATHPRESSO_NONCOPYABLE(AstProgram)

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstProgram(AstBuilder* ast)
    : AstBlock(ast, kAstNodeProgram) {}
};

// MathPresso - AstVarDecl
// =======================

struct AstVarDecl : public AstUnary {
  MATHPRESSO_NONCOPYABLE(AstVarDecl)

  // Members
  // -------

  AstSymbol* _symbol;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstVarDecl(AstBuilder* ast)
    : AstUnary(ast, kAstNodeVarDecl),
      _symbol(nullptr) {}

  MATHPRESSO_INLINE void destroy(AstBuilder*) {
    if (symbol())
      symbol()->decrement_used_count();
  }

  // Accessors
  // ---------

  MATHPRESSO_INLINE AstSymbol* symbol() const { return _symbol; }
  MATHPRESSO_INLINE void set_symbol(AstSymbol* symbol) { _symbol = symbol; }
};

// MathPresso - AstVar
// ===================

struct AstVar : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstVar)

  // Members
  // -------

  AstSymbol* _symbol;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstVar(AstBuilder* ast)
    : AstNode(ast, kAstNodeVar),
      _symbol(nullptr) {}

  // Accessors
  // ---------

  MATHPRESSO_INLINE AstSymbol* symbol() const { return _symbol; }
  MATHPRESSO_INLINE void set_symbol(AstSymbol* symbol) { _symbol = symbol; }
};

// MathPresso - AstImm
// ===================

struct AstImm : public AstNode {
  MATHPRESSO_NONCOPYABLE(AstImm)

  // Members
  // -------

  double _value;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstImm(AstBuilder* ast, double value = 0.0)
    : AstNode(ast, kAstNodeImm),
      _value(value) {}

  // Accessors
  // ---------

  MATHPRESSO_INLINE double value() const { return _value; }
  MATHPRESSO_INLINE void set_value(double value) { _value = value; }
};

// MathPresso - AstUnaryOp
// =======================

struct AstUnaryOp : public AstUnary {
  MATHPRESSO_NONCOPYABLE(AstUnaryOp)

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstUnaryOp(AstBuilder* ast, uint32_t op_type)
    : AstUnary(ast, kAstNodeUnaryOp) { set_op_type(op_type); }
};

// MathPresso - AstBinaryOp
// ========================

struct AstBinaryOp : public AstBinary {
  MATHPRESSO_NONCOPYABLE(AstBinaryOp)

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstBinaryOp(AstBuilder* ast, uint32_t op_type)
    : AstBinary(ast, kAstNodeBinaryOp) { set_op_type(op_type); }

  MATHPRESSO_INLINE void destroy(AstBuilder*) {
    if (op_info_table[op_type()].is_assignment() && left()) {
      AstVar* var = static_cast<AstVar*>(left());
      if (var->symbol())
        var->symbol()->decrement_write_count();
    }
  }
};

// MathPresso - AstCall
// ====================

struct AstCall : public AstBlock {
  MATHPRESSO_NONCOPYABLE(AstCall)

  // Members
  // -------

  AstSymbol* _symbol;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstCall(AstBuilder* ast)
    : AstBlock(ast, kAstNodeCall),
      _symbol(nullptr) {}

  // Accessors
  // ---------

  MATHPRESSO_INLINE AstSymbol* symbol() const { return _symbol; }
  MATHPRESSO_INLINE void set_symbol(AstSymbol* symbol) { _symbol = symbol; }
};

// MathPresso - AstVisitor
// =======================

struct AstVisitor {
  MATHPRESSO_NONCOPYABLE(AstVisitor)

  // Members
  // -------

  AstBuilder* _ast;

  // Construction & Destruction
  // --------------------------

  AstVisitor(AstBuilder* ast);
  virtual ~AstVisitor();

  // Accessors
  // ---------

  MATHPRESSO_INLINE AstBuilder* ast() const { return _ast; }

  // OnNode
  // ------

  virtual Error on_node(AstNode* node);

  virtual Error on_program(AstProgram* node);
  virtual Error on_block(AstBlock* node) = 0;
  virtual Error on_var_decl(AstVarDecl* node) = 0;
  virtual Error on_var(AstVar* node) = 0;
  virtual Error on_imm(AstImm* node) = 0;
  virtual Error on_unary_op(AstUnaryOp* node) = 0;
  virtual Error on_binary_op(AstBinaryOp* node) = 0;
  virtual Error on_invoke(AstCall* node) = 0;
};

// MathPresso - AstDump
// ====================

struct AstDump : public AstVisitor {
  MATHPRESSO_NONCOPYABLE(AstDump)

  // Members
  // -------

  String& _sb;
  uint32_t _level;

  // Construction & Destruction
  // --------------------------

  AstDump(AstBuilder* ast, String& sb);
  virtual ~AstDump();

  // OnNode
  // ------

  virtual Error on_block(AstBlock* node);
  virtual Error on_var_decl(AstVarDecl* node);
  virtual Error on_var(AstVar* node);
  virtual Error on_imm(AstImm* node);
  virtual Error on_unary_op(AstUnaryOp* node);
  virtual Error on_binary_op(AstBinaryOp* node);
  virtual Error on_invoke(AstCall* node);

  // Helpers
  // -------

  Error info(const char* fmt, ...);
  Error nest(const char* fmt, ...);
  Error denest();
};

} // {mathpresso}

// [Guard]
#endif // _MATHPRESSO_MPAST_P_H
