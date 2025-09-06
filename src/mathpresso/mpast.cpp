// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mpast_p.h"

namespace mathpresso {

// MathPresso - AstNodeSize
// ========================

struct AstNodeSize {
  // Members
  // -------

  uint8_t _node_type;
  uint8_t _reserved;
  uint16_t _node_size;

  // Accessors
  // ---------

  MATHPRESSO_INLINE uint32_t node_type() const { return _node_type; }
  MATHPRESSO_INLINE uint32_t node_size() const { return _node_size; }
};

#define ROW(type, size) { type, 0, static_cast<uint8_t>(size) }
static const AstNodeSize ast_node_size_table[] = {
  ROW(kAstNodeNone     , 0                   ),
  ROW(kAstNodeProgram  , sizeof(AstProgram)  ),
  ROW(kAstNodeBlock    , sizeof(AstBlock)    ),
  ROW(kAstNodeVarDecl  , sizeof(AstVarDecl)  ),
  ROW(kAstNodeVar      , sizeof(AstVar)      ),
  ROW(kAstNodeImm      , sizeof(AstImm)      ),
  ROW(kAstNodeUnaryOp  , sizeof(AstUnaryOp)  ),
  ROW(kAstNodeBinaryOp , sizeof(AstBinaryOp) ),
  ROW(kAstNodeCall     , sizeof(AstCall)     )
};
#undef ROW

// MathPresso - AstBuilder
// =======================

AstBuilder::AstBuilder(Arena& arena)
  : _arena(arena),
    _root_scope(nullptr),
    _program_node(nullptr),
    _num_slots(0) {}
AstBuilder::~AstBuilder() {}

AstScope* AstBuilder::new_scope(AstScope* parent, uint32_t scope_type) {
  void* p = _arena.alloc_reusable(sizeof(AstScope));
  if (p == nullptr) {
    return nullptr;
  }
  return new(p) AstScope(this, parent, scope_type);
}

void AstBuilder::delete_scope(AstScope* scope) {
  scope->~AstScope();
  _arena.free_reusable(scope, sizeof(AstScope));
}

AstSymbol* AstBuilder::new_symbol(const StringRef& key, uint32_t hash_code, uint32_t symbol_type, uint32_t scope_type) {
  size_t keySize = key.size();
  void* p = _arena.alloc_reusable(sizeof(AstSymbol) + keySize + 1);

  if (p == nullptr)
    return nullptr;

  char* keyData = static_cast<char*>(p) + sizeof(AstSymbol);
  ::memcpy(keyData, key.data(), keySize);

  keyData[keySize] = '\0';
  return new(p) AstSymbol(keyData, static_cast<uint32_t>(keySize), hash_code, symbol_type, scope_type);
}

AstSymbol* AstBuilder::shadow_symbol(const AstSymbol* other) {
  StringRef name(other->name(), other->name_size());
  AstSymbol* sym = new_symbol(name, other->hash_code(), other->symbol_type(), kAstScopeShadow);

  if (sym == nullptr)
    return nullptr;

  sym->_op_type = other->_op_type;
  sym->_symbol_flags = other->_symbol_flags;

  switch (sym->symbol_type()) {
    case kAstSymbolVariable: {
      sym->_var_slot_id = other->_var_slot_id;
      sym->_var_offset = other->_var_offset;
      sym->_value = other->_value;
      break;
    }

    case kAstSymbolFunction: {
      sym->_func_ptr = other->_func_ptr;
      sym->_func_args = other->_func_args;
      break;
    }
  }

  return sym;
}

void AstBuilder::delete_symbol(AstSymbol* symbol) {
  size_t keySize = symbol->name_size();
  symbol->~AstSymbol();
  _arena.free_reusable(symbol, sizeof(AstSymbol) + keySize + 1);
}

void AstBuilder::delete_node(AstNode* node) {
  uint32_t size = node->size();
  AstNode** children = node->children();

  uint32_t node_type = node->node_type();
  MATHPRESSO_ASSERT(ast_node_size_table[node_type].node_type() == node_type);

  switch (node_type) {
    case kAstNodeProgram  : static_cast<AstProgram*  >(node)->destroy(this); break;
    case kAstNodeBlock    : static_cast<AstBlock*    >(node)->destroy(this); break;
    case kAstNodeVarDecl  : static_cast<AstVarDecl*  >(node)->destroy(this); break;
    case kAstNodeVar      : static_cast<AstVar*      >(node)->destroy(this); break;
    case kAstNodeImm      : static_cast<AstImm*      >(node)->destroy(this); break;
    case kAstNodeUnaryOp  : static_cast<AstUnaryOp*  >(node)->destroy(this); break;
    case kAstNodeBinaryOp : static_cast<AstBinaryOp* >(node)->destroy(this); break;
    case kAstNodeCall     : static_cast<AstCall*     >(node)->destroy(this); break;
  }

  for (uint32_t i = 0; i < size; i++) {
    AstNode* child = children[i];
    if (child != nullptr)
      delete_node(child);
  }

  _arena.free_reusable(node, ast_node_size_table[node_type].node_size());
}

Error AstBuilder::init_program_scope() {
  if (_root_scope == nullptr) {
    _root_scope = new_scope(nullptr, kAstScopeGlobal);
    MATHPRESSO_NULLCHECK(_root_scope);
  }

  if (_program_node == nullptr) {
    _program_node = new_node<AstProgram>();
    MATHPRESSO_NULLCHECK(_program_node);
  }

  return kErrorOk;
}

Error AstBuilder::dump(String& sb) {
  return AstDump(this, sb).on_program(program_node());
}

// MathPresso - AstScope
// =====================

struct AstScopeReleaseHandler {
  MATHPRESSO_INLINE AstScopeReleaseHandler(AstBuilder* ast) : _ast(ast) {}
  MATHPRESSO_INLINE void release(AstSymbol* node) { _ast->delete_symbol(node); }

