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
#include "mathpresso_parser_p.h"
#include "mathpresso_tokenizer_p.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

// ============================================================================
// [Constants]
// ============================================================================

//! \internal
//!
//! Operator priority.
static const int mpOperatorPriority[] = {
  0 , // kMPBinaryOpNone
  5 , // kMPBinaryOpAssign
  10, // kMPBinaryOpAdd
  10, // kMPBinaryOpSub
  15, // kMPBinaryOpMul
  15, // kMPBinaryOpDiv
  15  // kMPBinaryOpMod
};

MPParser::MPParser(WorkContext& ctx, const char* input, size_t length)
  : _ctx(ctx),
    _tokenizer(input, length) {}
MPParser::~MPParser() {}

MPResult MPParser::parse(ASTNode** dst) {
  return parseTree(dst);
}

MPResult MPParser::parseTree(ASTNode** dst) {
  Vector<ASTNode*> nodes;
  MPResult result = kMPResultOk;

  for (;;)
  {
    ASTNode* ast = NULL;
    if ((result = parseExpression(&ast, NULL, 0, false)) != kMPResultOk)
      goto failed;
    if (ast) nodes.append(ast);

    MP_ASSERT(_last.tokenType != kMPTokenError);
    switch (_last.tokenType)
    {
      case kMPTokenEOI:
        goto finished;
      case kMPTokenComma:
      case kMPTokenRParen:
        result = kMPResultUnexpectedToken;
        goto failed;
      case kMPTokenSemicolon:
        // Skip semicolon and parse another expression.
        _tokenizer.next(&_last);
        break;
    }
  }

finished:
  if (nodes.getLength() == 0) {
    return kMPResultNoExpression;
  }
  else if (nodes.getLength() == 1) {
    *dst = nodes[0];
    return kMPResultOk;
  }
  else {
    ASTBlock* block = new ASTBlock(_ctx.genId());
    block->_nodes.swap(nodes);

    *dst = block;
    return kMPResultOk;
  }

failed:
  for (size_t i = 0; i < nodes.getLength(); i++)
    delete nodes[i];
  return result;
}

