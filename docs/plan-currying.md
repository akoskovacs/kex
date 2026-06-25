# Currying / Partial Application Plan

## Motivation

Add explicit currying/partial application to Kex using the `~` prefix. This is opt-in — without `~`, missing args remain a compile error. The goal is a lightweight "demo" feature that doesn't complicate the core language.

## Syntax

```kex
~func(args)                    # partial application
~(+)                           # operator as binary function value
~(+)(2)                        # operator with first arg bound (left-to-right)
~(-)(_, 2)                     # operator with second arg bound (placeholder)
~func(_, b)                    # skip first arg, bind second
~(+)(2)(3)                     # chained application — fully applied, evaluates to 5
```

## Examples

```kex
let add(a, b) = a + b
let multiply(a, b) = a * b

# Partial application of named functions
let inc = ~add(1)
let double = ~multiply(2)
let doubles = [1, 2, 3].map(~multiply(2))   # [2, 4, 6]

# Operators as functions
let sum = [1, 2, 3].reduce(0, ~(+))         # 6
let product = [1, 2, 3].reduce(1, ~(*))     # 6

# Placeholder for explicit arg positioning
let sub5 = ~(-)(_, 5)                        # {|x| x - 5}
let div_by_2 = ~(/)(_, 2)                    # {|x| x / 2}
let result = sub5(10)                        # 5

# Chained application
~(+)(2)(3)                                   # 5
~add(1)(2)                                   # 3
```

## Design Decisions

1. **Monolithic parse** — the parser greedily consumes all trailing `(args)` groups as part of the `~` form. No general `expr(args)` syntax added. `~(+)(2)(3)` is one syntactic unit parsed entirely by `parseCurryExpr()`.

2. **Compile error if fully saturated without trailing call** — `~func(args)` is only valid when there are genuinely missing args, OR when followed by more `(args)` groups that complete the application. Enforced at semantic analysis, not parsing.

3. **`_` only valid inside `~` expressions** — bare `func(_, x)` without `~` remains a compile error. The parser specifically allows `_` as `CurryPlaceholder` when parsing curry arg lists.

4. **No UFCS combination** — `items.~add(5)` is disallowed. Use `items.map(~add(5))` instead.

5. **Left-to-right fill** — `~(+)(2)` is shorthand for `~(+)(2, _)`. Use explicit `_` when order matters.

6. **CurryExpr holds a name string** — not an arbitrary expression. You can curry named functions and operators, not arbitrary expressions.

## Known Limitations

- **Named arguments can't be curried** — only positional args work with `~`.
- **Can't curry arbitrary expressions** — `~(some_var)(1)` is not valid. Only `~name(args)` and `~(op)(args)` forms. To curry a variable, assign it a name first.
- **Multiple `_` produces a multi-arg function** — `~f(_, 2, _)` takes 2 args, filled left-to-right positionally.
- **Method currying** — `&.method` handles method references. For methods with args, use `~Module.method(bound_arg)` with the explicit qualified name.
- **No trailing blocks** — `~func(1) do ... end` is not supported; the do-block won't attach to the curry expression.

## Grammar

```ebnf
expr = ...
     | curry_expr
     ;

curry_expr
  = TILDE LOWER_IDENT LPAREN curry_args RPAREN curry_chain*
  | TILDE LPAREN binary_op RPAREN curry_chain*
  ;

curry_chain = LPAREN curry_args RPAREN ;

curry_args = curry_arg ( COMMA curry_arg )* | (* empty *) ;

curry_arg
  = UNDERSCORE
  | expr
  ;

TILDE = "~" ;
```

## Implementation

1. **Token** — Add `Tilde` to `TokenType` enum, string mapping, and lexer switch
2. **AST** — Add `CurryPlaceholder` (empty struct) and `CurryExpr` to the `Expr` variant:
   ```cpp
   struct CurryPlaceholder {};

   struct CurryExpr {
       std::string name;                           // function or operator name
       bool isOperator;                            // true for ~(+), ~(*), etc.
       std::vector<std::vector<ExprPtr>> argGroups; // each (args) group in the chain
   };
   ```
   Each element of `argGroups` is one `(args)` paren group. An empty `argGroups` means bare `~(+)`.
3. **Parser — parseCurryExpr()** — Dispatched from `parsePrimary()` when `~` is seen. Parses the name/operator, then greedily consumes all trailing `(curry_args)` groups.
4. **Semantic check** (future) — Error if fully saturated with no remaining open positions.

## BEAM Codegen

Currying desugars to lambda generation — no runtime support needed.

### Desugaring Examples

```kex
~add(1)          # add has arity 2
```
```erlang
fun (Arg2) -> apply 'add'/2(1, Arg2) end
```

```kex
~(+)
```
```erlang
fun (Arg1, Arg2) -> call 'erlang':'+'(Arg1, Arg2) end
```

```kex
~(+)(2)
```
```erlang
fun (Arg2) -> call 'erlang':'+'(2, Arg2) end
```

```kex
~(-)(_, 5)
```
```erlang
fun (Arg1) -> call 'erlang':'-'(Arg1, 5) end
```

```kex
~(+)(2)(3)       # fully applied — no lambda, direct call
```
```erlang
call 'erlang':'+'(2, 3)
```

### Dependencies

- Requires **Resolve pass** — needs function arities to know which positions are open
- Requires **TypeCheck** (minimal) — for UFCS-qualified curry like `~Module.method(arg)`
- Fits in the desugaring pass alongside loops and `?` rewriting (Milestone 4 in beam-codegen-plan)
- No runtime library additions needed

### Placement in Pipeline

```
parse → semantic (resolve arities) → desugar (curry → lambda) → Core Erlang emit
```

The curry desugarer runs early in the desugar pass since the output (lambdas, direct calls) is already handled by downstream stages.

## Files to Modify

- `src/lexer/token.hxx` — enum entry
- `src/lexer/token.cxx` — string mapping
- `src/lexer/lexer.cxx` — switch case
- `src/ast/ast.hxx` — CurryPlaceholder, CurryExpr structs + variant entries
- `src/parser/parser.hxx` — declare `parseCurryExpr()`
- `src/parser/parser.cxx` — `parsePrimary()` branch + `parseCurryExpr()` implementation
- `grammar.ebnf` — formal grammar update
