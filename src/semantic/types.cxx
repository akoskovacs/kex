#include "types.hxx"

namespace kex::semantic {

auto Type::integer() -> TypePtr {
    return std::make_shared<Type>(Type{PrimitiveType{PrimitiveType::Int}});
}

auto Type::floating() -> TypePtr {
    return std::make_shared<Type>(Type{PrimitiveType{PrimitiveType::Float}});
}

auto Type::string() -> TypePtr {
    return std::make_shared<Type>(Type{PrimitiveType{PrimitiveType::String}});
}

auto Type::boolean() -> TypePtr {
    return std::make_shared<Type>(Type{PrimitiveType{PrimitiveType::Bool}});
}

auto Type::atom() -> TypePtr {
    return std::make_shared<Type>(Type{PrimitiveType{PrimitiveType::Atom}});
}

auto Type::unit() -> TypePtr {
    return std::make_shared<Type>(Type{PrimitiveType{PrimitiveType::Unit}});
}

auto Type::unknown() -> TypePtr {
    return std::make_shared<Type>(Type{UnknownType{}});
}

auto Type::named(const std::string& name, std::vector<TypePtr> args) -> TypePtr {
    return std::make_shared<Type>(Type{NamedType{name, std::move(args)}});
}

auto Type::func(std::vector<TypePtr> params, TypePtr result) -> TypePtr {
    return std::make_shared<Type>(Type{FuncType{std::move(params), std::move(result)}});
}

auto Type::list(TypePtr element) -> TypePtr {
    return std::make_shared<Type>(Type{ListType{std::move(element)}});
}

auto Type::tuple(std::vector<TypePtr> elements) -> TypePtr {
    return std::make_shared<Type>(Type{TupleType{std::move(elements)}});
}

auto Type::map(TypePtr key, TypePtr value) -> TypePtr {
    return std::make_shared<Type>(Type{MapType{std::move(key), std::move(value)}});
}

auto Type::optional(TypePtr inner) -> TypePtr {
    return std::make_shared<Type>(Type{OptionalType{std::move(inner)}});
}

auto Type::typeVar(int id) -> TypePtr {
    return std::make_shared<Type>(Type{TypeVar{id}});
}

auto typeToString(const TypePtr& type) -> std::string {
    if (!type) return "?";

    return std::visit([](const auto& t) -> std::string {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, PrimitiveType>) {
            switch (t.kind) {
                case PrimitiveType::Int: return "Int";
                case PrimitiveType::Float: return "Float";
                case PrimitiveType::String: return "String";
                case PrimitiveType::Bool: return "Bool";
                case PrimitiveType::Atom: return "Atom";
                case PrimitiveType::Unit: return "()";
            }
            return "?";
        }
        else if constexpr (std::is_same_v<T, NamedType>) {
            std::string result = t.name;
            if (!t.typeArgs.empty()) {
                result += "<";
                for (size_t i = 0; i < t.typeArgs.size(); i++) {
                    if (i > 0) result += ", ";
                    result += typeToString(t.typeArgs[i]);
                }
                result += ">";
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, FuncType>) {
            std::string result = "(";
            for (size_t i = 0; i < t.params.size(); i++) {
                if (i > 0) result += ", ";
                result += typeToString(t.params[i]);
            }
            result += ") -> " + typeToString(t.result);
            return result;
        }
        else if constexpr (std::is_same_v<T, TupleType>) {
            std::string result = "(";
            for (size_t i = 0; i < t.elements.size(); i++) {
                if (i > 0) result += ", ";
                result += typeToString(t.elements[i]);
            }
            result += ")";
            return result;
        }
        else if constexpr (std::is_same_v<T, ListType>) {
            return "[" + typeToString(t.element) + "]";
        }
        else if constexpr (std::is_same_v<T, MapType>) {
            return "{" + typeToString(t.key) + ": " + typeToString(t.value) + "}";
        }
        else if constexpr (std::is_same_v<T, OptionalType>) {
            return typeToString(t.inner) + "?";
        }
        else if constexpr (std::is_same_v<T, UnionType>) {
            std::string result;
            for (size_t i = 0; i < t.members.size(); i++) {
                if (i > 0) result += " | ";
                result += typeToString(t.members[i]);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, TypeVar>) {
            return "T" + std::to_string(t.id);
        }
        else {
            return "unknown";
        }
    }, type->kind);
}

auto typesEqual(const TypePtr& a, const TypePtr& b) -> bool {
    if (!a || !b) return false;
    if (a.get() == b.get()) return true;

    return std::visit([&b](const auto& at) -> bool {
        using AT = std::decay_t<decltype(at)>;
        auto* bt = std::get_if<AT>(&b->kind);
        if (!bt) return false;

        if constexpr (std::is_same_v<AT, PrimitiveType>) {
            return at.kind == bt->kind;
        }
        else if constexpr (std::is_same_v<AT, NamedType>) {
            if (at.name != bt->name) return false;
            if (at.typeArgs.size() != bt->typeArgs.size()) return false;
            for (size_t i = 0; i < at.typeArgs.size(); i++) {
                if (!typesEqual(at.typeArgs[i], bt->typeArgs[i])) return false;
            }
            return true;
        }
        else if constexpr (std::is_same_v<AT, FuncType>) {
            if (at.params.size() != bt->params.size()) return false;
            for (size_t i = 0; i < at.params.size(); i++) {
                if (!typesEqual(at.params[i], bt->params[i])) return false;
            }
            return typesEqual(at.result, bt->result);
        }
        else if constexpr (std::is_same_v<AT, ListType>) {
            return typesEqual(at.element, bt->element);
        }
        else if constexpr (std::is_same_v<AT, TupleType>) {
            if (at.elements.size() != bt->elements.size()) return false;
            for (size_t i = 0; i < at.elements.size(); i++) {
                if (!typesEqual(at.elements[i], bt->elements[i])) return false;
            }
            return true;
        }
        else if constexpr (std::is_same_v<AT, MapType>) {
            return typesEqual(at.key, bt->key) && typesEqual(at.value, bt->value);
        }
        else if constexpr (std::is_same_v<AT, OptionalType>) {
            return typesEqual(at.inner, bt->inner);
        }
        else if constexpr (std::is_same_v<AT, TypeVar>) {
            return at.id == bt->id;
        }
        else {
            return false;
        }
    }, a->kind);
}

auto TypeEnv::set(const std::string& name, TypePtr type) -> void {
    m_types[name] = std::move(type);
}

auto TypeEnv::get(const std::string& name) const -> TypePtr {
    auto it = m_types.find(name);
    if (it != m_types.end()) return it->second;
    return nullptr;
}

auto TypeEnv::has(const std::string& name) const -> bool {
    return m_types.count(name) > 0;
}

} // namespace kex::semantic
