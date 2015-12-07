// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../mathpresso/mathpresso.h"

#include <stdio.h>
#include <stdlib.h>

struct Variables {
  double x;
  double y;
  double z;
};

int main(int argc, char* argv[])
{
  mathpresso::Context ctx;
  mathpresso::Expression e;

  Variables variables;
  variables.x = 0.0;
  variables.y = 0.0;
  variables.z = 0.0;

  ctx.addEnvironment(mathpresso::kMPEnvironmentAll);
  ctx.addVariable("x", MATHPRESSO_OFFSET(Variables, x));
  ctx.addVariable("y", MATHPRESSO_OFFSET(Variables, y));
  ctx.addVariable("z", MATHPRESSO_OFFSET(Variables, z));

  fprintf(stdout, "=========================================================\n");
  fprintf(stdout, "MathPresso - Command Line Evaluator\n");
  fprintf(stdout, "(c) 2008-2010, Petr Kobalicek, MIT License.\n");
  fprintf(stdout, "---------------------------------------------------------\n");
  fprintf(stdout, "You can use variables 'x', 'y' and 'z'. Initial values of\n");
  fprintf(stdout, "these variables are 0.0, but using '=' operator the value\n");
  fprintf(stdout, "can be assigned (use for example x = 1).\n");
  fprintf(stdout, "=========================================================\n");

  for (;;)
  {
    char buffer[4096];
    fgets(buffer, 4095, stdin);
    if (buffer[0] == 0) break;

    mathpresso::MPResult result = e.create(ctx, buffer);
    if (result == mathpresso::kMPResultNoExpression) break;

    if (result != mathpresso::kMPResultOk) {
      fprintf(stderr, "Error compiling expression:\n%s\n", buffer);
      break;
    }
    else {
      fprintf(stdout, "%f\n", e.evaluate(&variables));
    }
  }

  return 0;
}