MPResult MPParser::parseExpression(ASTNode** dst,
  ASTNode* _left,
  int minPriority,
  bool isInsideExpression) {

  ASTNode* left = _left;
  ASTNode* right = NULL;

  MPResult result = kMPResultOk;
  uint op = kMPBinaryOpNone;
  uint om = kMPBinaryOpNone;

  MPToken& token = _last;
  MPToken helper;

  for (;;) {
    _tokenizer.next(&token);

    switch (token.tokenType) {
      // ----------------------------------------------------------------------
      case kMPTokenError:
        _tokenizer.back(&token);
        result = kMPResultInvalidToken;
        goto failure;
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      case kMPTokenComma:
      case kMPTokenSemicolon:
        if (left == NULL) {
          return (isInsideExpression)
            ? kMPResultUnexpectedToken
            : kMPResultOk;
        }
        // ... fall through ...
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      case kMPTokenEOI:
        if (op != kMPBinaryOpNone) {
          // Expecting expression.
          result = kMPResultExpectedExpression;
          goto failure;
        }

        _tokenizer.back(&token);
        *dst = left;
        return kMPResultOk;
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      case kMPTokenInt:
      case kMPTokenDouble:
        right = new ASTImmediate(_ctx.genId(),
          om == kMPBinaryOpSub ? -token.value : token.value);
        om = kMPBinaryOpNone;
        break;
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      case kMPTokenLParen:
        result = parseExpression(&right, NULL, 0, true);
        if (result != kMPResultOk) goto failure;

        _tokenizer.next(&token);
        if (token.tokenType != kMPTokenRParen) {
          result = kMPResultUnexpectedToken;
          goto failure;
        }

        if (om == kMPBinaryOpSub) {
          ASTUnaryOp* transform = new ASTUnaryOp(_ctx.genId());
          transform->setUnaryType(kMPUnaryOpNegate);
          transform->setChild(right);
          right = transform;
        }

        om = kMPBinaryOpNone;
        break;
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      case kMPTokenRParen:
        if (op != kMPBinaryOpNone || om != kMPBinaryOpNone) {
          result = kMPResultUnexpectedToken;
          goto failure;
        }

        if (left == NULL && isInsideExpression) {
          result = kMPResultUnexpectedToken;
          goto failure;
        }

        _tokenizer.back(&token);
        *dst = left;
        return kMPResultOk;
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      case kMPTokenOperator:
        if (token.operatorType == kMPBinaryOpAssign) {
          // Assignment inside expression is not allowed.
          if (isInsideExpression) {
            result = kMPResultAssignmentInsideExpression;
            goto failure;
          }

          // Must be assigned to an variable.
          if (left == NULL || left->getNodeType() != kMPNodeVariable) {
            result = kMPResultAssignmentToNonVariable;
            goto failure;
          }
        }

        if (op == kMPBinaryOpNone && left == NULL) {
          om = token.operatorType;
          if (om == kMPBinaryOpAdd || om == kMPBinaryOpSub) continue;

          result = kMPResultUnexpectedToken;
          goto failure;
        }

        op = token.operatorType;
        if (mpOperatorPriority[op] < minPriority) {
          _tokenizer.back(&token);

          *dst = left;
          return kMPResultOk;
        }
        continue;
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      case kMPTokenSymbol: {
        const char* symbolName = _tokenizer.beg + token.pos;
        size_t symbolLength = token.len;

        MPToken ttoken;
        bool isFunction = (_tokenizer.peek(&ttoken) == kMPTokenLParen);

        // Parse function.
        if (isFunction) {
          Function* function = _ctx._ctx->functions.get(symbolName, symbolLength);
          if (function == NULL) {
            result = kMPResultNoSymbol;
            goto failure;
          }

          // Parse LPAREN token again.
          _tokenizer.next(&ttoken);

          Vector<ASTNode*> arguments;
          bool first = true;

          // Parse function arguments.
          for (;;) {
            _tokenizer.next(&ttoken);

            if (ttoken.tokenType == kMPTokenError) {
              mpDeleteAll(arguments);
              result = kMPResultInvalidToken;
              goto failure;
            }

            // ')' - End of function call.
            if (ttoken.tokenType == kMPTokenRParen) {
              break;
            }

            // ',' - Arguments delimiter for non-first argument.
            if (!first) {
              if (ttoken.tokenType != kMPTokenComma) {
                mpDeleteAll(arguments);
                result = kMPResultExpectedExpression;
                goto failure;
              }
            }
            else {
              _tokenizer.back(&ttoken);
            }

            // Expression.
            ASTNode* arg;
            if ((result = parseExpression(&arg, NULL, 0, true)) != kMPResultOk) {
              mpDeleteAll(arguments);
              goto failure;
            }

            arguments.append(arg);
            first = false;
          }

          // Validate function arguments.
          if (function->getArguments() != arguments.getLength()) {
            mpDeleteAll(arguments);
            result = kMPResultArgumentsMismatch;
            goto failure;
          }

          // Done.
          ASTCall* call = new ASTCall(_ctx.genId(), function);
          call->swapArguments(arguments);
          right = call;
        }
        // Parse variable.
        else {
          Variable* var = _ctx._ctx->variables.get(symbolName, symbolLength);
          if (var == NULL) {
            result = kMPResultNoSymbol;
            goto failure;
          }

          if (var->type == kMPVariableTypeConst)
            right = new ASTImmediate(_ctx.genId(), var->c.value);
          else
            right = new ASTVariable(_ctx.genId(), var);
        }

        if (om == kMPBinaryOpSub) {
          ASTUnaryOp* transform = new ASTUnaryOp(_ctx.genId());
          transform->setUnaryType(kMPUnaryOpNegate);
          transform->setChild(right);
          right = transform;
        }

        om = kMPBinaryOpNone;
        break;
      }
      // ----------------------------------------------------------------------

      // ----------------------------------------------------------------------
      default:
        MP_ASSERT_NOT_REACHED();
      // ----------------------------------------------------------------------
    }

    if (left) {
      _tokenizer.peek(&helper);

      if (helper.tokenType == kMPTokenOperator && mpOperatorPriority[op] < mpOperatorPriority[helper.operatorType]) {
        result = parseExpression(&right, right, mpOperatorPriority[helper.operatorType], true);
        if (result != kMPResultOk) { right = NULL; goto failure; }
      }

      ASTBinaryOp* parent = new ASTBinaryOp(_ctx.genId(), op);
      parent->setLeft(left);
      parent->setRight(right);

      left = parent;
      right = NULL;
      op = kMPBinaryOpNone;
    }
    else {
      left = right;
      right = NULL;
    }
  }

failure:
  if (left) delete left;
  if (right) delete right;

  *dst = NULL;
  return result;
}

} // mathpresso namespace
