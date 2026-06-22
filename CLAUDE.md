# Kex Language Compiler

Kex is a functional programming language with Ruby-like syntax, UFCS (Uniform Function Call Syntax), immutability by default, and an Elixir-style process model. File extension: `.kex`.

## Build

```
cmake -B build -G "Unix Makefiles"
cmake --build build
```

Run: `./build/kex <file.kex>`

## Project Structure

- `grammar.ebnf` — formal grammar specification (EBNF)
- `examples/` — example `.kex` source files covering all language features
- `src/` — compiler source (C++20)
  - `src/lexer/` — tokenizer (token.hxx/cxx, lexer.hxx/cxx)
  - `src/parser/` — recursive descent parser (parser.hxx/cxx)
  - `src/ast/` — AST node types (ast.hxx)
  - `src/main.cxx` — CLI entry point

## Code Style

- C++20
- File extensions: `.hxx` (headers), `.cxx` (source)
- camelCase for function/method names
- `m_member` for class members
- `auto foo() -> ReturnType` trailing return style
- Namespace: `kex`

## Current Status

Lexer, AST, and parser are complete. All 17 example files parse successfully. Next: semantic analysis or codegen.
