// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_EXPORTS

// [Dependencies]
#include "./mpast_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::mpAstNodeSize]
// ============================================================================

struct AstNodeSize {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE uint32_t getNodeType() const { return _nodeType; }
  MATHPRESSO_INLINE uint32_t getNodeSize() const { return _nodeSize; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t _nodeType;
  uint8_t _reserved;
  uint16_t _nodeSize;
};

#define ROW(type, size) { type, 0, static_cast<uint8_t>(size) }
static const AstNodeSize mpAstNodeSize[] = {
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

// ============================================================================
// [mathpresso::AstBuilder - Construction / Destruction]
// ============================================================================

AstBuilder::AstBuilder(Allocator* allocator)
  : _allocator(allocator),
    _globalScope(NULL),
    _programNode(NULL) {}
AstBuilder::~AstBuilder() {}

// ============================================================================
// [mathpresso::AstBuilder - Factory]
// ============================================================================

AstScope* AstBuilder::newScope(AstScope* parent, uint32_t scopeType) {
  void* p = _allocator->alloc(sizeof(AstScope));
  if (p == NULL)
    return NULL;
  return new(p) AstScope(this, parent, scopeType);
}

void AstBuilder::deleteScope(AstScope* scope) {
  scope->~AstScope();
  _allocator->release(scope, sizeof(AstScope));
}

AstSymbol* AstBuilder::newSymbol(const StringRef& key, uint32_t hVal, uint32_t symbolType, uint32_t scopeType) {
  size_t kLen = key.getLength();
  void* p = _allocator->alloc(sizeof(AstSymbol) + kLen + 1);

  if (p == NULL)
    return NULL;

  char* kStr = static_cast<char*>(p) + sizeof(AstSymbol);
  ::memcpy(kStr, key.getData(), kLen);

  kStr[kLen] = '\0';
  return new(p) AstSymbol(kStr, static_cast<uint32_t>(kLen), hVal, symbolType, scopeType);
}

void AstBuilder::deleteSymbol(AstSymbol* symbol) {
  size_t kLen = symbol->getLength();
  symbol->~AstSymbol();
  _allocator->release(symbol, sizeof(AstSymbol) + kLen + 1);
}

void AstBuilder::deleteNode(AstNode* node) {
  uint32_t length = node->getLength();
  AstNode** children = node->getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];
    if (child != NULL)
      deleteNode(child);
  }

  uint32_t nodeType = node->getNodeType();
  MATHPRESSO_ASSERT(mpAstNodeSize[nodeType].getNodeType() == nodeType);

  switch (nodeType) {
    case kAstNodeProgram  : static_cast<AstProgram*  >(node)->destroy(this); break;
    case kAstNodeBlock    : static_cast<AstBlock*    >(node)->destroy(this); break;
    case kAstNodeVarDecl  : static_cast<AstVarDecl*  >(node)->destroy(this); break;
    case kAstNodeVar      : static_cast<AstVar*      >(node)->destroy(this); break;
    case kAstNodeImm      : static_cast<AstImm*      >(node)->destroy(this); break;
    case kAstNodeUnaryOp  : static_cast<AstUnaryOp*  >(node)->destroy(this); break;
    case kAstNodeBinaryOp : static_cast<AstBinaryOp* >(node)->destroy(this); break;
    case kAstNodeCall     : static_cast<AstCall*     >(node)->destroy(this); break;
  }

  _allocator->release(node, mpAstNodeSize[nodeType].getNodeSize());
}

// ============================================================================
// [mathpresso::AstBuilder - Initialization]
// ============================================================================

Error AstBuilder::initProgramScope() {
  if (_globalScope == NULL) {
    _globalScope = newScope(NULL, kAstScopeGlobal);
    MATHPRESSO_NULLCHECK(_globalScope);
  }

  if (_programNode == NULL) {
    _programNode = newNode<AstProgram>();
    MATHPRESSO_NULLCHECK(_programNode);
  }

  return kErrorOk;
}

// ============================================================================
// [mathpresso::AstBuilder - Dump]
// ============================================================================

Error AstBuilder::dump(StringBuilder& sb) {
  return AstDump(this, sb).onProgram(getProgramNode());
}

// ============================================================================
// [mathpresso::AstScope - Construction / Destruction]
// ============================================================================

struct AstScopeReleaseHandler {
  MATHPRESSO_INLINE AstScopeReleaseHandler(AstBuilder* ast) : _ast(ast) {}
  MATHPRESSO_INLINE void release(AstSymbol* node) { _ast->deleteSymbol(node); }

  AstBuilder* _ast;
};

AstScope::AstScope(AstBuilder* ast, AstScope* parent, uint32_t scopeType)
  : _ast(ast),
    _parent(parent),
    _symbols(ast->getAllocator()),
    _scopeType(static_cast<uint8_t>(scopeType)) {}

