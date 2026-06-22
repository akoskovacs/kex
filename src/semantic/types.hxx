#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kex::semantic {

struct Type;
using TypePtr = std::shared_ptr<Type>;

struct PrimitiveType {
    enum Kind { Int, Float, String, Bool, Atom, Unit };
    Kind kind;
};

struct NamedType {
    std::string name;
    std::vector<TypePtr> typeArgs;
};

struct FuncType {
    std::vector<TypePtr> params;
    TypePtr result;
};

struct TupleType {
    std::vector<TypePtr> elements;
};

struct ListType {
    TypePtr element;
};

struct MapType {
    TypePtr key;
    TypePtr value;
};

struct OptionalType {
    TypePtr inner;
};

struct UnionType {
    std::vector<TypePtr> members;
};

struct TypeVar {
    int id;
};

struct UnknownType {};

struct Type {
    std::variant<
        PrimitiveType,
        NamedType,
        FuncType,
        TupleType,
        ListType,
        MapType,
        OptionalType,
        UnionType,
        TypeVar,
        UnknownType
    > kind;

    static auto integer() -> TypePtr;
    static auto floating() -> TypePtr;
    static auto string() -> TypePtr;
    static auto boolean() -> TypePtr;
    static auto atom() -> TypePtr;
    static auto unit() -> TypePtr;
    static auto unknown() -> TypePtr;
    static auto named(const std::string& name, std::vector<TypePtr> args = {}) -> TypePtr;
    static auto func(std::vector<TypePtr> params, TypePtr result) -> TypePtr;
    static auto list(TypePtr element) -> TypePtr;
    static auto tuple(std::vector<TypePtr> elements) -> TypePtr;
    static auto map(TypePtr key, TypePtr value) -> TypePtr;
    static auto optional(TypePtr inner) -> TypePtr;
    static auto typeVar(int id) -> TypePtr;
};

auto typeToString(const TypePtr& type) -> std::string;
auto typesEqual(const TypePtr& a, const TypePtr& b) -> bool;

class TypeEnv {
public:
    auto set(const std::string& name, TypePtr type) -> void;
    auto get(const std::string& name) const -> TypePtr;
    auto has(const std::string& name) const -> bool;

private:
    std::unordered_map<std::string, TypePtr> m_types;
};

} // namespace kex::semantic
