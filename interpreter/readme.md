# Model Language Interpreter

An interpreter for a small imperative model language, written in C++.

## Overview

The interpreter runs in two phases:

1. **Lexing + parsing + semantic analysis + code generation** — a recursive
   descent parser drives a finite-state lexer, checks context conditions, and
   emits an intermediate representation (reverse Polish notation, POLIZ).
2. **Execution** — a stack machine interprets the generated POLIZ.

## Language features

- Types: `int` and `string`
- Declarations with optional initialization: `int a = -5, b;` `string s = "hi";`
- Statements: `if/else`, `while`, `read`, `write`, compound blocks `{ ... }`
- Operators: `+ - * /`, comparisons `< > <= >= == !=`, logical `and or not`,
  assignment `=` (chainable: `a = b = c = 0;`)
- String concatenation and string comparisons
- Comments: `/* ... */` and `# ...`

## Build

    g++ -std=c++17 -O2 -o interpreter interpreter.cpp

## Usage

    ./interpreter program.txt

The program reads input from stdin when it hits a `read` statement.

## Example

    program
    {
     int a = 51, b = 6, c;
     string x = "abc", y, z = "abcd";
     c = (a + b) * 2;
     if (c >= 100 or x == z)
     {
      read (y);
      write (x + y + z, c);
     }
     write (x);
    }

## Error handling

Errors are reported through a single `InterpretError` type with a
message and, where applicable, the source line number. Covers lexical,
syntax, and context errors (undeclared/redeclared identifiers, type
mismatches), use of unassigned variables, and division by zero. Exits
with a non-zero status on any error.

## Tests

Two test categories, following the assignment specification:

- `a_tests/` — **valid programs**, checked against expected output.
- `b_tests/` — **error programs**, checked for correct diagnostics.

Run the whole suite:

    bash run_tests.sh

## Scope

This implements the **common part** of the language specification. The
optional variant features (`case`, `do-while`, `for`, `goto/break/continue`,
`boolean`/`real` types, the `%` operator, lazy boolean evaluation) are not
implemented.
