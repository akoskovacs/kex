# Testing

## Standard Library Framework

Testing is a library, not a language feature. The stdlib provides an RSpec-style framework.

## Syntax

```kex
using Test

describe MyServer do
  before do
    Mock.File.expect(:read, "config.toml", returns: Ok("port=8080"))
  end

  describe "loadConfig" do
    it "parses the config file" do
      let config = MyServer.loadConfig("config.toml")
      assert config == Ok(Config { port: 8080, host: "localhost" })
    end

    it "returns error for missing file" do
      Mock.File.expect(:read, "missing.toml", returns: Error(IOError(:not_found)))
      let result = MyServer.loadConfig("missing.toml")
      assert result.error?
    end
  end
end
```

## Key Components

- `describe` — groups related tests (can be nested)
- `it` — individual test case
- `before` — setup run before each test in the group
- `assert` — assertion (fails the test if false)

## Mocks

The stdlib provides mock implementations of foul modules:

```kex
Mock.File.expect(:read, "path", returns: Ok("content"))
Mock.IO.expect(:println, "hello", returns: ())
Mock.Process.expect(:spawn, returns: fakePid)
```

Since `foul` marks the IO boundary and modules are the unit of abstraction, you know exactly what to mock — swap the foul dependencies.

## Running Tests

```
kex test
kex test path/to/test.kex
```
