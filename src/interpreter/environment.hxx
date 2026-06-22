#pragma once

#include "value.hxx"
#include <memory>
#include <string>
#include <unordered_map>

namespace kex::interpreter {

class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr);

    auto define(const std::string& name, ValuePtr value) -> void;
    auto set(const std::string& name, ValuePtr value) -> bool;
    auto get(const std::string& name) const -> ValuePtr;
    auto has(const std::string& name) const -> bool;
    auto parent() const -> std::shared_ptr<Environment>;

private:
    std::unordered_map<std::string, ValuePtr> m_bindings;
    std::shared_ptr<Environment> m_parent;
};

} // namespace kex::interpreter
