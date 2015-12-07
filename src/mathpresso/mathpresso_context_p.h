// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _MATHPRESSO_CONTEXT_P_H
#define _MATHPRESSO_CONTEXT_P_H

#include "mathpresso.h"
#include "mathpresso_util_p.h"

namespace mathpresso {

struct ASTNode;

// ============================================================================
// [mathpresso::MFunc]
// ============================================================================

typedef double (*MFunc_Ret_D_ARG0)(void);
typedef double (*MFunc_Ret_D_ARG1)(double);
typedef double (*MFunc_Ret_D_ARG2)(double, double);
typedef double (*MFunc_Ret_D_ARG3)(double, double, double);
typedef double (*MFunc_Ret_D_ARG4)(double, double, double, double);
typedef double (*MFunc_Ret_D_ARG5)(double, double, double, double, double);
typedef double (*MFunc_Ret_D_ARG6)(double, double, double, double, double, double);
typedef double (*MFunc_Ret_D_ARG7)(double, double, double, double, double, double, double);
typedef double (*MFunc_Ret_D_ARG8)(double, double, double, double, double, double, double, double);

// ============================================================================
// [mathpresso::Function]
// ============================================================================

struct Function {
  inline Function(void* ptr, int prototype, int functionId) {
    this->ptr = ptr;
    this->prototype = prototype;
    this->functionId = functionId;
  }

  inline void* getPtr() const { return ptr; }
  inline int getPrototype() const { return prototype; }
  inline int getArguments() const { return prototype & 0xFF; }
  inline int getFunctionId() const { return functionId; }

  void* ptr;
  int prototype;
  int functionId;
};

// ============================================================================
// [mathpresso::Variable]
// ============================================================================

struct Variable {
  inline Variable(int type, double value) {
    this->type = type;
    this->c.value = value;
  }

  inline Variable(int type, int offset, int flags) {
    this->type = type;
    this->v.offset = offset;
    this->v.flags = flags;
  }

  int type;

  union {
    struct {
      double value;
    } c;

    struct {
      int offset;
      int flags;
    } v;
  };
};

// ============================================================================
// [mathpresso::ContextPrivate]
// ============================================================================

struct MATHPRESSO_NOAPI ContextPrivate {
  ContextPrivate();
  ~ContextPrivate();

  inline void addRef() { refCount.inc(); }
  inline void release() { if (refCount.dec()) delete this; }
  inline bool isDetached() { return refCount.get() == 1; }

  ContextPrivate* copy() const;

  MPAtomic refCount;
  Hash<Variable> variables;
  Hash<Function> functions;

private:
  // DISABLE COPY of ContextPrivate instance.
  ContextPrivate(const ContextPrivate& other);
  ContextPrivate& operator=(const ContextPrivate& other);
};

// ============================================================================
// [mathpresso::ExpressionPrivate]
// ============================================================================

struct ExpressionPrivate {
  inline ExpressionPrivate()
    : ast(NULL),
      ctx(NULL) {}

  inline ~ExpressionPrivate() {
    MP_ASSERT(ast == NULL);
    MP_ASSERT(ctx == NULL);
  }

  ASTNode* ast;
  ContextPrivate* ctx;
};

// ============================================================================
// [mathpresso::WorkContext]
// ============================================================================

//! \internal
//!
//! Simple class that is used to generate node IDs.
struct WorkContext {
  WorkContext(const Context& ctx);
  ~WorkContext();

  //! Get next id.
  inline uint genId() { return _id++; }

  //! Context data.
  ContextPrivate* _ctx;
  //! Current counter position.
  uint _id;
};

} // mathpresso namespace

#endif // _MATHPRESSO_CONTEXT_P_H
