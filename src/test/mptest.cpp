// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../mathpresso/mathpresso.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

struct TestExpression {
  const char* expression;
  double expected;
};

#define TABLE_SIZE(table) \
  (sizeof(table) / sizeof(table[0]))

#define TEST_EXPRESSION(expression) \
  { #expression, (double)(expression) }

#if defined(min)
 #undef min
#endif // min

#if defined(max)
# undef max
#endif // max

inline double min(double x, double y) { return x < y ? x : y; }
inline double max(double x, double y) { return x > y ? x : y; }

int main(int argc, char* argv[]) {
  double x = 5.1;
  double y = 6.7;
  double z = 9.9;
  double PI = 3.14159265358979323846;

  mathpresso::Context ctx;
  mathpresso::Expression e0;
  mathpresso::Expression e1;

  ctx.addEnvironment(mathpresso::kMPEnvironmentAll);
  ctx.addVariable("x", 0 * sizeof(double));
  ctx.addVariable("y", 1 * sizeof(double));
  ctx.addVariable("z", 2 * sizeof(double));

  double variables[] = { x, y, z };

  TestExpression tests[] = {
    TEST_EXPRESSION( x+y ),
    TEST_EXPRESSION( x-y ),
    TEST_EXPRESSION( x*y ),
    TEST_EXPRESSION( x/y ),
    TEST_EXPRESSION( -(x+y) ),
    TEST_EXPRESSION( -(x-y) ),
    TEST_EXPRESSION( x*z + y*z),
    TEST_EXPRESSION( x*z - y*z),
    TEST_EXPRESSION( x*z*y*z),
    TEST_EXPRESSION( x*z/y*z),
    TEST_EXPRESSION( sin(x) * cos(y) * tan(z) ),
    TEST_EXPRESSION( min(x, y) ),
    TEST_EXPRESSION( max(x, y) ),
    TEST_EXPRESSION( x == y ),
    TEST_EXPRESSION( x != y ),
    TEST_EXPRESSION( x <  y ),
    TEST_EXPRESSION( x <= y ),
    TEST_EXPRESSION( x >  y ),
    TEST_EXPRESSION( x >= y ),
    TEST_EXPRESSION( -x ),
    TEST_EXPRESSION( -1 + x ),
    TEST_EXPRESSION( -(-(-1)) ),
    TEST_EXPRESSION( -(-(-x)) ),
    TEST_EXPRESSION( (x+y)*x ),
    TEST_EXPRESSION( (x+y)*y ),
    TEST_EXPRESSION( (x+y)*(1.19+z) ),
    TEST_EXPRESSION( ((x+(x+2.13))*y) ),
    TEST_EXPRESSION( (x+y+z*2+(x*z+z*1.5)) ),
    TEST_EXPRESSION( (((((((x-0.28)+y)+x)+x)*x)/1.12)*y) ),
    TEST_EXPRESSION( ((((x*((((y-1.50)+1.82)-x)/PI))/x)*x)+z) ),
    TEST_EXPRESSION( (((((((((x+1.35)+PI)/PI)-y)+z)-z)+y)/x)+0.81) )
  };

  for (int i = 0; i < TABLE_SIZE(tests); i++) {
    printf("EXP: %s\n", tests[i].expression);

    if (e0.create(ctx, tests[i].expression, mathpresso::kMPOptionDisableJIT) != mathpresso::kMPResultOk) {
      printf("     Failure: Compilation error (no-jit).\n");
      continue;
    }

    if (e1.create(ctx, tests[i].expression, mathpresso::kMPOptionNone) != mathpresso::kMPResultOk) {
      printf("     Failure: Compilation error (use-jit).\n");
      continue;
    }

    double res0 = e0.evaluate(variables);
    double res1 = e1.evaluate(variables);
    double expected = tests[i].expected;

    const char* msg0 = res0 == expected ? "Ok" : "Failure";
    const char* msg1 = res1 == expected ? "Ok" : "Failure";

    printf(
      "     expected=%f\n"
      "     eval    =%f (%s)\n"
      "     jit     =%f (%s)\n"
      "\n",
      expected, res0, msg0, res1, msg1);
  }

  return 0;
}
