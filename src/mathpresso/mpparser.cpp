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

// MathPresso - AstNestedScope
// ===========================

//! \internal
//!
//! Nested scope used only by the parser and always allocated statically.
struct AstNestedScope : public AstScope {
  MATHPRESSO_NONCOPYABLE(AstNestedScope)

  // Members
  // -------

  Parser* _parser;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE AstNestedScope(Parser* parser)
    : AstScope(parser->_ast, parser->_current_scope, kAstScopeNested),
      _parser(parser) {
    _parser->_current_scope = this;
  }

  MATHPRESSO_INLINE ~AstNestedScope() {
    AstScope* p = parent();
    MATHPRESSO_ASSERT(p != nullptr);

    _parser->_current_scope = p;
    p->_symbols.merge_to_invisible_slot(this->_symbols);
  }
};

// MathPresso - Parser Errors
// ==========================

#define MATHPRESSO_PARSER_ERROR(_Token_, ...) \
  return _error_reporter->on_error( \
    kErrorInvalidSyntax, static_cast<uint32_t>((size_t)(_Token_.position())), __VA_ARGS__)

#define MATHPRESSO_PARSER_WARNING(_Token_, ...) \
  _error_reporter->on_warning( \
    static_cast<uint32_t>((size_t)(_Token_.position())), __VA_ARGS__)

// MathPresso - Parser
// ===================

Error Parser::parse_program(AstProgram* block) {
  for (;;) {
    Token token;
    uint32_t uToken = _tokenizer.peek(&token);

    // Parse the end of the input.
    if (uToken == kTokenEnd)
      break;
    MATHPRESSO_PROPAGATE(parse_statement(block, kEnableVarDecls | kEnableNestedBlock));
  }

  if (block->size() == 0)
    return MATHPRESSO_TRACE_ERROR(kErrorNoExpression);

  return kErrorOk;
}

// Parse <statement>; or { [<statement>; ...] }
Error Parser::parse_statement(AstBlock* block, uint32_t flags) {
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

    MATHPRESSO_PROPAGATE(block->will_add());
    MATHPRESSO_NULLCHECK(nested = _ast->new_node<AstBlock>());
    block->append_node(nested);

    AstNestedScope tmpScope(this);
    return parse_block_or_statement(nested);
  }

  // Parse a variable declaration.
  if (uToken == kTokenVar) {
    if (!(flags & kEnableVarDecls))
      MATHPRESSO_PARSER_ERROR(token, "Cannot declare a new variable here.");
    return parse_variable_decl(block);
  }

  // Parse an expression.
  AstNode* expression;

  MATHPRESSO_PROPAGATE(block->will_add());
  MATHPRESSO_PROPAGATE(parse_expression(&expression, false));
  block->append_node(expression);

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
Error Parser::parse_block_or_statement(AstBlock* block) {
  Token token;
  uint32_t uToken = _tokenizer.next(&token);

  // Parse the <block>, consume '{' token.
  block->set_position(token.positionAsUInt());
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
        MATHPRESSO_PROPAGATE(parse_statement(block, kEnableVarDecls | kEnableNestedBlock));
      }
    }
  }
  else {
    return parse_statement(block, kNoFlags);
  }
}

