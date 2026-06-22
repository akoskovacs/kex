#pragma once

#include "../ast/ast.hxx"
#include "symbol.hxx"
#include "types.hxx"
#include <string>
#include <vector>

namespace kex::semantic {

struct Diagnostic;

class TypeChecker {
public:
    auto check(const ast::Program& program, std::vector<Diagnostic>& diagnostics) -> void;

private:
    // Top-level
    auto checkTopLevel(const ast::TopLevelItem& item) -> void;
    auto checkModule(const ast::ModuleDef& mod) -> void;
    auto checkFunctionDef(const ast::FunctionDef& def) -> void;
    auto checkMakeDef(const ast::MakeDef& def) -> void;
    auto checkMainBlock(const ast::MainBlock& block) -> void;

    // Type inference for expressions
    auto inferExpr(const ast::Expr& expr) -> TypePtr;
    auto inferBody(const std::vector<ast::ExprPtr>& body) -> TypePtr;

    // Binary operator type resolution
    auto inferBinaryOp(TokenType op, const TypePtr& left, const TypePtr& right,
                       SourceLocation loc) -> TypePtr;

    // Scope management
    auto pushScope() -> void;
    auto popScope() -> void;
    auto defineVar(const std::string& name, TypePtr type) -> void;
    auto lookupVar(const std::string& name) const -> TypePtr;

    // Error reporting
    auto error(SourceLocation loc, const std::string& msg) -> void;
    auto typeMismatch(SourceLocation loc, const TypePtr& expected, const TypePtr& actual) -> void;

    std::vector<Diagnostic>* m_diagnostics = nullptr;
    TypeEnv m_globals;
    std::vector<TypeEnv> m_scopeStack;
    int m_nextTypeVar = 0;

    auto freshTypeVar() -> TypePtr;
};

} // namespace kex::semantic
