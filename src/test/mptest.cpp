// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../mathpresso/mathpresso.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(min)
 #undef min
#endif // min

#if defined(max)
# undef max
#endif // max

#if defined(__GNUC__) || defined(__clang__)
inline void set_x87_mode(unsigned short mode) {
  __asm__ __volatile__ ("fldcw %0" :: "m" (*&mode));
}
#endif

// ============================================================================
// [Double]
// ============================================================================

// Wrapper for `double` so we can test % operator in our tests properly.
struct Double {
  inline Double(double val = 0) : val(val) {}
  inline operator double() const { return val; }

  inline Double& operator=(double x) { val = x; return *this; }
  inline Double& operator=(const Double& x) { val = x.val; return *this; }

  double val;
};

inline Double operator-(const Double& self) { return Double(-self.val); }
inline Double operator!(const Double& self) { return Double(!self.val); }

inline Double operator+(const Double& x, double y) { return Double(x.val + y); }
inline Double operator-(const Double& x, double y) { return Double(x.val - y); }
inline Double operator*(const Double& x, double y) { return Double(x.val * y); }
inline Double operator/(const Double& x, double y) { return Double(x.val / y); }
inline Double operator%(const Double& x, double y) { return Double(fmod(x.val, y)); }

inline Double operator+(double x, const Double& y) { return Double(x + y.val); }
inline Double operator-(double x, const Double& y) { return Double(x - y.val); }
inline Double operator*(double x, const Double& y) { return Double(x * y.val); }
inline Double operator/(double x, const Double& y) { return Double(x / y.val); }
inline Double operator%(double x, const Double& y) { return Double(fmod(x, y.val)); }

inline Double operator+(const Double& x, const Double& y) { return Double(x.val + y.val); }
inline Double operator-(const Double& x, const Double& y) { return Double(x.val - y.val); }
inline Double operator*(const Double& x, const Double& y) { return Double(x.val * y.val); }
inline Double operator/(const Double& x, const Double& y) { return Double(x.val / y.val); }
inline Double operator%(const Double& x, const Double& y) { return Double(fmod(x.val, y.val)); }

// ============================================================================
// [TestOption]
// ============================================================================

struct TestOption {
  const char* name;
  unsigned int options;
};

// ============================================================================
// [TestExpression]
// ============================================================================

struct TestExpression {
  const char* expression;
  double result;
  double xyz[3];
};

// ============================================================================
// [TestLog]
// ============================================================================

struct TestOutputLog : public mathpresso::OutputLog {
  TestOutputLog() {}
  virtual ~TestOutputLog() {}
  virtual void log(unsigned int type, unsigned int line, unsigned int column, const char* message, size_t len) {
    switch (type) {
      case kMessageError     : printf("[Failure]: %s (at %u)\n", message, column); break;
      case kMessageWarning   : printf("[Warning]: %s (at %u)\n", message, column); break;
      case kMessageAstInitial: printf("[AST-Initial]:\n%s", message); break;
      case kMessageAstFinal  : printf("[AST-Final]:\n%s", message); break;
      case kMessageAsm       : printf("[Machine-Code]:\n%s", message); break;
    }
  }
};

// ============================================================================
// [TestApp]
// ============================================================================

// The reason for TestApp is that we want to replace all functions the
// expression can use.
struct TestApp {
  // --------------------------------------------------------------------------
  // [Compatibility with the MathPresso Language]
  // --------------------------------------------------------------------------

  typedef Double var;

  // Constants / Variables.
  Double E, PI;
  Double x, y, z, big;

  // Functions.
  inline Double avg(double x, double y) { return Double((x + y) * 0.5); }
  inline Double min(double x, double y) { return Double(x < y ? x : y); }
  inline Double max(double x, double y) { return Double(x > y ? x : y); }

  inline Double abs(double x) { return Double(x < 0.0 ? -x : x); }
  inline Double recip(double x) { return Double(1.0 / x); }

  inline Double frac(double x) { return Double(x - ::floor(x)); }
  inline Double round(double x) {
    double y = ::floor(x);
    return Double(y + (x - y >= 0.5 ? double(1.0) : double(0.0)));
  }
  inline Double roundeven(double x) { return Double(::rint(x)); }

  // --------------------------------------------------------------------------
  // [TestApp]
  // --------------------------------------------------------------------------

