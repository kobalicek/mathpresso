// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// This is a minimum working example that uses most of MathPresso features. It
// shows how to compile and evaluate expressions and how to handle errors. It
// also shows how to print the generated AST and machine code.
#include "../src/mathpresso/mathpresso.h"

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
  virtual void log(unsigned int type, unsigned int line, unsigned int column, const char* message, size_t size) {
    (void)size;

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

int main() {
  MyOutputLog outputLog;

  // Create the context, add builtins and define the `Data` layout.
  mathpresso::Context ctx;
  ctx.add_builtins();
  ctx.add_variable("x", MATHPRESSO_OFFSET(Data, x));
  ctx.add_variable("y", MATHPRESSO_OFFSET(Data, y));
  ctx.add_variable("z", MATHPRESSO_OFFSET(Data, z));

  // The following options will cause that MathPresso will send everything
  // it does to `OutputLog`.
  unsigned int options =
    mathpresso::kOptionVerbose         | // Enable warnings, not just errors.
    mathpresso::kOptionDebugAst        | // Enable AST dumps.
    mathpresso::kOptionDebugMachineCode; // Enable machine code dumps.

  mathpresso::Expression exp;
  mathpresso::Error err = exp.compile(ctx,
    "-(-(abs(x * y - floor(x)))) * z * (12.9 - 3)", options, &outputLog);

  if (err) {
    // Handle possible error. The OutputLog has already received the reason
    // in a human readable form.
    printf("ERROR %u\n", err);
    return 1;
  }

  // Evaluate the expression, if compiled.
  Data data = { 12.2, 9.2, -1.9 };
  double result = exp.evaluate(&data);

  printf("RESULT: %f\n", result);
  return 0;
}
