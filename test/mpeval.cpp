// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../src/mathpresso/mathpresso.h"

#include <stdio.h>
#include <stdlib.h>

struct Variables {
  double x;
  double y;
  double z;
};

// By inheriting `OutputLog` one can create a way how to handle possible errors
// and report them to humans. The most interesting and used message type is
// `kMessageError`, because it signalizes an invalid expression. Other message
// types are used mostly for debugging.
struct MyOutputLog : public mathpresso::OutputLog {
  virtual void log(unsigned int type, unsigned int line, unsigned int column, const char* message, size_t size) {
    (void)line;
    (void)column;
    (void)size;

    if (type == kMessageError)
      printf("ERROR: %s\n", message);
    else
      printf("WARNING: %s\n", message);
  }
};

int main() {
  mathpresso::Context ctx;
  mathpresso::Expression e;

  MyOutputLog outputLog;

  Variables variables;
  variables.x = 0.0;
  variables.y = 0.0;
  variables.z = 0.0;

  ctx.addBuiltIns();
  ctx.addVariable("x", MATHPRESSO_OFFSET(Variables, x));
  ctx.addVariable("y", MATHPRESSO_OFFSET(Variables, y));
  ctx.addVariable("z", MATHPRESSO_OFFSET(Variables, z));

  fprintf(stdout, "=========================================================\n");
  fprintf(stdout, "MPEval - MathPresso's Command Line Evaluator\n");
  fprintf(stdout, "---------------------------------------------------------\n");
  fprintf(stdout, "You can use variables 'x', 'y' and 'z'. Initial values of\n");
  fprintf(stdout, "these variables are 0.0. Assignment operator '=' can be\n");
  fprintf(stdout, "used to change the initial values.\n");
  fprintf(stdout, "=========================================================\n");

  for (;;) {
    char buffer[4096];
    fgets(buffer, 4095, stdin);
    if (buffer[0] == 0) break;

    mathpresso::Error result = e.compile(ctx, buffer, mathpresso::kOptionVerbose, &outputLog);
    if (result == mathpresso::kErrorOk)
      fprintf(stdout, "%f\n", e.evaluate(&variables));
    if (result == mathpresso::kErrorNoExpression)
      break;
  }

  return 0;
}