  AstBuilder* _ast;
};

AstScope::AstScope(AstBuilder* ast, AstScope* parent, uint32_t scope_type)
  : _ast(ast),
    _parent(parent),
    _symbols(ast->arena()),
    _scope_type(static_cast<uint8_t>(scope_type)) {}

AstScope::~AstScope() {
  AstScopeReleaseHandler handler(_ast);
  _symbols.reset(handler);
}

AstSymbol* AstScope::resolve_symbol(const StringRef& name, uint32_t hash_code, AstScope** scope_out) {
  AstScope* scope = this;
  AstSymbol* symbol;

  do {
    symbol = scope->_symbols.get(name, hash_code);
  } while (symbol == nullptr && (scope = scope->parent()) != nullptr);

  if (scope_out != nullptr)
    *scope_out = scope;

  return symbol;
}

// MathPresso - AstNode
// ====================

AstNode* AstNode::replace_node(AstNode* ref_node, AstNode* node) {
  MATHPRESSO_ASSERT(ref_node != nullptr);
  MATHPRESSO_ASSERT(ref_node->parent() == this);
  MATHPRESSO_ASSERT(node == nullptr || !node->has_parent());

  uint32_t size = _size;
  AstNode** children = _children;

  for (uint32_t i = 0; i < size; i++) {
    AstNode* child = children[i];

    if (child != ref_node)
      continue;

    children[i] = node;
    ref_node->_parent = nullptr;

    if (node != nullptr)
      node->_parent = this;

    return ref_node;
  }

  return nullptr;
}

AstNode* AstNode::replace_at(uint32_t index, AstNode* node) {
  AstNode* child = child_at(index);
  _children[index] = node;

  if (child != nullptr)
    child->_parent = nullptr;

  if (node != nullptr)
    node->_parent = this;

  return child;
}

AstNode* AstNode::inject_node(AstNode* ref_node, AstUnary* node) {
  MATHPRESSO_ASSERT(ref_node != nullptr && ref_node->parent() == this);
  MATHPRESSO_ASSERT(node != nullptr && node->parent() == nullptr);

  uint32_t size = _size;
  AstNode** children = _children;

  for (uint32_t i = 0; i < size; i++) {
    AstNode* child = children[i];

    if (child != ref_node)
      continue;

    children[i] = node;
    ref_node->_parent = node;

    node->_parent = this;
    node->_child = ref_node;

    return ref_node;
  }

  return nullptr;
}

AstNode* AstNode::inject_at(uint32_t index, AstUnary* node) {
  AstNode* child = child_at(index);

  MATHPRESSO_ASSERT(node != nullptr && node->parent() == nullptr);
  MATHPRESSO_ASSERT(child != nullptr);

  _children[index] = node;
  child->_parent = node;

  node->_parent = this;
  node->_child = child;

  return child;
}

// MathPresso - AstBlock
// =====================

static Error block_node_grow(AstBlock* self) {
  size_t old_capacity = self->_capacity;
  size_t new_capacity = old_capacity;

  size_t size = self->_size;
  MATHPRESSO_ASSERT(old_capacity == size);

  // Grow, we prefer growing quickly until we reach 128 and then 1024 nodes. We
  // don't expect to reach these limits in the most used expressions; only test
  // cases can exploit this assumption.
  //
  // Growing schema:
  //   0..4..8..16..32..64..128..256..384..512..640..768..896..1024..[+256]
  if (new_capacity == 0)
    new_capacity = 4;
  else if (new_capacity < 128)
    new_capacity *= 2;
  else if (new_capacity < 1024)
    new_capacity += 128;
  else
    new_capacity += 256;

  Arena& arena = self->ast()->arena();

  size_t allocated_capacity;
  AstNode** old_array = self->children();
  AstNode** new_array = arena.alloc_reusable<AstNode*>(Arena::aligned_size(new_capacity * sizeof(AstNode)), asmjit::Out(allocated_capacity));

  MATHPRESSO_NULLCHECK(new_array);
  allocated_capacity /= sizeof(AstNode*);

  self->_children = new_array;
  self->_capacity = static_cast<uint32_t>(allocated_capacity);

  if (old_capacity != 0) {
    ::memcpy(new_array, old_array, size * sizeof(AstNode*));
    arena.free_reusable(old_array, old_capacity * sizeof(AstNode*));
  }

  return kErrorOk;
}

Error AstBlock::will_add() {
  // Grow if needed.
  if (_size == _capacity) {
    MATHPRESSO_PROPAGATE(block_node_grow(this));
  }
  return kErrorOk;
}

AstNode* AstBlock::remove_node(AstNode* node) {
  MATHPRESSO_ASSERT(node != nullptr);
  MATHPRESSO_ASSERT(node->parent() == this);

  AstNode** ptr = children();
  AstNode** ptr_end = ptr + _size;

  while (ptr != ptr_end) {
    if (ptr[0] == node)
      goto _Found;
    ptr++;
  }

  // If remove_node() has been called we expect the node to be found. Otherwise
  // there is a bug somewhere.
  MATHPRESSO_ASSERT(!"Reached");
  return nullptr;

_Found:
  _size--;
  ::memmove(ptr, ptr + 1, static_cast<size_t>(ptr_end - ptr - 1) * sizeof(AstNode*));

  node->_parent = nullptr;
  return node;
}

AstNode* AstBlock::remove_at(uint32_t index) {
  MATHPRESSO_ASSERT(index < _size);

  if (index >= _size)
    return nullptr;

  AstNode** ptr = children() + index;
  AstNode* old_node = ptr[0];

  _size--;
  ::memmove(ptr, ptr + 1, static_cast<size_t>(_size - index) * sizeof(AstNode*));

  old_node->_parent = nullptr;
  return old_node;
}

// MathPresso - AstVisitor
// =======================

AstVisitor::AstVisitor(AstBuilder* ast)
  : _ast(ast) {}
AstVisitor::~AstVisitor() {}

Error AstVisitor::on_node(AstNode* node) {
  switch (node->node_type()) {
    case kAstNodeProgram  : return on_program  (static_cast<AstProgram*  >(node));
    case kAstNodeBlock    : return on_block    (static_cast<AstBlock*    >(node));
    case kAstNodeVarDecl  : return on_var_decl (static_cast<AstVarDecl*  >(node));
    case kAstNodeVar      : return on_var      (static_cast<AstVar*      >(node));
    case kAstNodeImm      : return on_imm      (static_cast<AstImm*      >(node));
    case kAstNodeUnaryOp  : return on_unary_op (static_cast<AstUnaryOp*  >(node));
    case kAstNodeBinaryOp : return on_binary_op(static_cast<AstBinaryOp* >(node));
    case kAstNodeCall     : return on_invoke   (static_cast<AstCall*     >(node));

    default:
      return MATHPRESSO_TRACE_ERROR(kErrorInvalidState);
  }
}

Error AstVisitor::on_program(AstProgram* node) {
  return on_block(node);
}

// MathPresso - AstDump
// ====================

AstDump::AstDump(AstBuilder* ast, String& sb)
  : AstVisitor(ast),
    _sb(sb),
    _level(0) {}
AstDump::~AstDump() {}

Error AstDump::on_block(AstBlock* node) {
  AstNode** children = node->children();
  uint32_t i, size = node->size();

  for (i = 0; i < size; i++)
    on_node(children[i]);

  return kErrorOk;
}

Error AstDump::on_var_decl(AstVarDecl* node) {
  AstSymbol* sym = node->symbol();

  nest("%s [VarDecl]", sym ? sym->name() : static_cast<const char*>(nullptr));
  if (node->child())
    MATHPRESSO_PROPAGATE(on_node(node->child()));
  return denest();
}

Error AstDump::on_var(AstVar* node) {
  AstSymbol* sym = node->symbol();
  return info("%s", sym ? sym->name() : static_cast<const char*>(nullptr));
}

Error AstDump::on_imm(AstImm* node) {
  return info("%f", node->_value);
}

Error AstDump::on_unary_op(AstUnaryOp* node) {
  nest("%s [Unary]", OpInfo::get(node->op_type()).name);
  if (node->child())
    MATHPRESSO_PROPAGATE(on_node(node->child()));
  return denest();
}

Error AstDump::on_binary_op(AstBinaryOp* node) {
  nest("%s [Binary]", OpInfo::get(node->op_type()).name);
  if (node->left())
    MATHPRESSO_PROPAGATE(on_node(node->left()));
  if (node->right())
    MATHPRESSO_PROPAGATE(on_node(node->right()));
  return denest();
}

Error AstDump::on_invoke(AstCall* node) {
  AstSymbol* sym = node->symbol();

  nest("%s()", sym ? sym->name() : static_cast<const char*>(nullptr));
  on_block(node);
  return denest();
}

Error AstDump::info(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  _sb.append_chars(' ', static_cast<size_t>(_level) * 2);
  _sb.append_vformat(fmt, ap);
  _sb.append('\n');

  va_end(ap);
  return kErrorOk;
}

Error AstDump::nest(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  _sb.append_chars(' ', static_cast<size_t>(_level) * 2);
  _sb.append_vformat(fmt, ap);
  _sb.append('\n');

  va_end(ap);
  _level++;

  return kErrorOk;
}

Error AstDump::denest() {
  MATHPRESSO_ASSERT(_level > 0);
  _level--;

  return kErrorOk;
}

} // {mathpresso}
