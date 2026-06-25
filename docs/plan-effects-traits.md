# Effects and Traits

## Effect System: `foul`

Kex tracks effects without monads. Functions are either pure or foul.

### Rules

- `let` — pure function. Compiler enforces no effects.
- `foul` — effectful function (replaces `let`, not a modifier on it).
- `main` — implicitly foul.
- Pure functions cannot call foul functions (compile error).
- Foul functions can call pure functions freely.

### Examples

```kex
# Pure
let parse(input: String) -> Config =
  ...

# Foul
foul readConfig(path: String) -> Result<Config, AppError> do
  IO.readFile(path)?.parse()
end

foul startApp do
  Supervisor.start(strategy: :one_for_one) do
    worker(Database)
  end
end

# One-liner
foul printHello = IO.printLine("Hello!")

# Entry point
main do
  IO.printLine("Hello!")
end
```

### `using` is for imports

`using` is a namespace import mechanism (not effects):

```kex
module App do
  using Http do
    let router = Router.Config {}
      .get("/", &handleHome)
  end
end
```

### `IO.inspect` — debug escape hatch

`IO.inspect` is a special IO call that the compiler ignores for purity checking. Pure functions can use it freely.

```kex
let parse(input: String) -> Config do
  let tokens = tokenize(input)
  IO.inspect(tokens)  # doesn't make this foul
  buildConfig(tokens)
end
```

- Always available, even in pure functions
- Compiler never treats it as a foul call
- Prints to stderr, not stdout
- Fully eliminated (argument included) in release builds — don't put side effects inside `IO.inspect()`
- Compiler flag (`--inspect=keep`) to retain in release for production debugging

### Relationship to processes

Effects are visible in the code through service calls (`IO.readFile`, `Net.fetch`, etc.). The compiler infers which services a foul function uses — no manual capability annotations needed. Tooling (LSP, docs) can display inferred capabilities on hover.

### Foul closures

Closures are never annotated as foul. Instead, the enclosing function's purity determines what's allowed:

```kex
# Fine — enclosing function is foul
foul process(nums: [Int]) -> [Int] do
  nums.map { |n| IO.printLine(n); n * 2 }
end

# Error — enclosing function is pure, closure calls foul
let process(nums: [Int]) -> [Int] =
  nums.map { |n| IO.printLine(n); n * 2 }  # compile error
```

No changes to `map`'s signature needed — it accepts any closure. Purity is checked at the context level, not the closure level.

### Foul trait methods

Traits can declare foul methods:

```kex
trait Serializable do
  foul save : (path: String) -> Result<Unit, Error>
  foul load : (path: String) -> This
end
```

A trait can mix pure and foul methods. Implementors must match the foul marker.

### Granular capabilities

Not in the syntax. The compiler infers capabilities from the body. If granular restrictions are needed later, they can be added as optional annotations without breaking existing code.

---

## Traits

Traits declare type contracts. They use type signatures (`:`) for required methods and `let` for default implementations.

### Syntax

```kex
trait Printable do
  to(String) : () -> String
end

type Comparison = Less | Equal | Greater
trait Comparable do
  compare : This -> Comparison
end
```

`Comparison` is an ordinary sum type (Kex's existing "enum," see `docs/types.md`'s
Sum Types section) — no separate enum-with-discriminant-value feature needed.
Where an `Int` (negative/zero/positive, the Ruby `<=>` convention used by
`sort`/`examples/types.kex`/`docs/types.md` today) is wanted from a
`Comparison`, that's a `to(Integer)` conversion method, added later as part of
the broader `to(Type)`-style conversion mechanism (see `Printable`'s
`to(String)` above for the same pattern) — not a new language feature now.

- `for: Kind` — optional, defaults to `for: This`. Not used by any trait in this doc right now (see Higher-Kinded Types below) — reserved for when a trait needs to apply to a type constructor instead of a concrete type.
- `name : type` — required, implementor must provide.
- `let name(...) = ...` — default implementation, can be overridden.

### Implementation via `make`

```kex
make Point implement: Comparable do
  let compare(other) -> Comparison = ...
end
```

### `this` and `This`

- `this` — the instance (value-level). Used inside `make` blocks and trait signatures.
- `This` — the implementing type (type-level). Used in trait signatures to refer to the concrete type.

```kex
trait Cloneable do
  clone : (this) -> This
end

make MyRecord implement: Cloneable do
  let clone -> MyRecord = MyRecord { ...this }
end
```

---

## Higher-Kinded Types (HKTs)

**Deferred — not part of this plan.** Traits like `Monad`/`Functor` that abstract
over a type constructor (`Option`, `Result`, etc.) rather than a concrete type
need this, but the syntax for declaring "this trait applies to a type
constructor, not a type" (a candidate was `F<_>`, the `for:` slot exists to
carry it) isn't settled, and it's not clear yet that HKTs are wanted in Kex at
all. Traits in this doc are all `for: This` (the default, see Syntax above) —
i.e. they apply to concrete types only. Revisit as a separate follow-up doc if
a concrete need for Monad/Functor-style abstraction shows up; don't let it
block landing the rest of the trait system.

### Type parameter convention

Single uppercase letters (`A`, `B`, `C`) are type parameters, not concrete types.

---

## Shared State

No global mutable variables. If you need shared mutable state, use a process.

```kex
# Module-level `let` is fine (immutable constant)
let MAX_RETRIES = 3

# Mutable shared state = a process
foul startCounter -> Process<CounterMessage> do
  spawn do
    var state = 0
    loop
      receive do
        :increment -> state = state + 1
        (:get, sender) -> sender.send(state)
      end
    end
  end
end
```

Module-level `var` does not exist. This avoids concurrency bugs and keeps the process model as the single mechanism for shared state.

---

## Design Rationale

- **No mandatory monads.** The process model handles effects at runtime (isolation, sequencing, failure) — no need to encode them in the type system.
- **`foul` gives signature-level effect information** without monadic ceremony (no lifting, no transformer stacks). Compiler infers granular capabilities.
- **HKTs (and Monad/Functor-style traits built on them) are deferred**, not designed yet — see Higher-Kinded Types above. Nothing else in this doc depends on them.
- **No global mutable state.** Shared state lives in processes — the single concurrency primitive.
