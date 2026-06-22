#include "environment.hxx"

namespace kex::interpreter {

Environment::Environment(std::shared_ptr<Environment> parent)
    : m_parent(std::move(parent)) {}

auto Environment::define(const std::string& name, ValuePtr value) -> void {
    m_bindings[name] = std::move(value);
}

auto Environment::set(const std::string& name, ValuePtr value) -> bool {
    if (m_bindings.count(name)) {
        m_bindings[name] = std::move(value);
        return true;
    }
    if (m_parent) {
        return m_parent->set(name, std::move(value));
    }
    return false;
}

auto Environment::get(const std::string& name) const -> ValuePtr {
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) return it->second;
    if (m_parent) return m_parent->get(name);
    return nullptr;
}

auto Environment::has(const std::string& name) const -> bool {
    if (m_bindings.count(name)) return true;
    if (m_parent) return m_parent->has(name);
    return false;
}

auto Environment::parent() const -> std::shared_ptr<Environment> {
    return m_parent;
}

} // namespace kex::interpreter
