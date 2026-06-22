#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kex::interpreter {

struct Value;
using ValuePtr = std::shared_ptr<Value>;

struct NoneValue {};
struct IntValue { int64_t value; };
struct FloatValue { double value; };
struct StringValue { std::string value; };
struct BoolValue { bool value; };
struct AtomValue { std::string name; };

struct ListValue { std::vector<ValuePtr> elements; };
struct TupleValue { std::vector<ValuePtr> elements; };
struct MapValue { std::vector<std::pair<ValuePtr, ValuePtr>> entries; };
struct RangeValue { int64_t start; int64_t end; };

using StreamGenerator = std::function<std::shared_ptr<struct Value>(int64_t index)>;
struct StreamValue {
    StreamGenerator generator;
    int64_t offset = 0;
};

struct RecordValue {
    std::string typeName;
    std::unordered_map<std::string, ValuePtr> fields;
};

using NativeFunc = std::function<ValuePtr(std::vector<ValuePtr>)>;

struct FunctionValue {
    std::string name;
    NativeFunc native; // for built-in functions
};

struct LambdaValue {
    std::vector<std::string> params;
    // body is stored by reference to AST — evaluated at call time
    const void* body = nullptr; // points to vector<ExprPtr>
    struct Environment* closure = nullptr;
};

struct Value {
    std::variant<
        NoneValue,
        IntValue,
        FloatValue,
        StringValue,
        BoolValue,
        AtomValue,
        ListValue,
        TupleValue,
        MapValue,
        RangeValue,
        StreamValue,
        RecordValue,
        FunctionValue,
        LambdaValue
    > data;

    static auto none() -> ValuePtr;
    static auto integer(int64_t v) -> ValuePtr;
    static auto floating(double v) -> ValuePtr;
    static auto string(std::string v) -> ValuePtr;
    static auto boolean(bool v) -> ValuePtr;
    static auto atom(std::string name) -> ValuePtr;
    static auto list(std::vector<ValuePtr> elems) -> ValuePtr;
    static auto tuple(std::vector<ValuePtr> elems) -> ValuePtr;
    static auto record(std::string type, std::unordered_map<std::string, ValuePtr> fields) -> ValuePtr;

    auto isTrue() const -> bool;
    auto toString() const -> std::string;
    auto toRepr() const -> std::string;
    auto typeName() const -> std::string;
};

auto valuesEqual(const ValuePtr& a, const ValuePtr& b) -> bool;

} // namespace kex::interpreter
