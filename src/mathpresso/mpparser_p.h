// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MATHPRESSO_MPPARSER_P_H
#define _MATHPRESSO_MPPARSER_P_H

// [Dependencies]
#include "./mpast_p.h"
#include "./mptokenizer_p.h"

namespace mathpresso {

struct AstBuilder;
struct AstBlock;
struct AstNode;
struct AstProgram;
struct AstVar;

// MathPresso - Parser
// ===================

struct Parser {
  MATHPRESSO_NONCOPYABLE(Parser)

  enum Flags {
    kNoFlags = 0x00,
    kEnableVarDecls = 0x01,
    kEnableNestedBlock = 0x02
  };

  // Members
  // -------

  AstBuilder* _ast;
  ErrorReporter* _error_reporter;

  AstScope* _current_scope;
  Tokenizer _tokenizer;

  // Construction & Destruction
  // --------------------------

  MATHPRESSO_INLINE Parser(AstBuilder* ast, ErrorReporter* error_reporter, const char* body, size_t size)
    : _ast(ast),
      _error_reporter(error_reporter),
      _current_scope(ast->root_scope()),
      _tokenizer(body, size) {}
  MATHPRESSO_INLINE ~Parser() {}

  // Accessors
  // ---------

  MATHPRESSO_INLINE AstScope* current_scope() const { return _current_scope; }

  // Parse
  // -----

  MATHPRESSO_NOAPI Error parse_program(AstProgram* block);

  MATHPRESSO_NOAPI Error parse_statement(AstBlock* block, uint32_t flags);
  MATHPRESSO_NOAPI Error parse_block_or_statement(AstBlock* block);

  MATHPRESSO_NOAPI Error parse_variable_decl(AstBlock* block);
  MATHPRESSO_NOAPI Error parse_expression(AstNode** pNodeOut, bool isNested);
  MATHPRESSO_NOAPI Error parse_call(AstNode** pNodeOut);
};

} // {mathpresso}

// [Guard]
#endif // _MATHPRESSO_MPPARSER_P_H