AstScope::~AstScope() {
  AstScopeReleaseHandler handler(_ast);
  _symbols.reset(handler);
}

// ============================================================================
// [mathpresso::AstScope - Ops]
// ============================================================================

AstSymbol* AstScope::resolveSymbol(const StringRef& name, uint32_t hVal, AstScope** scopeOut) {
  AstScope* scope = this;
  AstSymbol* symbol;

  do {
    symbol = scope->_symbols.get(name, hVal);
  } while (symbol == NULL && (scope = scope->getParent()) != NULL);

  if (scopeOut != NULL)
    *scopeOut = scope;

  return symbol;
}

// ============================================================================
// [mathpresso::AstNode - Ops]
// ============================================================================

AstNode* AstNode::replaceNode(AstNode* refNode, AstNode* node) {
  MATHPRESSO_ASSERT(refNode != NULL);
  MATHPRESSO_ASSERT(refNode->getParent() == this);
  MATHPRESSO_ASSERT(node == NULL || !node->hasParent());

  uint32_t length = _length;
  AstNode** children = getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];

    if (child != refNode)
      continue;

    children[i] = node;
    refNode->_parent = NULL;

    if (node != NULL)
      node->_parent = this;

    return refNode;
  }

  return NULL;
}

AstNode* AstNode::replaceAt(uint32_t index, AstNode* node) {
  AstNode* child = getAt(index);
  _children[index] = node;

  if (child != NULL)
    child->_parent = NULL;

  if (node != NULL)
    node->_parent = this;

  return child;
}

AstNode* AstNode::injectNode(AstNode* refNode, AstUnary* node) {
  MATHPRESSO_ASSERT(refNode != NULL && refNode->getParent() == this);
  MATHPRESSO_ASSERT(node != NULL && node->getParent() == NULL);

  uint32_t length = _length;
  AstNode** children = getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];

    if (child != refNode)
      continue;

    children[i] = node;
    refNode->_parent = node;

    node->_parent = this;
    node->_child = refNode;

    return refNode;
  }

  return NULL;
}

AstNode* AstNode::injectAt(uint32_t index, AstUnary* node) {
  AstNode* child = getAt(index);

  MATHPRESSO_ASSERT(node != NULL && node->getParent() == NULL);
  MATHPRESSO_ASSERT(child != NULL);

  _children[index] = node;
  child->_parent = node;

  node->_parent = this;
  node->_child = child;

  return child;
}

// ============================================================================
// [mathpresso::AstBlock - Ops]
// ============================================================================

static Error mpBlockNodeGrow(AstBlock* self) {
  size_t oldCapacity = self->_capacity;
  size_t newCapacity = oldCapacity;

  size_t length = self->_length;
  MATHPRESSO_ASSERT(oldCapacity == length);

  // Grow, we prefer growing quickly until we reach 128 and then 1024 nodes. We
  // don't expect to reach these limits in the most used expressions; only test
  // cases can exploit this assumption.
  //
  // Growing schema:
  //   0..4..8..16..32..64..128..256..384..512..640..768..896..1024..[+256]
  if (newCapacity == 0)
    newCapacity = 4;
  else if (newCapacity < 128)
    newCapacity *= 2;
  else if (newCapacity < 1024)
    newCapacity += 128;
  else
    newCapacity += 256;

  Allocator* allocator = self->getAst()->getAllocator();

  AstNode** oldArray = self->getChildren();
  AstNode** newArray = static_cast<AstNode**>(allocator->alloc(newCapacity * sizeof(AstNode), newCapacity));

  MATHPRESSO_NULLCHECK(newArray);
  newCapacity /= sizeof(AstNode*);

  self->_children = newArray;
  self->_capacity = static_cast<uint32_t>(newCapacity);

  if (oldCapacity != 0) {
    ::memcpy(newArray, oldArray, length * sizeof(AstNode*));
    allocator->release(oldArray, oldCapacity * sizeof(AstNode*));
  }

  return kErrorOk;
}

Error AstBlock::willAdd() {
  // Grow if needed.
  if (_length == _capacity)
    MATHPRESSO_PROPAGATE(mpBlockNodeGrow(this));
  return kErrorOk;
}

AstNode* AstBlock::removeNode(AstNode* node) {
  MATHPRESSO_ASSERT(node != NULL);
  MATHPRESSO_ASSERT(node->getParent() == this);

  AstNode** p = getChildren();
  AstNode** pEnd = p + _length;

  while (p != pEnd) {
    if (p[0] == node)
      goto _Found;
    p++;
  }

  // If removeNode() has been called we expect the node to be found. Otherwise
  // there is a bug somewhere.
  MATHPRESSO_ASSERT(!"Reached");
  return NULL;

_Found:
  _length--;
  ::memmove(p, p + 1, static_cast<size_t>(pEnd - p - 1) * sizeof(AstNode*));

  node->_parent = NULL;
  return node;
}