  TestApp(int argc, char* argv[])
    : argc(argc),
      argv(argv),
      E(2.7182818284590452354),
      PI(3.14159265358979323846),
      x(1.5),
      y(2.5),
      z(9.9),
      big(4503599627370496.0) {
  }

  bool hasArg(const char* arg) {
    for (int i = 1; i < argc; i++) {
      if (::strcmp(argv[i], arg) == 0)
        return true;
    }

    return false;
  }

  int run() {
    bool failed = false;
    bool verbose = hasArg("--verbose");

    // Set the FPU precision to `double` if running 32-bit. Required
    // to be able to compare the result of C++ code with JIT code.
#if defined(__GNUC__) || defined(__clang__)
    if (sizeof(void*) == 4)
      set_x87_mode(0x027FU);
#endif

    mathpresso::Context ctx;
    mathpresso::Expression e;
    TestOutputLog outputLog;

    ctx.addBuiltIns();
    ctx.addVariable("x"  , 0 * sizeof(double));
    ctx.addVariable("y"  , 1 * sizeof(double));
    ctx.addVariable("z"  , 2 * sizeof(double));
    ctx.addVariable("big", 3 * sizeof(double));

#define TEST_INLINE(exp) { #exp, (double)(exp), { x, y, z } }
#define TEST_STRING(str, result) { str, result, { x, y, z } }
#define TEST_OUTPUT(str, result, x, y, z) { str, result, { x, y, z } }

    TestExpression tests[] = {
      TEST_INLINE(0.0),
      TEST_INLINE(10.0),
      TEST_INLINE(100.0),
      TEST_INLINE(1999.0),

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

      TEST_INLINE(x % y),
      TEST_INLINE(z % y),
      TEST_INLINE(x % -y),
      TEST_INLINE(z % -y),
      TEST_INLINE(-x % y),
      TEST_INLINE(-z % y),
      TEST_INLINE(-x % -y),
      TEST_INLINE(-z % -y),

      TEST_INLINE(-(x + y)),
      TEST_INLINE(-(x - y)),
      TEST_INLINE(-(x * y)),
      TEST_INLINE(-(x / y)),
      TEST_INLINE(-(x % y)),

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

      TEST_STRING("var a=x; a=a*a*a; a", x * x * x),
      TEST_STRING("var a=x; a=a*a*a*a; a", x * x * x * x),
      TEST_STRING("var a=x+1; a=a*a*a; a", (x+1.0) * (x+1.0) * (x+1.0)),
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

    unsigned int defaultOptions = mathpresso::kNoOptions;
    if (verbose) {
      defaultOptions |= mathpresso::kOptionVerbose  |
                        mathpresso::kOptionDebugAsm ;
    }

    TestOption options[] = {
      "SSE2", defaultOptions | mathpresso::kOptionDisableSSE4_1,
      "BEST", defaultOptions
    };

    printf("MPTest environment:\n");
    printf("  x = %f\n", (double)x);
    printf("  y = %f\n", (double)y);
    printf("  z = %f\n", (double)z);
    printf("  big = %f\n", (double)big);

    for (int i = 0; i < MATHPRESSO_ARRAY_SIZE(tests); i++) {
      const TestExpression& test = tests[i];
      const char* exp = test.expression;
      bool allOk = true;

      for (int j = 0; j < MATHPRESSO_ARRAY_SIZE(options); j++) {
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

          static const char indentation[] = "          ";
          if (result != test.result) printf("%s result(%f) != expected(%f)\n", indentation, result, test.result);
          if (arg[0] != test.xyz[0]) printf("%s x(%f) != expected(%f)\n", indentation, arg[0], test.xyz[0]);
          if (arg[1] != test.xyz[1]) printf("%s y(%f) != expected(%f)\n", indentation, arg[1], test.xyz[1]);
          if (arg[2] != test.xyz[2]) printf("%s z(%f) != expected(%f)\n", indentation, arg[2], test.xyz[2]);

          allOk = false;
        }
      }

      if (allOk)
        printf("[Success]: \"%s\" -> %f\n", exp, test.result);
      else
        failed = true;
    }

    return failed ? 1 : 0;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  int argc;
  char** argv;
};

// ============================================================================
// [Main]
// ============================================================================

int main(int argc, char* argv[]) {
  return TestApp(argc, argv).run();
}
