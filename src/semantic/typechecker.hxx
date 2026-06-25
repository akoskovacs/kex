#pragma once

#include "../ast/ast.hxx"
#include "stdlib_signatures.hxx"
#include "symbol.hxx"
#include "traits.hxx"
#include "types.hxx"
#include <set>
#include <string>
#include <unordered_map>
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

    // ADT registry (sum types with a known, closed constructor set), for
    // match exhaustiveness checking. Populated in a pass over the whole
    // program before any function body is checked, so forward references
    // work the same way `m_globals` already does.
    auto registerAdt(const ast::TypeDef& def) -> void;
    auto registerAdtsInModule(const ast::ModuleDef& mod) -> void;

    // Type alias registry: `type X = <type_expr>` where the RHS is not
    // constructor-shaped (no UpperIdent variants) — the aliased type is
    // expanded inline whenever X appears in a type annotation.
    auto registerTypeAliases(const ast::Program& program) -> void;
    auto registerTypeAliasesInModule(const ast::ModuleDef& mod) -> void;
    auto typeDefToType(const ast::TypeDef& def) -> TypePtr;

    // Standalone type signatures (`fact : Integer -> Integer`) — registered
    // before function bodies are checked so the annotation acts as a declared
    // contract rather than competing with body inference.
    auto registerDeclaredSignatures(const ast::Program& program) -> void;
    auto annotationToSignature(const ast::TypeAnnotation& ann) -> std::optional<Signature>;

    // Pre-register provisional signatures (param types from inline annotations,
    // TypeVar result) for all non-annotation-declared FunctionDefs before any
    // body is checked. This makes recursive and forward-reference calls
    // checkable: a call to `fact` inside `fact`'s own body, or a call to `b`
    // defined after `a`, finds a signature in m_userSignatures.
    auto preRegisterFunctionSigs(const ast::Program& program) -> void;
    auto preRegisterFunctionDef(const ast::FunctionDef& def) -> void;
    auto checkMatchExhaustiveness(const ast::MatchExpr& node, SourceLocation loc) -> void;

    // Defines every variable a pattern introduces (VarPattern, shorthand
    // record fields, nested constructor/list/tuple args) in the current
    // scope as a fresh type var — mirrors how checkFunctionDef already
    // treats untyped params, just recursing into pattern structure.
    auto bindPatternVars(const ast::Pattern& pat) -> void;
    auto checkFunctionDef(const ast::FunctionDef& def) -> void;
    auto checkMakeDef(const ast::MakeDef& def) -> void;
    auto checkMainBlock(const ast::MainBlock& block) -> void;

    // Resolves a parsed `ast::TypeExpr` (param/signature annotation syntax)
    // into a semantic::Type. `genericVars` is scoped to one function clause
    // so repeated occurrences of the same single-letter generic name (e.g.
    // `zip(a : [A], b : [B])`) resolve to the same TypeVar within that
    // clause, not a fresh one per occurrence.
    auto resolveTypeExpr(const ast::TypeExpr& typeExpr,
                        std::unordered_map<std::string, TypePtr>& genericVars) -> TypePtr;

    // Type inference for expressions
    auto inferExpr(const ast::Expr& expr) -> TypePtr;
    auto inferBody(const std::vector<ast::ExprPtr>& body) -> TypePtr;

    // Binary operator type resolution
    auto inferBinaryOp(TokenType op, const TypePtr& left, const TypePtr& right,
                       SourceLocation loc) -> TypePtr;

    // Call checking (FunctionCall and MethodCall, the latter desugared to
    // the same path with the receiver prepended as the first argument).
    // Resolves `name` against the stdlib table, then the user-defined
    // top-level/module-level function table (m_userSignatures) — not
    // make-block methods, whose implicit `this` receiver isn't a regular
    // param, so the "receiver is argument 0" UFCS desugaring used here
    // would mis-count arity for them (see checkMakeDef).
    auto checkCall(const std::string& name, const std::vector<TypePtr>& argTypes,
                   SourceLocation loc) -> TypePtr;
    auto argMatchesParam(const TypePtr& argType, const TypePtr& paramType) const -> bool;
    auto displaySignature(const std::string& name, const Signature& sig) const -> std::string;

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
    TraitRegistry m_traits = TraitRegistry::withBuiltins();
    SignatureTable m_stdlib = SignatureTable::withStdlib();

    // typeName -> constructor names; constructorName -> owning typeName.
    std::unordered_map<std::string, std::vector<std::string>> m_adtVariants;
    std::unordered_map<std::string, std::string> m_adtOfConstructor;

    // Type alias map — populated before function bodies are checked.
    std::unordered_map<std::string, TypePtr> m_typeAliases;

    // Functions whose signatures came from a standalone TypeAnnotation
    // (`fact : Integer -> Integer`) — these are "declared" and checkFunctionDef
    // uses them to validate the body.  Pre-registered provisional sigs (for
    // forward-reference/recursion support) are NOT in this set, so they
    // don't accidentally gate body param-type overrides.
    std::set<std::string> m_annotationDeclared;

    // Per-clause signatures for top-level/module-level user functions,
    // built as each FunctionDef is checked — so a call to a function
    // defined earlier in the file gets real argument checking, the same
    // way the existing forward-reference limit already applies uniformly
    // (a call to a function defined *later*, or a self/mutually-recursive
    // call within a not-yet-finished FunctionDef, isn't in here yet and
    // checkCall falls back to Type::unknown() for it — no regression vs.
    // today, just not yet improved; that's call-graph SCC ordering,
    // phase 5b, not attempted here).
    std::unordered_map<std::string, std::vector<Signature>> m_userSignatures;
    bool m_inMakeBlock = false;

    auto freshTypeVar() -> TypePtr;
};

} // namespace kex::semantic
