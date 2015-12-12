MathPresso - Mathematical Expression Evaluator And Jit Compiler
===============================================================

Official Repository: https://github.com/kobalicek/mathpresso-ng

MathPresso is a C++ library designed to parse mathematical expressions and compile them into machine code. It's much faster than traditional AST or byte-code based evaluators, because there is basically no overhead in the expression's execution. The JIT compiler is based on AsmJit and works on X86 and X64 architectures.

This is an updated version of MathPresso that works with a new AsmJit library and uses double-precision floating points. It has many bugs fixed compared to the last version on google-code and contains improvements that can make execution of certain built-in functions faster if the host CPU supports SSE4.1 (rounding, modulo).

This version of MathPresso is currently not completely up-to-date with its forks called DoublePresso. Pull request that add functionality contained there and bugs fixes are welcome.

This is also a transitional version that is available to users that want to use MathPresso and cannot wait for the new MPSL engine, which is a work in progress.

Usage
=====

MathPresso's expression is always created around a `mathpresso::Context`, which defines an environment the expression can access and use. For example if you plan to extend MathPresso with your own function or constant the `Context` is the way to go. The `Context` also defines inputs and outputs of the expression that is shown in the example below:

```c++
#include "mathpresso/mathpresso.h"

int main(int argc, char* argv() {
  mathpresso::Context ctx;
  mathpresso::Expression exp;

  // Initialize the context by adding a basic environment MathPresso offers.
  // It adds E and PI constants and all built-in functions.
  ctx.addEnvironment(mathpresso::kMPEnvironmentAll);

  // Let the context know the name of the variables we will refer to and
  // their positions in the data pointer. We will use an array of 3 doubles,
  // so index them by using `sizeof(double)`, like a normal C array.
  ctx.addVariable("x", 0 * sizeof(double));
  ctx.addVariable("y", 1 * sizeof(double));
  ctx.addVariable("z", 2 * sizeof(double));

  // Compile some expression. The create parameters are:
  //   1. `mathpresso::Context&` - The expression's environment.
  //   2. `const char* exp` - The expression string.
  //   3. `uint` - Options, just pass `mathpresso::kMPOptionNone`.
  mathpresso::MPResult err = exp.create(ctx, "(x*y) % z", mathpresso::kMPOptionNone);
  if (err != mathpresso::kMPResultOk) {
    // Handle possible syntax or compilation error.
    printf("Expression Error: %u\n", err);
    return 1;
  }

  // To evaluate the expression you need to create the `data` to be passed
  // to the expression and initialize it. Every expression returns `double`,
  // to return more members simply use the passed `data`.
  double data[] = {
    1.2, // 'x' - available at data[0]
    3.8, // 'y' - available at data[1]
    1.3  // 'z' - available at data[2]
  };
  printf("Output: %f\n", exp.evaluate(data));

  return 0;
}
```

The example above should be self-explanatory. The next example does the same but by using a `struct` instead of an array to address the expression's data:

```c++
#include "mathpresso/mathpresso.h"

struct Data {
  inline Data(double x, double y, double z)
    : x(x), y(y), z(z) {}

  double x, y, z;
};

int main(int argc, char* argv() {
  mathpresso::Context ctx;
  mathpresso::Expression exp;

  ctx.addEnvironment(mathpresso::kMPEnvironmentAll);
  ctx.addVariable("x", MATHPRESSO_OFFSET(Data, x));
  ctx.addVariable("y", MATHPRESSO_OFFSET(Data, y));
  ctx.addVariable("z", MATHPRESSO_OFFSET(Data, z));

  mathpresso::MPResult err = exp.create(ctx, "(x*y) % z", mathpresso::kMPOptionNone);
  if (err != mathpresso::kMPResultOk) {
    printf("Expression Error: %u\n", err);
    return 1;
  }

  Data data(1.2, 3.8. 1.3);
  printf("Output: %f\n", exp.evaluate(&data));

  return 0;
}
```

Supported Operators
===================

The following arithmetic operators are supported:

  - Assigngment `x = y`
  - Addition `x + y`
  - Subtracion `x - y`
  - Multiplication `x * y`
  - Division `x / y`
  - Remainder `x % y`

The following comparision operators are supported:

  - Equal `x == y`
  - Not equal `x != y`
  - Greater than `x > y`
  - Greater or equal than `x >= y`
  - Lesser than `x < y`
  - Lesser or equal than `x <= y`

The following unary operators are supported:
  
  - Negate `-(x)`

The following functions are supported throught `addEnvironment()`:

  - Round `round(x)`
  - Floor `floor(x)`
  - Ceil `ceil(x)`
  - Absolute value `abs(x)`
  - Square root `sqrt(x)`
  - Reciprocal `recip(x)`
  - Exponential `exp(x)`
  - Logarithm `log(x)`
  - Logarithm of base 10 `log10(x)`
  - Sine `sin(x)`
  - Cosine `cos(x)`
  - Tangent `tan(x)`
  - Hyperbolic sine `sinh(x)`
  - Hyperbolic cosine `cosh(x)`
  - Hyperbolic tangent `tanh(x)`
  - Arc sine `asin(x)`
  - Arc cosine `acos(x)`
  - Arc tangent `atan(x)`
  - Arc tangent `atan2(x, y)`
  - Average value `avg(x, y)`
  - Minimum value `min(x, y)`
  - Maximum value `max(x, y)`
  - Power `pow(x, y)`

The following constants are supported throught `addEnvironment()`:

  - E `2.7182818284590452354`
  - PI `3.14159265358979323846`

Dependencies
============

AsmJit - 1.0 or later.

Contact
=======

Petr Kobalicek <kobalicek.petr@gmail.com>
(TODO: add here contact to people that maintain DoublePresso)
