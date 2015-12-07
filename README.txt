MathPresso - Mathematical Expression Evaluator And Jit Compiler
===============================================================

Official Repository: https://github.com/kobalicek/mathpresso-ng

MathPresso is C++ library that is able to parse mathematical expressions and compile them into machine code. It's much faster than traditional AST-based evaluators, because there is basically no overhead in expression execution. The JIT compiler is based on AsmJit and works on X86 and X64 architectures.

This is an updated version of MathPresso that works with a new AsmJit library and uses double-precision floating points. It has some bugs fixed compared to the last version on google-code, but it's not up to date with a MathPresso forks called DoublePresso.

This is also a transitional version that is available to users that want to use MathPresso and cannot wait for the new MPSL engine, which is a work in progress.

Dependencies
============

AsmJit - 1.0 or later.

Contact
=======

Petr Kobalicek <kobalicek.petr@gmail.com>
(TODO: add here contact to people that maintain DoublePresso)
