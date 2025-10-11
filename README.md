MathPresso
==========

Mathematical Expression Parser And JIT Compiler.

  * [Official Repository (kobalicek/mathpresso)](https://github.com/kobalicek/mathpresso)
  * [Permissive Zlib license](./LICENSE.md)


Introduction
------------

MathPresso is a C++ library designed to parse mathematical expressions and compile them into machine code. It's much faster than traditional AST or byte-code based evaluators, because there is basically no overhead in the expression's execution. The JIT compiler is based on AsmJit and works on X86 and X64 architectures.


Features
--------

  * Unary operators:
    * Negate `-(x)`
    * Not `!(x)`
  * Arithmetic operators:
    * Assignment `x = y`
    * Addition `x + y`
    * Subtraction `x - y`
    * Multiplication `x * y`
    * Division `x / y`
    * Modulo `x % y`
  * Comparison operators:
    * Equal `x == y`
    * Not equal `x != y`
    * Greater `x > y`
    * Greater or equal `x >= y`
    * Lesser `x < y`
    * Lesser or equal `x <= y`
  * Functions defined by `add_builtins()`:
    * Check for NaN `is_nan(x)`
    * Check for infinity `is_inf(x)`
    * Check for finite number `is_finite(x)`
    * Get a sign bit `sign_bit(x)`
    * Copy sign `copy_sign(x, y)`
    * Truncate `trunc(x)`
    * Floor `floor(x)`
    * Ceil `ceil(x)`
    * Round to even `round_even(x)`
    * Round half up `round_half_up(x)`
    * Round half away `round_half_away(x)`
    * Average value `avg(x, y)`
    * Minimum value `min(x, y)`
    * Maximum value `max(x, y)`
    * Absolute value `abs(x)`
    * Exponential `exp(x)`
    * Logarithm `log(x)`
    * Logarithm of base 2 `log2(x)`
    * Logarithm of base 10 `log10(x)`
    * Square root `sqrt(x)`
    * Fraction `frac(x)`
    * Reciprocal `recip(x)`
    * Power `pow(x, y)`
    * Sine `sin(x)`
    * Cosine `cos(x)`
    * Tangent `tan(x)`
    * Hyperbolic sine `sinh(x)`
    * Hyperbolic cosine `cosh(x)`
    * Hyperbolic tangent `tanh(x)`
    * Arcsine `asin(x)`
    * Arccosine `acos(x)`
    * Arctangent `atan(x)`
    * Arctangent `atan2(x, y)`
  * Constants defined by `add_builtins()`:
    * Infinity `INF`
    * Not a Number `NaN`
    * Euler's constant `E = 2.7182818284590452354`
    * PI `PI = 3.14159265358979323846`


Usage
-----

MathPresso's expression is always created around a `mathpresso::Context`, which defines an environment the expression can access and use. For example if you plan to extend MathPresso with your own function or constant the `Context` is the way to go. The `Context` also defines inputs and outputs of the expression as shown in the example below:

```c++
#include <mathpresso/mathpresso.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  mathpresso::Context ctx;
  mathpresso::Expression exp;

  // Initialize the context by adding MathPresso built-ins. Without this line
  // functions like round(), sin(), etc won't be available.
  ctx.add_builtins();

  // Let the context know the name of the variables we will refer to and
  // their positions in the data pointer. We will use an array of 3 doubles,
  // so index them by using `sizeof(double)`, like a normal C array.
  //
  // The `add_variable()` also contains a third parameter that describes
  // variable flags, use `kVariableRO` to make a certain variable read-only.
  ctx.add_variable("x", 0 * sizeof(double));
  ctx.add_variable("y", 1 * sizeof(double));
  ctx.add_variable("z", 2 * sizeof(double));

  // Compile the expression.
  //
  // The create parameters are:
  //   1. `mathpresso::Context&` - The expression's context / environment.
  //   2. `const char* body` - The expression body.
  //   3. `unsigned int` - Options, just pass `mathpresso::kNoOptions`.
  mathpresso::Error err = exp.compile(ctx, "(x*y) % z", mathpresso::kNoOptions);

  // Handle possible syntax or compilation error.
  if (err != mathpresso::kErrorOk) {
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
#include <mathpresso/mathpresso.h>
#include <stdio.h>

struct Data {
  inline Data(double x, double y, double z)
    : x(x), y(y), z(z) {}

  double x, y, z;
};

int main(int argc, char* argv[]) {
  mathpresso::Context ctx;
  mathpresso::Expression exp;

  ctx.add_builtins();
  ctx.add_variable("x", MATHPRESSO_OFFSET(Data, x));
  ctx.add_variable("y", MATHPRESSO_OFFSET(Data, y));
  ctx.add_variable("z", MATHPRESSO_OFFSET(Data, z));

  mathpresso::Error err = exp.compile(ctx, "(x*y) % z", mathpresso::kNoOptions);
  if (err != mathpresso::kErrorOk) {
    printf("Expression Error: %u\n", err);
    return 1;
  }

  Data data(1.2, 3.8. 1.3);
  printf("Output: %f\n", exp.evaluate(&data));

  return 0;
}
```


Error Handling
--------------

MathPresso allows to attach an `OutputLog` instance to retrieve a human readable error message in case of error. It can output the following:

  - Errors, only one as MathPresso stops after the first error
  - Warnings
  - Abstract syntax tree (AST)
  - Assembly (ASM)

Here is the minimum working example that uses `OutputLog` to display errors. The interface is very simple, but extensible.

```c++
// This is a minimum working example that uses most of MathPresso features. It
// shows how to compile and evaluate expressions and how to handle errors. It
// also shows how to print the generated AST and machine code.
#include <mathpresso/mathpresso.h>
#include <stdio.h>

// The data passed to the expression.
struct Data {
  double x, y, z;
};

// By inheriting `OutputLog` one can create a way how to handle possible errors
// and report them to humans. The most interesting and used message type is
// `kMessageError`, because it signalizes an invalid expression. Other message
// types are used mostly for debugging.
struct MyOutputLog : public mathpresso::OutputLog {
  MyOutputLog() {}
  virtual ~MyOutputLog() {}
  virtual void log(unsigned int type, unsigned int line, unsigned int column, const char* message, size_t size) {
    switch (type) {
      case kMessageError:
        printf("[ERROR]: %s (line %u, column %u)\n", message, line, column);
        break;

      case kMessageWarning:
        printf("[WARNING]: %s (line %u, column %u)\n", message, line, column);
        break;

      case kMessageAstInitial:
        printf("[AST-INITIAL]\n%s", message);
        break;

      case kMessageAstFinal:
        printf("[AST-FINAL]\n%s", message);
        break;

      case kMessageAsm:
        printf("[ASSEMBLY]\n%s", message);
        break;

      default:
        printf("[UNKNOWN]\n%s", message);
        break;
    }
  }
};

int main(int argc, char* argv[]) {
  MyOutputLog output_log;

  // Create the context, add builtins and define the `Data` layout.
  mathpresso::Context ctx;
  ctx.add_builtins();
  ctx.add_variable("x"  , MATHPRESSO_OFFSET(Data, x));
  ctx.add_variable("y"  , MATHPRESSO_OFFSET(Data, y));
  ctx.add_variable("z"  , MATHPRESSO_OFFSET(Data, z));

  // The following options will cause that MathPresso will send everything
  // it does to `OutputLog`.
  unsigned int options =
    mathpresso::kOptionVerbose         | // Enable warnings, not just errors.
    mathpresso::kOptionDebugAst        | // Enable AST dumps.
    mathpresso::kOptionDebugMachineCode; // Enable ASM dumps.

  mathpresso::Expression exp;
  mathpresso::Error err = exp.compile(ctx,
    "-(-(abs(x * y - floor(x)))) * z * (12.9 - 3)", options, &output_log);

  // Handle possible syntax or compilation error. The OutputLog has already
  // received and printed the reason in a human readable form.
  if (err) {
    printf("ERROR %u\n", err);
    return 1;
  }

  // Evaluate the expression, if compiled.
  Data data = { 12.2, 9.2, -1.9 };
  double result = exp.evaluate(&data);

  printf("RESULT: %f\n", result);
  return 0;
}
```

When executed the output of the application would be something like:

```
[AST-INITIAL]
* [Binary]
  * [Binary]
    - [Unary]
      - [Unary]
        abs [Unary]
          - [Binary]
            * [Binary]
              x
              y
            floor [Unary]
              x
    z
  - [Binary]
    12.900000
    3.000000

[AST-FINAL]
* [Binary]
  * [Binary]
    abs [Unary]
      - [Binary]
        * [Binary]
          x
          y
        floor [Unary]
          x
    z
  9.900000

[ASSEMBLY]
L0:                                 ;                     |                           ..
lea rax, [L2]                       ; 488D05........      | lea pConst, [L2]          ..w
movsd xmm0, [rdx]                   ; F20F1002            | movsd v3, [pVariables]    .r.w
mulsd xmm0, [rdx+8]                 ; F20F594208          | mulsd v3, [pVariables+8]  .r.x
movsd xmm1, [rdx]                   ; F20F100A            | movsd v4, [pVariables]    .r..w
roundsd xmm1, xmm1, 9               ; 660F3A0BC909        | roundsd v4, v4, 9         ....x
subsd xmm0, xmm1                    ; F20F5CC1            | subsd v3, v4              ...xR
xorpd xmm1, xmm1                    ; 660F57C9            | xorpd v5, v5              .... w
subsd xmm1, xmm0                    ; F20F5CC8            | subsd v5, v3              ...r x
maxsd xmm1, xmm0                    ; F20F5FC8            | maxsd v5, v3              ...R x
mulsd xmm1, [rdx+16]                ; F20F594A10          | mulsd v5, [pVariables+16] .R.  x
mulsd xmm1, [rax]                   ; F20F5908            | mulsd v5, [pConst]        . R  x
movsd [rcx], xmm1                   ; F20F1109            | movsd [pResult], v5       R    R
L1:                                 ;                     |
ret                                 ; C3                  |
.align 8
L2:                                 ;                     |
.data CDCCCCCCCCCC2340

RESULT: -1885.514400
```


Build Dependencies
------------------

  * [AsmJit](https://asmjit.com)


Support
-------

MathPresso is an open-source library released under a permissive ZLIB license, which makes it possible to use it freely in any open-source or commercial product. Free support is available through issues and gitter channel.


Authors & Maintainers
---------------------

  * Petr Kobalicek <kobalicek.petr@gmail.com>
