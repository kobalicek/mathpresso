// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../src/mathpresso/mathpresso.h"

#include <limits>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Undef all macros that could collide with mathpresso language builtins (see tests).
#if defined(min)
# undef min
#endif

#if defined(max)
# undef max
#endif

#if defined(isnan)
# undef isnan
#endif

#if defined(isinf)
# undef isinf
#endif

#if defined(isfinite)
# undef isfinite
#endif

#if defined(__GNUC__)
static inline void set_x87_mode(unsigned short mode) {
  __asm__ __volatile__ ("fldcw %0" :: "m" (*&mode));
}
#endif

// Double Bits
// ===========

// This is a copy-pase from `mpeval_p.h`, we can't include it here since it's
// private.
union DoubleBits {
  static MATHPRESSO_INLINE DoubleBits from_double(double d) { DoubleBits u; u.d = d; return u; }

  MATHPRESSO_INLINE void set_nan() { hi = 0x7FF80000u; lo = 0x00000000u; }
  MATHPRESSO_INLINE void set_inf() { hi = 0x7FF00000u; lo = 0x00000000u; }

  MATHPRESSO_INLINE bool is_nan() const { return (hi & 0x7FF00000u) == 0x7FF00000u && ((hi & 0x000FFFFFu) | lo) != 0x00000000u; }
  MATHPRESSO_INLINE bool is_inf() const { return (hi & 0x7FFFFFFFu) == 0x7FF00000u && lo == 0x00000000u; }
  MATHPRESSO_INLINE bool is_finite() const { return (hi & 0x7FF00000u) != 0x7FF00000u; }

  double d;
  struct { unsigned int lo, hi; };
};

// Test Option
// ===========

struct TestOption {
  const char* name;
  unsigned int options;
};

// Test Expression
// ===============

struct TestExpression {
  const char* expression;
  double result;
  double xyz[3];
};

// Test Logger
// ===========

struct TestOutputLog : public mathpresso::OutputLog {
  TestOutputLog() {}
  virtual ~TestOutputLog() {}
  virtual void log(unsigned int type, unsigned int line, unsigned int column, const char* message, size_t size) {
    (void)size;

    switch (type) {
      case kMessageError     : printf("[Failure]: %s (at %u:%u)\n", message, line, column); break;
      case kMessageWarning   : printf("[Warning]: %s (at %u:%u)\n", message, line, column); break;
      case kMessageAstInitial: printf("[AST-Initial]:\n%s", message); break;
      case kMessageAstFinal  : printf("[AST-Final]:\n%s", message); break;
      case kMessageAsm       : printf("[Machine-Code]:\n%s", message); break;
    }
  }
};

// Test Functions
// ==============

static double custom1(double x) { return x; }
static double custom2(double x, double y) { return x + y; }

// Test Application
// ================

// The reason for TestApp is that we want to replace all functions the
// expression can use.
struct TestApp {
  // Members
  // -------

  int argc;
  char** argv;


  // Compatibility with the MathPresso Language
  // ------------------------------------------

  // Constants / Variables.
  double E, PI;
  double x, y, z, big;

  // Functions.
  inline double isinf(double x) { return DoubleBits::from_double(x).is_inf() ? 1.0 : 0.0; }
  inline double isnan(double x) { return DoubleBits::from_double(x).is_nan() ? 1.0 : 0.0; }
  inline double isfinite(double x) { return DoubleBits::from_double(x).is_finite() ? 1.0 : 0.0; }

  inline double avg(double x, double y) { return (x + y) * 0.5; }
  inline double min(double x, double y) { return x < y ? x : y; }
  inline double max(double x, double y) { return x > y ? x : y; }

  inline double abs(double x) { return x < 0.0 ? -x : x; }
  inline double recip(double x) { return 1.0 / x; }

  inline double frac(double x) { return x - ::floor(x); }
  inline double round(double x) {
    double y = ::floor(x);
    return y + (x - y >= 0.5 ? double(1.0) : double(0.0));
  }
  inline double roundeven(double x) { return ::rint(x); }