// Parse "var <name> = <expression>[, <name> = <expression>, ...];".
Error Parser::parse_variable_decl(AstBlock* block) {
  Token token;
  uint32_t uToken = _tokenizer.next(&token);
  StringRef str;

  bool is_first = true;
  uint32_t position = token.positionAsUInt();

  // Parse the 'var' keyword.
  if (uToken != kTokenVar)
    MATHPRESSO_PARSER_ERROR(token, "Expected 'var' keyword.");

  AstScope* scope = _current_scope;
  for (;;) {
    // Parse the variable name.
    if (_tokenizer.next(&token) != kTokenSymbol)
      MATHPRESSO_PARSER_ERROR(token, is_first
        ? "Expected a variable name after 'var' keyword."
        : "Expected a variable name after colon ','.");

    MATHPRESSO_PROPAGATE(block->will_add());

    if (!is_first)
      position = token.positionAsUInt();

    // Resolve the variable name.
    AstSymbol* vSym;
    AstScope* vScope;

    str.set(_tokenizer._start + token.position(), token.size());
    if ((vSym = scope->resolve_symbol(str, token.hash_code(), &vScope)) != nullptr) {
      if (vSym->symbol_type() != kAstSymbolVariable || scope == vScope)
        MATHPRESSO_PARSER_ERROR(token, "Attempt to redefine '%s'.", vSym->name());

      if (vSym->has_node()) {
        uint32_t line, column;
        _error_reporter->get_line_and_column(vSym->node()->position(), line, column);
        MATHPRESSO_PARSER_WARNING(token, "Variable '%s' shadows a variable declared at [%d:%d].", vSym->name(), line, column);
      }
      else {
        MATHPRESSO_PARSER_WARNING(token, "Variable '%s' shadows a variable of the same name.", vSym->name());
      }
    }

    vSym = _ast->new_symbol(str, token.hash_code(), kAstSymbolVariable, scope->scope_type());
    MATHPRESSO_NULLCHECK(vSym);
    scope->put_symbol(vSym);

    AstVarDecl* decl = _ast->new_node<AstVarDecl>();
    MATHPRESSO_NULLCHECK_(decl, { _ast->delete_symbol(vSym); });

    decl->set_position(position);
    decl->set_symbol(vSym);

    // Assign a slot and fill to safe defaults.
    vSym->set_var_offset(0);
    vSym->set_var_slot_id(_ast->new_slot_id());
    vSym->set_node(decl);

    // Parse possible assignment '='.
    uToken = _tokenizer.next(&token);
    bool is_assigned = (uToken == kTokenAssign);

    if (is_assigned) {
      AstNode* expression;
      MATHPRESSO_PROPAGATE_(parse_expression(&expression, false), { _ast->delete_node(decl); });

      decl->set_child(expression);
      vSym->increment_write_count();

      uToken = _tokenizer.next(&token);
    }

    // Make the symbol declared so it can be referenced after now.
    vSym->mark_declared();

    // Parse the ',' or ';' tokens.
    if (uToken == kTokenComma || uToken == kTokenSemicolon || uToken == kTokenEnd) {
      block->append_node(decl);

      // Token ';' terminates the declaration.
      if (uToken != kTokenComma)
        break;
    }
    else {
      _ast->delete_symbol(vSym);
      MATHPRESSO_PARSER_ERROR(token, "Unexpected token.");
    }

    is_first = false;
  }

  return kErrorOk;
}

