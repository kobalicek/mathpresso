// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mpparser_p.h"

namespace mathpresso {

// ============================================================================
// [mathpresso::Parser - Syntax Error]
// ============================================================================

#define MATHPRESSO_PARSER_ERROR(_Token_, ...) \
  return _errorReporter->onError( \
    kErrorInvalidSyntax, static_cast<uint32_t>((size_t)(_Token_.position())), __VA_ARGS__)

#define MATHPRESSO_PARSER_WARNING(_Token_, ...) \
  _errorReporter->onWarning( \
    static_cast<uint32_t>((size_t)(_Token_.position())), __VA_ARGS__)

// ============================================================================
// [mathpresso::AstNestedScope]
// ============================================================================

//! \internal
//!
//! Nested scope used only by the parser and always allocated statically.
struct AstNestedScope : public AstScope {
  MATHPRESSO_NONCOPYABLE(AstNestedScope)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MATHPRESSO_INLINE AstNestedScope(Parser* parser)
    : AstScope(parser->_ast, parser->_currentScope, kAstScopeNested),
      _parser(parser) {
    _parser->_currentScope = this;
  }

  MATHPRESSO_INLINE ~AstNestedScope() {
    AstScope* p = parent();
    MATHPRESSO_ASSERT(p != NULL);

    _parser->_currentScope = p;
    p->_symbols.mergeToInvisibleSlot(this->_symbols);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Parser* _parser;
};

// ============================================================================
// [mathpresso::Parser - Parse]
// ============================================================================

Error Parser::parseProgram(AstProgram* block) {
  for (;;) {
    Token token;
    uint32_t uToken = _tokenizer.peek(&token);

    // Parse the end of the input.
    if (uToken == kTokenEnd)
      break;
    MATHPRESSO_PROPAGATE(parseStatement(block, kEnableVarDecls | kEnableNestedBlock));
  }

  if (block->size() == 0)
    return MATHPRESSO_TRACE_ERROR(kErrorNoExpression);

  return kErrorOk;
}

// Parse <statement>; or { [<statement>; ...] }
Error Parser::parseStatement(AstBlock* block, uint32_t flags) {
  Token token;
  uint32_t uToken = _tokenizer.peek(&token);

  // Parse the ';' token.
  if (uToken == kTokenSemicolon) {
    _tokenizer.consume();
    return kErrorOk;
  }

  // Parse a nested block.
  if (uToken == kTokenLCurl) {
    AstBlock* nested;

    if (!(flags & kEnableNestedBlock))
      MATHPRESSO_PARSER_ERROR(token, "Cannot declare a new block-scope here.");

    MATHPRESSO_PROPAGATE(block->willAdd());
    MATHPRESSO_NULLCHECK(nested = _ast->newNode<AstBlock>());
    block->appendNode(nested);

    AstNestedScope tmpScope(this);
    return parseBlockOrStatement(nested);
  }

  // Parse a variable declaration.
  if (uToken == kTokenVar) {
    if (!(flags & kEnableVarDecls))
      MATHPRESSO_PARSER_ERROR(token, "Cannot declare a new variable here.");
    return parseVariableDecl(block);
  }

  // Parse an expression.
  AstNode* expression;

  MATHPRESSO_PROPAGATE(block->willAdd());
  MATHPRESSO_PROPAGATE(parseExpression(&expression, false));
  block->appendNode(expression);

  uToken = _tokenizer.peek(&token);
  if (uToken == kTokenSemicolon) {
    _tokenizer.consume();
    return kErrorOk;
  }

  if (uToken == kTokenEnd)
    return kErrorOk;

  MATHPRESSO_PARSER_ERROR(token, "Expected a ';' after an expression.");
}

// Parse <block|statement>;.
Error Parser::parseBlockOrStatement(AstBlock* block) {
  Token token;
  uint32_t uToken = _tokenizer.next(&token);

  // Parse the <block>, consume '{' token.
  block->setPosition(token.positionAsUInt());
  if (uToken == kTokenLCurl) {
    for (;;) {
      uToken = _tokenizer.peek(&token);

      // Parse the end of the block '}'.
      if (uToken == kTokenRCurl) {
        // Consume the '}' token.
        _tokenizer.consume();
        return kErrorOk;
      }
      else {
        MATHPRESSO_PROPAGATE(parseStatement(block, kEnableVarDecls | kEnableNestedBlock));
      }
    }
  }
  else {
    return parseStatement(block, kNoFlags);
  }
}

// Parse "var <name> = <expression>[, <name> = <expression>, ...];".
Error Parser::parseVariableDecl(AstBlock* block) {
  Token token;
  uint32_t uToken = _tokenizer.next(&token);
  StringRef str;

  bool isFirst = true;
  uint32_t position = token.positionAsUInt();

  // Parse the 'var' keyword.
  if (uToken != kTokenVar)
    MATHPRESSO_PARSER_ERROR(token, "Expected 'var' keyword.");

  AstScope* scope = _currentScope;
  for (;;) {
    // Parse the variable name.
    if (_tokenizer.next(&token) != kTokenSymbol)
      MATHPRESSO_PARSER_ERROR(token, isFirst
        ? "Expected a variable name after 'var' keyword."
        : "Expected a variable name after colon ','.");

    MATHPRESSO_PROPAGATE(block->willAdd());

    if (!isFirst)
      position = token.positionAsUInt();

    // Resolve the variable name.
    AstSymbol* vSym;
    AstScope* vScope;

    str.set(_tokenizer._start + token.position(), token.size());
    if ((vSym = scope->resolveSymbol(str, token.hashCode(), &vScope)) != NULL) {
      if (vSym->symbolType() != kAstSymbolVariable || scope == vScope)
        MATHPRESSO_PARSER_ERROR(token, "Attempt to redefine '%s'.", vSym->name());

      if (vSym->hasNode()) {
        uint32_t line, column;
        _errorReporter->getLineAndColumn(vSym->node()->position(), line, column);
        MATHPRESSO_PARSER_WARNING(token, "Variable '%s' shadows a variable declared at [%d:%d].", vSym->name(), line, column);
      }
      else {
        MATHPRESSO_PARSER_WARNING(token, "Variable '%s' shadows a variable of the same name.", vSym->name());
      }
    }

    vSym = _ast->newSymbol(str, token.hashCode(), kAstSymbolVariable, scope->scopeType());
    MATHPRESSO_NULLCHECK(vSym);
    scope->putSymbol(vSym);

    AstVarDecl* decl = _ast->newNode<AstVarDecl>();
    MATHPRESSO_NULLCHECK_(decl, { _ast->deleteSymbol(vSym); });

    decl->setPosition(position);
    decl->setSymbol(vSym);

    // Assign a slot and fill to safe defaults.
    vSym->setVarOffset(0);
    vSym->setVarSlotId(_ast->newSlotId());
    vSym->setNode(decl);

    // Parse possible assignment '='.
    uToken = _tokenizer.next(&token);
    bool isAssigned = (uToken == kTokenAssign);

    if (isAssigned) {
      AstNode* expression;
      MATHPRESSO_PROPAGATE_(parseExpression(&expression, false), { _ast->deleteNode(decl); });

      decl->setChild(expression);
      vSym->incWriteCount();

      uToken = _tokenizer.next(&token);
    }

    // Make the symbol declared so it can be referenced after now.
    vSym->setDeclared();

    // Parse the ',' or ';' tokens.
    if (uToken == kTokenComma || uToken == kTokenSemicolon || uToken == kTokenEnd) {
      block->appendNode(decl);

      // Token ';' terminates the declaration.
      if (uToken != kTokenComma)
        break;
    }
    else {
      _ast->deleteSymbol(vSym);
      MATHPRESSO_PARSER_ERROR(token, "Unexpected token.");
    }

    isFirst = false;
  }

  return kErrorOk;
}

Error Parser::parseExpression(AstNode** pNode, bool isNested) {
  AstScope* scope = _currentScope;

  // It's important that the given expression is parsed in a way that it can be
  // correctly evaluated. The `parseExpression()` function can handle expressions
  // that contain unary and binary operators combined with terminals (variables,
  // constants or function calls).
  //
  // The most expression parsers usually use stack to handle operator precedence,
  // but MPSL uses AstNode parent->child hierarchy instead. When operator with a
  // higher precedence is found it traverses down in the hierarchy and when
  // operator with the same/lower precedence is found the hierarchy is traversed
  // back.
  //
  //                               AST Examples
  //
  // +-----------------+-----------------+-----------------+-----------------+
  // |                 |                 |                 |                 |
  // |   "a + b + c"   |   "a * b + c"   |   "a + b * c"   |   "a * b * c"   |
  // |                 |                 |                 |                 |
  // |       (+)       |       (+)       |       (+)       |       (*)       |
  // |      /   \      |      /   \      |      /   \      |      /   \      |
  // |   (+)     (c)   |   (*)     (c)   |   (a)     (*)   |   (*)     (c)   |
  // |   / \           |   / \           |           / \   |   / \           |
  // | (a) (b)         | (a) (b)         |         (b) (c) | (a) (b)         |
  // |                 |                 |                 |                 |
  // +-----------------+-----------------+-----------------+-----------------+
  // |                 |                 |                 |                 |
  // | "a + b + c + d" | "a + b * c + d" | "a * b + c * d" | "a * b * c + d" |
  // |                 |                 |                 |                 |
  // |       (+)       |       (+)       |       (+)       |       (+)       |
  // |      /   \      |      /   \      |      /   \      |      /   \      |
  // |   (+)     (d)   |    (+)   (d)    |   (*)     (*)   |   (*)     (d)   |
  // |    | \          |   /   \         |   / \     / \   |    | \          |
  // |   (+) (c)       |(a)     (*)      | (a) (b) (c) (d) |   (*) (c)       |
  // |   / \           |        / \      |                 |   / \           |
  // | (a) (b)         |      (b) (c)    |                 | (a) (b)         |
  // |                 |                 |                 |                 |
  // +-----------------+-----------------+-----------------+-----------------+

  Token token;
  uint32_t op;

  // Current binary operator node. Initial NULL value means that the parsing
  // just started and there is no binary operator yet. Once the first binary
  // operator has been parsed `oNode` will be set accordingly.
  AstBinaryOp* oNode = NULL;

  // Currently parsed node.
  AstNode* tNode = NULL;

  for (;;) {
    // Last unary node. It's an optimization to prevent recursion in case that
    // we found two or more unary expressions after each other. For example the
    // expression "-!-1" contains only unary operators that will be parsed by
    // a single `parseExpression()` call.
    AstUnary* unary = NULL;

_Repeat1:
    switch (_tokenizer.next(&token)) {
      // Parse a variable, a constant, or a function-call. This can be repeated
      // one or several times based on the expression type. For unary nodes it's
      // repeated immediately, for binary nodes it's repeated after the binary
      // node is created.

      // Parse a symbol (variable or function name).
      case kTokenSymbol: {
        StringRef str(_tokenizer._start + token.position(), token.size());

        AstScope* symScope;
        AstSymbol* sym = scope->resolveSymbol(str, token.hashCode(), &symScope);

        if (sym == NULL)
          MATHPRESSO_PARSER_ERROR(token, "Unresolved symbol %.*s.", static_cast<int>(str.size()), str.data());

        uint32_t symType = sym->symbolType();
        AstNode* zNode;

        if (symType == kAstSymbolVariable) {
          if (!sym->isDeclared())
            MATHPRESSO_PARSER_ERROR(token, "Can't use variable '%s' that is being declared.", sym->name());

          // Put symbol to shadow scope if it's global. This is done lazily and
          // only once per symbol when it's referenced.
          if (symScope->isGlobal()) {
            sym = _ast->shadowSymbol(sym);
            MATHPRESSO_NULLCHECK(sym);

            sym->setVarSlotId(_ast->newSlotId());
            symScope = _ast->rootScope();
            symScope->putSymbol(sym);
          }

          zNode = _ast->newNode<AstVar>();
          MATHPRESSO_NULLCHECK(zNode);
          static_cast<AstVar*>(zNode)->setSymbol(sym);

          zNode->setPosition(token.positionAsUInt());
          sym->incUsedCount();
        }
        else {
          // Will be parsed by `parseCall()` again.
          _tokenizer.set(&token);
          MATHPRESSO_PROPAGATE(parseCall(&zNode));
        }

        if (unary == NULL)
          tNode = zNode;
        else
          unary->setChild(zNode);
        break;
      }

      // Parse a number.
      case kTokenNumber: {
        AstImm* zNode = _ast->newNode<AstImm>();
        MATHPRESSO_NULLCHECK(zNode);

        zNode->setPosition(token.positionAsUInt());
        zNode->_value = token.value();

        if (unary == NULL)
          tNode = zNode;
        else
          unary->setChild(zNode);
        break;
      }

      // Parse expression terminators - ',', ':', ';' or ')'.
      case kTokenComma:
      case kTokenColon:
      case kTokenSemicolon:
      case kTokenRParen: {
        MATHPRESSO_PARSER_ERROR(token, "Expected an expression.");
      }

      // Parse a nested expression.
      case kTokenLParen: {
        AstNode* zNode;
        MATHPRESSO_PROPAGATE(parseExpression(&zNode, true));

        if (_tokenizer.next(&token) != kTokenRParen)
          MATHPRESSO_PARSER_ERROR(token, "Expected a ')' token.");

        if (unary == NULL)
          tNode = zNode;
        else
          unary->setChild(zNode);

        break;
      }

      // Parse a right-to-left associative unary operator ('+', '-', "!").
      case kTokenAdd         : op = kOpNone  ; goto _Unary;
      case kTokenSub         : op = kOpNeg   ; goto _Unary;
      case kTokenNot         : op = kOpNot   ; goto _Unary;
_Unary: {
        // Parse the unary operator.
        AstUnaryOp* opNode = _ast->newNode<AstUnaryOp>(op);
        MATHPRESSO_NULLCHECK(opNode);
        opNode->setPosition(token.positionAsUInt());

        if (unary == NULL)
          tNode = opNode;
        else
          unary->setChild(opNode);

        isNested = true;
        unary = opNode;

        goto _Repeat1;
      }

      case kTokenEnd: {
        MATHPRESSO_PARSER_ERROR(token, "Unexpected end of the program.");
      }

      default: {
        MATHPRESSO_PARSER_ERROR(token, "Unexpected token.");
      }
    }

// _Repeat2:
    switch (_tokenizer.next(&token)) {
      // Parse the expression terminators - ',', ':', ';', ')' or EOI.
      case kTokenComma:
      case kTokenColon:
      case kTokenSemicolon:
      case kTokenRParen:
      case kTokenEnd: {
        _tokenizer.set(&token);

        if (oNode != NULL) {
          oNode->setRight(tNode);
          // Iterate to the top-most node.
          while (oNode->hasParent())
            oNode = static_cast<AstBinaryOp*>(oNode->parent());
          tNode = oNode;
        }

        *pNode = tNode;
        return kErrorOk;
      }

      // Parse a binary operator.
      case kTokenAssign: {
        op = kOpAssign;

        // Check whether the assignment is valid.
        if (tNode->nodeType() != kAstNodeVar)
          MATHPRESSO_PARSER_ERROR(token, "Can't assign to a non-variable.");

        AstSymbol* sym = static_cast<AstVar*>(tNode)->symbol();
        if (sym->hasSymbolFlag(kAstSymbolIsReadOnly))
          MATHPRESSO_PARSER_ERROR(token, "Can't assign to a read-only variable '%s'.", sym->name());

        if (isNested)
          MATHPRESSO_PARSER_ERROR(token, "Invalid assignment inside an expression.");

        sym->incWriteCount();
        goto _Binary;
      }

      case kTokenEq          : op = kOpEq          ; goto _Binary;
      case kTokenNe          : op = kOpNe          ; goto _Binary;
      case kTokenGt          : op = kOpGt          ; goto _Binary;
      case kTokenGe          : op = kOpGe          ; goto _Binary;
      case kTokenLt          : op = kOpLt          ; goto _Binary;
      case kTokenLe          : op = kOpLe          ; goto _Binary;
      case kTokenAdd         : op = kOpAdd         ; goto _Binary;
      case kTokenSub         : op = kOpSub         ; goto _Binary;
      case kTokenMul         : op = kOpMul         ; goto _Binary;
      case kTokenDiv         : op = kOpDiv         ; goto _Binary;
      case kTokenMod         : op = kOpMod         ; goto _Binary;
_Binary: {
        AstBinaryOp* zNode = _ast->newNode<AstBinaryOp>(op);
        MATHPRESSO_NULLCHECK(zNode);
        zNode->setPosition(token.positionAsUInt());

        if (oNode == NULL) {
          // oNode <------+
          //              |
          // +------------+------------+ First operand - oNode becomes the newly
          // |         (zNode)         | created zNode; tNode is assigned to the
          // |        /       \        | left side of zNode and will be referred
          // |     (tNode)  (NULL)     | as (...) by the next operation.
          // +-------------------------+
          zNode->setLeft(tNode);
          oNode = zNode;
          break;
        }

        uint32_t oPrec = OpInfo::get(oNode->opType()).precedence;
        uint32_t zPrec = OpInfo::get(op).precedence;

        if (oPrec > zPrec) {
          // oNode <----------+
          //                  |
          // +----------------+--------+ The current operator (zPrec) has a
          // |     (oNode)    |        | higher precedence than the previous one
          // |    /       \   |        | (oPrec), so the zNode will be assigned
          // | (...)       (zNode)     | to the right side of oNode and it will
          // |            /       \    | function as a stack-like structure. We
          // |         (tNode)  (NULL) | have to advance back at some point.
          // +-------------------------+
          oNode->setRight(zNode);
          zNode->setLeft(tNode);
          oNode = zNode;
          break;
        }
        else {
          oNode->setRight(tNode);

          // Advance to the top-most oNode that has less or equal precedence
          // than zPrec.
          while (oNode->hasParent()) {
            // Terminate conditions:
            //   1. oNode has higher precedence than zNode.
            //   2. oNode has equal precedence and right-to-left associativity.
            if (OpInfo::get(oNode->opType()).rightAssociate(zPrec))
              break;
            oNode = static_cast<AstBinaryOp*>(oNode->parent());
          }

          // oNode <------+
          //              |
          // +------------+------------+
          // |         (zNode)         | Simple case - oNode becomes the left
          // |        /       \        | node in the created zNode and zNode
          // |     (oNode)  (NULL)     | becomes oNode for the next operator.
          // |    /       \            |
          // | (...)    (tNode)        | oNode will become a top-level node.
          // +-------------------------+
          if (!oNode->hasParent() && !OpInfo::get(oNode->opType()).rightAssociate(zPrec)) {
            zNode->setLeft(oNode);
          }
          // oNode <----------+
          //                  |
          // +----------------+--------+
          // |     (oNode)    |        |
          // |    /       \   |        | Complex case - inject node in place
          // | (...)       (zNode)     | of oNode.right (because of higher
          // |            /       \    | precedence or RTL associativity).
          // |        (tNode)   (NULL) |
          // +-------------------------+
          else {
            AstNode* pNode = oNode->unlinkRight();
            oNode->setRight(zNode);
            zNode->setLeft(pNode);
          }

          isNested = true;
          oNode = zNode;

          break;
        }
      }

      default: {
        MATHPRESSO_PARSER_ERROR(token, "Unexpected token.");
      }
    }
  }
}

// Parse "function([arg1 [, arg2, ...] ])".
Error Parser::parseCall(AstNode** pNodeOut) {
  Token token;
  uint32_t uToken;

  uToken = _tokenizer.next(&token);
  MATHPRESSO_ASSERT(uToken == kTokenSymbol);
  uint32_t position = token.positionAsUInt();

  StringRef str(_tokenizer._start + token.position(), token.size());
  AstSymbol* sym = _currentScope->resolveSymbol(str, token.hashCode());

  if (sym == NULL)
    MATHPRESSO_PARSER_ERROR(token, "Unresolved symbol %.*s.", static_cast<int>(str.size()), str.data());

  if (sym->symbolType() != kAstSymbolIntrinsic && sym->symbolType() != kAstSymbolFunction)
    MATHPRESSO_PARSER_ERROR(token, "Expected a function name.");

  uToken = _tokenizer.next(&token);
  if (uToken != kTokenLParen)
    MATHPRESSO_PARSER_ERROR(token, "Expected a '(' token after a function name.");

  AstCall* callNode = _ast->newNode<AstCall>();
  MATHPRESSO_NULLCHECK(callNode);

  callNode->setSymbol(sym);
  callNode->setPosition(position);

  uToken = _tokenizer.peek(&token);
  if (uToken != kTokenRParen) {
    for (;;) {
      // Parse the argument expression.
      AstNode* expression;
      Error err;

      if ((err = callNode->willAdd()) != kErrorOk || (err = parseExpression(&expression, true)) != kErrorOk) {
        _ast->deleteNode(callNode);
        return err;
      }

      callNode->appendNode(expression);

      // Parse ')' or ',' tokens.
      uToken = _tokenizer.peek(&token);
      if (uToken == kTokenRParen)
        break;

      if (uToken != kTokenComma) {
        _ast->deleteNode(callNode);
        MATHPRESSO_PARSER_ERROR(token, "Expected either ',' or ')' token.");
      }

      _tokenizer.consume();
    }
  }

  _tokenizer.consume();

  // Validate the number of function arguments.
  uint32_t n = callNode->size();
  uint32_t reqArgs = sym->funcArgs();

  if (n != reqArgs) {
    _ast->deleteNode(callNode);
    MATHPRESSO_PARSER_ERROR(token, "Function '%s' requires %u argument(s) (%u provided).", sym->name(), reqArgs, n);
  }

  // Transform an intrinsic function into unary or binary operator.
  if (sym->symbolType() == kAstSymbolIntrinsic) {
    const OpInfo& op = OpInfo::get(sym->opType());
    MATHPRESSO_ASSERT(n == op.opCount());

    AstNode* opNode;
    if (reqArgs == 1) {
      AstUnaryOp* unary = _ast->newNode<AstUnaryOp>(op.type);
      MATHPRESSO_NULLCHECK(unary);

      unary->setChild(callNode->removeAt(0));
      opNode = unary;
    }
    else {
      AstBinaryOp* binary = _ast->newNode<AstBinaryOp>(op.type);
      MATHPRESSO_NULLCHECK(binary);

      binary->setRight(callNode->removeAt(1));
      binary->setLeft(callNode->removeAt(0));
      opNode = binary;
    }

    opNode->setPosition(callNode->position());
    _ast->deleteNode(callNode);

    *pNodeOut = opNode;
    return kErrorOk;
  }
  else {
    *pNodeOut = callNode;
    return kErrorOk;
  }
}

} // mathpresso namespace
