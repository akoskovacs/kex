# Error Handling

## No Exceptions

Kex has no exceptions. Errors are values.

## Two Error Types

- `Optional<A>` (aka `A?`) — value might not exist
- `Result<A, E>` — operation might fail with an error

## Optional

```kex
let find(list: [A], f: A -> Bool) -> A? do
  ...
end

# Handle with pattern matching
match findUser(users, "alice") do
  Just(user) -> "Found ${user.name}"
  None -> "Not found"
end

# Or with map/flatMap
findUser(users, "alice")
  .map(&.name)
  .unwrapOr("Unknown")
```

## Result

```kex
type ParseError = InvalidFormat(String) | Overflow | EmptyInput

let parseInt(s: String) -> Result<Int, ParseError> do
  return Error(EmptyInput) if s.empty?
  ...
end
```

## The `?` Operator

`?` works only on `Result` — unwraps `Ok` or short-circuits with `Error`:

```kex
foul let loadConfig(path: String) -> Result<Config, AppError> do
  let content = File.read(path)?          # returns Error early if fails
  let parsed = Config.parse(content)?
  let port = parsePort(parsed["port"])?
  return Ok(Config { port: port })
end
```

`?` does NOT work on `Optional`. Use `match`, `.map`, or `.flatMap` instead.

## Combining

```kex
foul let getUserEmail(id: Int) -> Result<String, AppError> do
  let user = fetchUser(id)?
  match user.email do
    Just(email) -> Ok(email)
    None -> Error(AppError(:no_email))
  end
end
```