  // TestApp
  // -------

  TestApp(int argc, char* argv[])
    : argc(argc),
      argv(argv),
      E(2.7182818284590452354),
      PI(3.14159265358979323846),
      x(1.5),
      y(2.5),
      z(9.9),
      big(4503599627370496.0) {}

  bool has_arg(const char* arg) {
    for (int i = 1; i < argc; i++) {
      if (::strcmp(argv[i], arg) == 0)
        return true;
    }

    return false;
  }

  int run() {
    bool failed = false;
    bool verbose = has_arg("--verbose");
    bool debug_compiler = has_arg("--debug-compiler");

    // Set the FPU precision to `double` if running 32-bit. Required
    // to be able to compare the result of C++ code with JIT code.
    #if defined(__GNUC__)
    if (sizeof(void*) == 4)
      set_x87_mode(0x027Fu);
    #endif

    mathpresso::Context ctx;
    mathpresso::Expression e;
    TestOutputLog outputLog;

    ctx.add_builtins();
    ctx.add_variable("x"  , 0 * sizeof(double));
    ctx.add_variable("y"  , 1 * sizeof(double));
    ctx.add_variable("z"  , 2 * sizeof(double));
    ctx.add_variable("big", 3 * sizeof(double));

    ctx.add_function("custom1", (void*)custom1, mathpresso::kFunctionArg1);
    ctx.add_function("custom2", (void*)custom2, mathpresso::kFunctionArg2);

    #define TEST_INLINE(exp) { #exp, (double)(exp), { x, y, z } }
    #define TEST_STRING(str, result) { str, result, { x, y, z } }
    #define TEST_OUTPUT(str, result, x, y, z) { str, result, { x, y, z } }

    TestExpression tests[] = {
      TEST_INLINE(0.0),
      TEST_INLINE(10.0),
      TEST_INLINE(10.5),
      TEST_INLINE(10.55),
      TEST_INLINE(10.055),
      TEST_INLINE(100.0),
      TEST_INLINE(100.5),
      TEST_INLINE(1999.0),

      TEST_INLINE(3.14),
      TEST_INLINE(3.141),
      TEST_INLINE(3.1415),
      TEST_INLINE(3.14159),
      TEST_INLINE(3.141592),
      TEST_INLINE(3.1415926),
      TEST_INLINE(3.14159265),
      TEST_INLINE(3.141592653),
      TEST_INLINE(3.1415926535),
      TEST_INLINE(3.14159265358),
      TEST_INLINE(3.141592653589),
      TEST_INLINE(3.1415926535897),
      TEST_INLINE(3.14159265358979),
      TEST_INLINE(3.141592653589793),
      TEST_INLINE(3.1415926535897932),
      TEST_INLINE(3141592653589793.2),
      TEST_INLINE(314159265358979.32),
      TEST_INLINE(31415926535897.932),
      TEST_INLINE(3141592653589.7932),
      TEST_INLINE(314159265358.97932),
      TEST_INLINE(31415926535.897932),
      TEST_INLINE(3141592653.5897932),
      TEST_INLINE(314159265.35897932),
      TEST_INLINE(31415926.535897932),
      TEST_INLINE(3141592.6535897932),
      TEST_INLINE(314159.26535897932),
      TEST_INLINE(31415.926535897932),
      TEST_INLINE(3141.5926535897932),
      TEST_INLINE(314.15926535897932),
      TEST_INLINE(31.415926535897932),
      TEST_INLINE(3.1415926535897932),

      TEST_INLINE(1.2345),
      TEST_INLINE(123.45e-2),

      TEST_INLINE(12.345),
      TEST_INLINE(123.45e-1),

      TEST_INLINE(123.45),
      TEST_INLINE(123.45e0),

      TEST_INLINE(1234.5),
      TEST_INLINE(123.45e1),

      TEST_INLINE(12345),
      TEST_INLINE(123.45e2),

      TEST_INLINE(123450),
      TEST_INLINE(123.45e3),

      TEST_INLINE(1234500),
      TEST_INLINE(123.45e4),

      TEST_INLINE(12345000),
      TEST_INLINE(123.45e5),

      TEST_INLINE(1234500000),
      TEST_INLINE(12345e5),
      TEST_INLINE(12345e+5),
      TEST_INLINE(12345.e+5),
      TEST_INLINE(12345.0e+5),
      TEST_INLINE(12345.0000e+5),

      TEST_INLINE(0.12345),
      TEST_INLINE(12345e-5),
      TEST_INLINE(12345.e-5),
      TEST_INLINE(12345.0e-5),
      TEST_INLINE(12345.0000e-5),

      TEST_INLINE(1.7976931348623157e+308),
      TEST_INLINE(2.2250738585072014e-308),

      TEST_INLINE(isinf(x)),
      TEST_INLINE(isnan(x)),
      TEST_INLINE(isfinite(x)),

      TEST_STRING("isinf(0.0 / 0.0)", isinf(std::numeric_limits<double>::quiet_NaN())),
      TEST_STRING("isnan(0.0 / 0.0)", isnan(std::numeric_limits<double>::quiet_NaN())),
      TEST_STRING("isfinite(0.0 / 0.0)", isfinite(std::numeric_limits<double>::quiet_NaN())),

      TEST_STRING("isinf(1.0 / 0.0)", isinf(std::numeric_limits<double>::infinity())),
      TEST_STRING("isnan(1.0 / 0.0)", isnan(std::numeric_limits<double>::infinity())),
      TEST_STRING("isfinite(1.0 / 0.0)", isfinite(std::numeric_limits<double>::infinity())),

      TEST_INLINE(x + y),
      TEST_INLINE(x - y),
      TEST_INLINE(x * y),
      TEST_INLINE(x / y),

      TEST_INLINE(x * -y),
      TEST_INLINE(x / -y),

      TEST_INLINE(-x * y),
      TEST_INLINE(-x / y),

      TEST_INLINE(-x * -y),
      TEST_INLINE(-x / -y),

      TEST_STRING(" x %  y", ::fmod( x, y)),
      TEST_STRING(" z %  y", ::fmod( z, y)),
      TEST_STRING(" x % -y", ::fmod( x,-y)),
      TEST_STRING(" z % -y", ::fmod( z,-y)),
      TEST_STRING("-x %  y", ::fmod(-x, y)),
      TEST_STRING("-z %  y", ::fmod(-z, y)),
      TEST_STRING("-x % -y", ::fmod(-x,-y)),
      TEST_STRING("-z % -y", ::fmod(-z,-y)),

      TEST_INLINE(-(x + y)),
      TEST_INLINE(-(x - y)),
      TEST_INLINE(-(x * y)),
      TEST_INLINE(-(x / y)),

      TEST_STRING("-(x % y)", -(::fmod(x, y))),

      #if 0 // Temporarily disabled as it hangs VS, I don't know why.
      TEST_INLINE(x * z + y * z),
      TEST_INLINE(x * z - y * z),
      TEST_INLINE(x * z * y * z),
      TEST_INLINE(x * z / y * z),

      TEST_INLINE(x == y),
      TEST_INLINE(x != y),
      TEST_INLINE(x < y),
      TEST_INLINE(x <= y),
      TEST_INLINE(x > y),
      TEST_INLINE(x >= y),

      TEST_INLINE(x + y == y - z),
      TEST_INLINE(x * y == y * z),
      TEST_INLINE(x > y == y < z),
      #endif

      TEST_INLINE(-x),
      TEST_INLINE(-1.0 + x),
      TEST_INLINE(-(-(-1.0))),
      TEST_INLINE(-(-(-x))),

      TEST_INLINE((x+y)*(1.19+z)),
      TEST_INLINE(((x+(x+2.13))*y)),
      TEST_INLINE((x+y+z*2.0+(x*z+z*1.5))),
      TEST_INLINE((((((((x-0.28)+y)+x)+x)*x)/1.12)*y)),
      TEST_INLINE(((((x*((((y-1.50)+1.82)-x)/PI))/x)*x)+z)),
      TEST_INLINE((((((((((x+1.35)+PI)/PI)-y)+z)-z)+y)/x)+0.81)),

      TEST_INLINE(round(x)),
      TEST_INLINE(round(y)),
      TEST_INLINE(round(big)),
      TEST_INLINE(round(-x)),
      TEST_INLINE(round(-y)),
      TEST_INLINE(round(-big)),

      TEST_INLINE(roundeven(x)),
      TEST_INLINE(roundeven(y)),
      TEST_INLINE(roundeven(big)),
      TEST_INLINE(roundeven(-x)),
      TEST_INLINE(roundeven(-y)),
      TEST_INLINE(roundeven(-big)),

      TEST_INLINE(trunc(x)),
      TEST_INLINE(trunc(y)),
      TEST_INLINE(trunc(big)),
      TEST_INLINE(trunc(-x)),
      TEST_INLINE(trunc(-y)),
      TEST_INLINE(trunc(-big)),
      TEST_INLINE(trunc(0.11)),
      TEST_INLINE(trunc(-0.11)),
      TEST_INLINE(trunc(1232323232.11)),
      TEST_INLINE(trunc(-1232323232.11)),

      TEST_INLINE(floor(x)),
      TEST_INLINE(floor(y)),
      TEST_INLINE(floor(big)),
      TEST_INLINE(floor(-x)),
      TEST_INLINE(floor(-y)),
      TEST_INLINE(floor(-big)),

      TEST_INLINE(ceil(x)),
      TEST_INLINE(ceil(y)),
      TEST_INLINE(ceil(big)),
      TEST_INLINE(ceil(-x)),
      TEST_INLINE(ceil(-y)),
      TEST_INLINE(ceil(-big)),

      TEST_INLINE(abs(-x)),
      TEST_INLINE(abs(-big)),

      TEST_INLINE(frac(x)),
      TEST_INLINE(frac(-x)),
      TEST_INLINE(frac(y)),
      TEST_INLINE(frac(-y)),
      TEST_INLINE(frac(z)),
      TEST_INLINE(frac(-z)),
      TEST_INLINE(frac(big)),
      TEST_INLINE(frac(-big)),

      TEST_INLINE(sqrt(x)),
      TEST_INLINE(recip(x)),
      TEST_INLINE(exp(x)),
      TEST_INLINE(log(x)),
      TEST_INLINE(log10(x)),
      TEST_INLINE(sin(x)),
      TEST_INLINE(cos(x)),
      TEST_INLINE(tan(x)),
      TEST_INLINE(sin(x) * cos(y) * tan(z)),
      TEST_INLINE(avg(x, y)),
      TEST_INLINE(min(x, y)),
      TEST_INLINE(max(x, y)),
      TEST_INLINE(pow(x, y)),

      TEST_INLINE(custom1(x)),
      TEST_INLINE(custom2(x, y)),

      TEST_STRING("var a=1; a", 1.0),
      TEST_STRING("var a=199   * 2; a", 398),
      TEST_STRING("var a=199.  * 2; a", 398),
      TEST_STRING("var a=199.0 * 2; a", 398),

      TEST_STRING("var a=1; a=2; a", 2.0),
      TEST_STRING("var a=x; a=y; a", y),

      TEST_STRING("var a=1, b=2; var t=a; a=b; b=t; a", 2.0),
      TEST_STRING("var a=1, b=2; var t=a; a=b; b=t; b", 1.0),
      TEST_STRING("var a=x, b=y; var t=a; a=b; b=t; a", y),
      TEST_STRING("var a=x, b=y; var t=a; a=b; b=t; b", x),

      TEST_STRING("var a=x  ; a=a*a*a  ; a", x * x * x),
      TEST_STRING("var a=x  ; a=a*a*a*a; a", x * x * x * x),
      TEST_STRING("var a=x+1; a=a*a*a  ; a", (x+1.0) * (x+1.0) * (x+1.0)),
      TEST_STRING("var a=x+1; a=a*a*a*a; a", (x+1.0) * (x+1.0) * (x+1.0) * (x+1.0)),

      TEST_OUTPUT("x = 11; y = 22; z = 33"   , 33.0, 11.0, 22.0, 33.0),
      TEST_OUTPUT("x = 11; y = 22; z = 33;"  , 33.0, 11.0, 22.0, 33.0),
      TEST_OUTPUT("x = 11; y = 22; z = 33; x", 11.0, 11.0, 22.0, 33.0),
      TEST_OUTPUT("x =  y; y =  z; x = 99; x", 99.0, 99.0, z,    z   ),
      TEST_OUTPUT("x =  y; y =  z; x = 99; y", z   , 99.0, z,    z   ),
      TEST_OUTPUT("x =  y; y =  z; x = 99; z", z   , 99.0, z,    z   ),

      TEST_OUTPUT("var t = x; x = y; y = z; z = t"   , x, y, z, x),
      TEST_OUTPUT("var t = x; x = y; y = z; z = t; t", x, y, z, x)
    };

    #undef TEST_OUTPUT
    #undef TEST_STRING
    #undef TEST_INLINE

    unsigned int defaultOptions = mathpresso::kNoOptions;
    if (verbose)
      defaultOptions |= mathpresso::kOptionVerbose | mathpresso::kOptionDebugMachineCode;

    if (debug_compiler)
      defaultOptions |= mathpresso::kOptionVerbose | mathpresso::kOptionDebugCompiler;

    TestOption options[] = {
      { "SSE2", defaultOptions | mathpresso::kOptionDisableSSE4_1 },
      { "BEST", defaultOptions                                    }
    };

    printf("MPTest environment:\n");
    printf("  x = %f\n", (double)x);
    printf("  y = %f\n", (double)y);
    printf("  z = %f\n", (double)z);
    printf("  big = %f\n", (double)big);

    for (size_t i = 0; i < MATHPRESSO_ARRAY_SIZE(tests); i++) {
      const TestExpression& test = tests[i];
      const char* exp = test.expression;
      bool allOk = true;

      for (size_t j = 0; j < MATHPRESSO_ARRAY_SIZE(options); j++) {
        const TestOption& option = options[j];

        if (verbose) {
          printf("[Compile]:\n"
                 "  \"%s\" (%s)\n", exp, option.name);
        }

        int err = e.compile(ctx, exp, option.options, &outputLog);
        if (err) {
          printf("[ERROR %u]: \"%s\" (%s)\n", err, exp, option.name);
          allOk = false;
          continue;
        }

        double arg[] = { x, y, z, big };
        double result = e.evaluate(arg);

        if (result != test.result ||
            arg[0] != test.xyz[0] ||
            arg[1] != test.xyz[1] ||
            arg[2] != test.xyz[2]) {
          printf("[Failure]: \"%s\" (%s)\n", exp, option.name);

          static const char indentation[] = "  ";
          if (result != test.result) printf("%s _(%.17g) != expected(%.17g)\n", indentation, result, test.result);
          if (arg[0] != test.xyz[0]) printf("%s x(%.17g) != expected(%.17g)\n", indentation, arg[0], test.xyz[0]);
          if (arg[1] != test.xyz[1]) printf("%s y(%.17g) != expected(%.17g)\n", indentation, arg[1], test.xyz[1]);
          if (arg[2] != test.xyz[2]) printf("%s z(%.17g) != expected(%.17g)\n", indentation, arg[2], test.xyz[2]);

          allOk = false;
        }
      }

      if (allOk)
        printf("[Success]: \"%s\" -> %.17g\n", exp, test.result);
      else
        failed = true;
    }

    return failed ? 1 : 0;
  }
};

int main(int argc, char* argv[]) {
  return TestApp(argc, argv).run();
}