Error Parser::parse_expression(AstNode** pNode, bool isNested) {
  AstScope* scope = _current_scope;

  // It's important that the given expression is parsed in a way that it can be
  // correctly evaluated. The `parse_expression()` function can handle expressions
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

  // Current binary operator node. Initial nullptr value means that the parsing
  // just started and there is no binary operator yet. Once the first binary
  // operator has been parsed `oNode` will be set accordingly.
  AstBinaryOp* oNode = nullptr;

  // Currently parsed node.
  AstNode* tNode = nullptr;

  for (;;) {
    // Last unary node. It's an optimization to prevent recursion in case that
    // we found two or more unary expressions after each other. For example the
    // expression "-!-1" contains only unary operators that will be parsed by
    // a single `parse_expression()` call.
    AstUnary* unary = nullptr;

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
        AstSymbol* sym = scope->resolve_symbol(str, token.hash_code(), &symScope);

        if (sym == nullptr)
          MATHPRESSO_PARSER_ERROR(token, "Unresolved symbol %.*s.", static_cast<int>(str.size()), str.data());

        uint32_t symType = sym->symbol_type();
        AstNode* zNode;

        if (symType == kAstSymbolVariable) {
          if (!sym->is_declared())
            MATHPRESSO_PARSER_ERROR(token, "Can't use variable '%s' that is being declared.", sym->name());

          // Put symbol to shadow scope if it's global. This is done lazily and
          // only once per symbol when it's referenced.
          if (symScope->is_global()) {
            sym = _ast->shadow_symbol(sym);
            MATHPRESSO_NULLCHECK(sym);

            sym->set_var_slot_id(_ast->new_slot_id());
            symScope = _ast->root_scope();
            symScope->put_symbol(sym);
          }

          zNode = _ast->new_node<AstVar>();
          MATHPRESSO_NULLCHECK(zNode);
          static_cast<AstVar*>(zNode)->set_symbol(sym);

          zNode->set_position(token.positionAsUInt());
          sym->increment_used_count();
        }
        else {
          // Will be parsed by `parse_call()` again.
          _tokenizer.set(&token);
          MATHPRESSO_PROPAGATE(parse_call(&zNode));
        }

        if (unary == nullptr)
          tNode = zNode;
        else
          unary->set_child(zNode);
        break;
      }

      // Parse a number.
      case kTokenNumber: {
        AstImm* zNode = _ast->new_node<AstImm>();
        MATHPRESSO_NULLCHECK(zNode);

        zNode->set_position(token.positionAsUInt());
        zNode->_value = token.value();

        if (unary == nullptr)
          tNode = zNode;
        else
          unary->set_child(zNode);
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
        MATHPRESSO_PROPAGATE(parse_expression(&zNode, true));

        if (_tokenizer.next(&token) != kTokenRParen)
          MATHPRESSO_PARSER_ERROR(token, "Expected a ')' token.");

        if (unary == nullptr)
          tNode = zNode;
        else
          unary->set_child(zNode);

        break;
      }

      // Parse a right-to-left associative unary operator ('+', '-', "!").
      case kTokenAdd         : op = kOpNone  ; goto _Unary;
      case kTokenSub         : op = kOpNeg   ; goto _Unary;
      case kTokenNot         : op = kOpNot   ; goto _Unary;
_Unary: {
        // Parse the unary operator.
        AstUnaryOp* opNode = _ast->new_node<AstUnaryOp>(op);
        MATHPRESSO_NULLCHECK(opNode);
        opNode->set_position(token.positionAsUInt());

        if (unary == nullptr)
          tNode = opNode;
        else
          unary->set_child(opNode);

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

        if (oNode != nullptr) {
          oNode->set_right(tNode);
          // Iterate to the top-most node.
          while (oNode->has_parent())
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
        if (tNode->node_type() != kAstNodeVar)
          MATHPRESSO_PARSER_ERROR(token, "Can't assign to a non-variable.");

        AstSymbol* sym = static_cast<AstVar*>(tNode)->symbol();
        if (sym->has_symbol_flag(kAstSymbolIsReadOnly))
          MATHPRESSO_PARSER_ERROR(token, "Can't assign to a read-only variable '%s'.", sym->name());

        if (isNested)
          MATHPRESSO_PARSER_ERROR(token, "Invalid assignment inside an expression.");

        sym->increment_write_count();
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
        AstBinaryOp* zNode = _ast->new_node<AstBinaryOp>(op);
        MATHPRESSO_NULLCHECK(zNode);
        zNode->set_position(token.positionAsUInt());

        if (oNode == nullptr) {
          // oNode <------+
          //              |
          // +------------+------------+ First operand - oNode becomes the newly
          // |         (zNode)         | created zNode; tNode is assigned to the
          // |        /       \        | left side of zNode and will be referred
          // |     (tNode)  (nullptr)     | as (...) by the next operation.
          // +-------------------------+
          zNode->set_left(tNode);
          oNode = zNode;
          break;
        }

        uint32_t oPrec = OpInfo::get(oNode->op_type()).precedence;
        uint32_t zPrec = OpInfo::get(op).precedence;

        if (oPrec > zPrec) {
          // oNode <----------+
          //                  |
          // +----------------+--------+ The current operator (zPrec) has a
          // |     (oNode)    |        | higher precedence than the previous one
          // |    /       \   |        | (oPrec), so the zNode will be assigned
          // | (...)       (zNode)     | to the right side of oNode and it will
          // |            /       \    | function as a stack-like structure. We
          // |         (tNode)  (nullptr) | have to advance back at some point.
          // +-------------------------+
          oNode->set_right(zNode);
          zNode->set_left(tNode);
          oNode = zNode;
          break;
        }
        else {
          oNode->set_right(tNode);

          // Advance to the top-most oNode that has less or equal precedence
          // than zPrec.
          while (oNode->has_parent()) {
            // Terminate conditions:
            //   1. oNode has higher precedence than zNode.
            //   2. oNode has equal precedence and right-to-left associativity.
            if (OpInfo::get(oNode->op_type()).right_associate(zPrec))
              break;
            oNode = static_cast<AstBinaryOp*>(oNode->parent());
          }

          // oNode <------+
          //              |
          // +------------+------------+
          // |         (zNode)         | Simple case - oNode becomes the left
          // |        /       \        | node in the created zNode and zNode
          // |     (oNode)  (nullptr)     | becomes oNode for the next operator.
          // |    /       \            |
          // | (...)    (tNode)        | oNode will become a top-level node.
          // +-------------------------+
          if (!oNode->has_parent() && !OpInfo::get(oNode->op_type()).right_associate(zPrec)) {
            zNode->set_left(oNode);
          }
          // oNode <----------+
          //                  |
          // +----------------+--------+
          // |     (oNode)    |        |
          // |    /       \   |        | Complex case - inject node in place
          // | (...)       (zNode)     | of oNode.right (because of higher
          // |            /       \    | precedence or RTL associativity).
          // |        (tNode)   (nullptr) |
          // +-------------------------+
          else {
            AstNode* pNode = oNode->unlink_right();
            oNode->set_right(zNode);
            zNode->set_left(pNode);
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
Error Parser::parse_call(AstNode** pNodeOut) {
  Token token;
  uint32_t uToken;

  uToken = _tokenizer.next(&token);
  MATHPRESSO_ASSERT(uToken == kTokenSymbol);
  uint32_t position = token.positionAsUInt();

  StringRef str(_tokenizer._start + token.position(), token.size());
  AstSymbol* sym = _current_scope->resolve_symbol(str, token.hash_code());

  if (sym == nullptr)
    MATHPRESSO_PARSER_ERROR(token, "Unresolved symbol %.*s.", static_cast<int>(str.size()), str.data());

  if (sym->symbol_type() != kAstSymbolIntrinsic && sym->symbol_type() != kAstSymbolFunction)
    MATHPRESSO_PARSER_ERROR(token, "Expected a function name.");

  uToken = _tokenizer.next(&token);
  if (uToken != kTokenLParen)
    MATHPRESSO_PARSER_ERROR(token, "Expected a '(' token after a function name.");

  AstCall* call_node = _ast->new_node<AstCall>();
  MATHPRESSO_NULLCHECK(call_node);

  call_node->set_symbol(sym);
  call_node->set_position(position);

  uToken = _tokenizer.peek(&token);
  if (uToken != kTokenRParen) {
    for (;;) {
      // Parse the argument expression.
      AstNode* expression;
      Error err;

      if ((err = call_node->will_add()) != kErrorOk || (err = parse_expression(&expression, true)) != kErrorOk) {
        _ast->delete_node(call_node);
        return err;
      }

      call_node->append_node(expression);

      // Parse ')' or ',' tokens.
      uToken = _tokenizer.peek(&token);
      if (uToken == kTokenRParen)
        break;

      if (uToken != kTokenComma) {
        _ast->delete_node(call_node);
        MATHPRESSO_PARSER_ERROR(token, "Expected either ',' or ')' token.");
      }

      _tokenizer.consume();
    }
  }

  _tokenizer.consume();

  // Validate the number of function arguments.
  uint32_t n = call_node->size();
  uint32_t reqArgs = sym->func_args();

  if (n != reqArgs) {
    _ast->delete_node(call_node);
    MATHPRESSO_PARSER_ERROR(token, "Function '%s' requires %u argument(s) (%u provided).", sym->name(), reqArgs, n);
  }

  // Transform an intrinsic function into unary or binary operator.
  if (sym->symbol_type() == kAstSymbolIntrinsic) {
    const OpInfo& op = OpInfo::get(sym->op_type());
    MATHPRESSO_ASSERT(n == op.op_count());

    AstNode* opNode;
    if (reqArgs == 1) {
      AstUnaryOp* unary = _ast->new_node<AstUnaryOp>(op.type);
      MATHPRESSO_NULLCHECK(unary);

      unary->set_child(call_node->remove_at(0));
      opNode = unary;
    }
    else {
      AstBinaryOp* binary = _ast->new_node<AstBinaryOp>(op.type);
      MATHPRESSO_NULLCHECK(binary);

      binary->set_right(call_node->remove_at(1));
      binary->set_left(call_node->remove_at(0));
      opNode = binary;
    }

    opNode->set_position(call_node->position());
    _ast->delete_node(call_node);

    *pNodeOut = opNode;
    return kErrorOk;
  }
  else {
    *pNodeOut = call_node;
    return kErrorOk;
  }
}

} // {mathpresso}