AstNode* AstBlock::removeAt(uint32_t index) {
  MATHPRESSO_ASSERT(index < _length);

  if (index >= _length)
    return NULL;

  AstNode** p = getChildren() + index;
  AstNode* oldNode = p[0];

  _length--;
  ::memmove(p, p + 1, static_cast<size_t>(_length - index) * sizeof(AstNode*));

  oldNode->_parent = NULL;
  return oldNode;
}

// ============================================================================
// [mathpresso::AstVisitor - Construction / Destruction]
// ============================================================================

AstVisitor::AstVisitor(AstBuilder* ast)
  : _ast(ast) {}
AstVisitor::~AstVisitor() {}

// ============================================================================
// [mathpresso::AstVisitor - OnNode]
// ============================================================================

Error AstVisitor::onNode(AstNode* node) {
  switch (node->getNodeType()) {
    case kAstNodeProgram  : return onProgram  (static_cast<AstProgram*  >(node));
    case kAstNodeBlock    : return onBlock    (static_cast<AstBlock*    >(node));
    case kAstNodeVarDecl  : return onVarDecl  (static_cast<AstVarDecl*  >(node));
    case kAstNodeVar      : return onVar      (static_cast<AstVar*      >(node));
    case kAstNodeImm      : return onImm      (static_cast<AstImm*      >(node));
    case kAstNodeUnaryOp  : return onUnaryOp  (static_cast<AstUnaryOp*  >(node));
    case kAstNodeBinaryOp : return onBinaryOp (static_cast<AstBinaryOp* >(node));
    case kAstNodeCall     : return onCall     (static_cast<AstCall*     >(node));

    default:
      return MATHPRESSO_TRACE_ERROR(kErrorInvalidState);
  }
}

Error AstVisitor::onProgram(AstProgram* node) {
  return onBlock(node);
}

// ============================================================================
// [mathpresso::AstDump - Construction / Destruction]
// ============================================================================

AstDump::AstDump(AstBuilder* ast, StringBuilder& sb)
  : AstVisitor(ast),
    _sb(sb),
    _level(0) {}
AstDump::~AstDump() {}

// ============================================================================
// [mathpresso::AstDump - OnNode]
// ============================================================================

Error AstDump::onBlock(AstBlock* node) {
  AstNode** children = node->getChildren();
  uint32_t i, count = node->getLength();

  for (i = 0; i < count; i++)
    onNode(children[i]);

  return kErrorOk;
}

Error AstDump::onVarDecl(AstVarDecl* node) {
  AstSymbol* sym = node->getSymbol();

  nest("%s [VarDecl]", sym ? sym->getName() : static_cast<const char*>(NULL));
  if (node->hasChild())
    MATHPRESSO_PROPAGATE(onNode(node->getChild()));
  return denest();
}

Error AstDump::onVar(AstVar* node) {
  AstSymbol* sym = node->getSymbol();
  return info("%s", sym ? sym->getName() : static_cast<const char*>(NULL));
}

Error AstDump::onImm(AstImm* node) {
  return info("%f", node->_value);
}

Error AstDump::onUnaryOp(AstUnaryOp* node) {
  nest("%s [Unary]", OpInfo::get(node->getOp()).name);
  if (node->hasChild())
    MATHPRESSO_PROPAGATE(onNode(node->getChild()));
  return denest();
}

Error AstDump::onBinaryOp(AstBinaryOp* node) {
  nest("%s [Binary]", OpInfo::get(node->getOp()).name);
  if (node->hasLeft())
    MATHPRESSO_PROPAGATE(onNode(node->getLeft()));
  if (node->hasRight())
    MATHPRESSO_PROPAGATE(onNode(node->getRight()));
  return denest();
}

Error AstDump::onCall(AstCall* node) {
  AstSymbol* sym = node->getSymbol();

  nest("%s()", sym ? sym->getName() : static_cast<const char*>(NULL));
  onBlock(node);
  return denest();
}

// ============================================================================
// [mathpresso::AstDump - Helpers]
// ============================================================================

Error AstDump::info(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  _sb.appendChars(' ', static_cast<size_t>(_level) * 2);
  _sb.appendVFormat(fmt, ap);
  _sb.appendChar('\n');

  va_end(ap);
  return kErrorOk;
}

Error AstDump::nest(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  _sb.appendChars(' ', static_cast<size_t>(_level) * 2);
  _sb.appendVFormat(fmt, ap);
  _sb.appendChar('\n');

  va_end(ap);
  _level++;

  return kErrorOk;
}

Error AstDump::denest() {
  MATHPRESSO_ASSERT(_level > 0);
  _level--;

  return kErrorOk;
}

} // mathpresso namespace
