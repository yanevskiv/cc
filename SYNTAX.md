# Syntax

Checklist toward ISO C99.

## Lexical

- [x] `//` line comments
- [x] `/* */` block comments
- [x] identifiers
- [x] decimal integer literals
- [x] hex integer literals (`0x...`)
- [ ] octal integer literals (`0...`)
- [ ] integer suffixes (`u U l L ll LL` and combinations)
- [ ] floating-point literals (`1.0`, `1e10`, `0x1p0`)
- [x] character literals
- [ ] full escape set (`\a \b \f \v \xHH`, octal `\nnn`, `\uXXXX`/`\UXXXXXXXX`)
- [x] string literals
- [ ] adjacent string literal concatenation (`"a" "b"`)
- [ ] wide literals (`L"..."`, `L'x'`)
- [ ] multi-character constants (`'ab'`)

## Types

- [x] `int`, `char`, `void`
- [ ] `short`
- [ ] `long`, `long long`
- [ ] `float`, `double`
- [ ] `_Bool`
- [ ] `signed` / `unsigned`
- [x] `const` qualifier
- [ ] `volatile` qualifier
- [ ] `restrict` qualifier
- [x] pointer declarators (`int *p`, any number of stars)
- [ ] array declarators, incl. multi-dimensional (`[N][M]`)
- [ ] function-pointer declarators
- [ ] `struct`
- [ ] `union`
- [ ] `enum`
- [ ] `typedef`
- [ ] bitfields
- [ ] compound literals (`(T){ ... }`)

## Declarations

- [x] single declarator with optional initializer
- [ ] multiple declarators per statement (`int a, b, c;`)
- [ ] array initializers (`{1, 2, 3}`)
- [ ] designated initializers (`.field = v`, `[i] = v`)
- [ ] storage classes: `static`, `extern`, `register`, `auto`
- [ ] `inline`
- [ ] top-level (global) variable declarations
- [x] function prototypes (parsed; currently a no-op)

## Expressions and operators

- [x] arithmetic: `+ - * / %`
- [x] relational: `< > <= >=`
- [x] equality: `== !=`
- [x] logical: `&& ||`
- [x] unary `-` and `!`
- [ ] unary `+`
- [ ] bitwise: `& | ^ ~ << >>`
- [ ] compound assignment: `+= -= *= /= %= &= |= ^= <<= >>=`
- [ ] increment / decrement: `++ --` (prefix and postfix)
- [ ] ternary `?:`
- [ ] comma operator
- [x] assignment `=` (lvalue must be a plain variable today)
- [ ] address-of `&` (unary)
- [ ] dereference `*` (unary)
- [ ] array subscript `a[i]`
- [ ] struct/union member access `.` and `->`
- [ ] call through a function pointer
- [ ] `sizeof` (type and expression forms)
- [ ] cast expressions (`(T) expr`)
- [x] function calls, fixed arity
- [ ] compound literals in expression position
- [x] parenthesized expressions

## Statements

- [x] expression statement
- [x] empty statement (`;`)
- [x] compound statement / block
- [x] `if` / `else`
- [x] `while`
- [x] `for` (expression-only init; no `for (int i = 0; ...)`)
- [ ] declaration as a `for`-loop initializer
- [ ] `do`-`while`
- [ ] `switch` / `case` / `default`
- [ ] `break`
- [ ] `continue`
- [ ] `goto` and labels

## Functions

- [x] definitions with a fixed parameter list
- [x] variadic marker `...` (parsed, no varargs access — no `va_list`)
- [x] prototypes (parsed, no-op)
- [ ] function-pointer parameters/variables actually callable
- [ ] old-style (K&R) parameter lists (not planned — obsolete)

## Preprocessor

- [ ] `#include`
- [ ] `#define` (object-like)
- [ ] `#define` (function-like, incl. variadic macros)
- [ ] `#undef`
- [ ] `#if` / `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif`, `defined()`
- [ ] `#pragma`
- [ ] `#error` / `#warning`
- [ ] token pasting (`##`) and stringizing (`#`)
- [ ] predefined macros (`__FILE__`, `__LINE__`, `__func__`, ...)
