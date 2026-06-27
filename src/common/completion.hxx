#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace kex {

// Given a list literal body (the text between '[' and ']'), guess the element
// type name.  Returns "" if the contents are empty or mixed.
inline auto elementTypeOfList(const std::string& contents) -> std::string {
    if (contents.find_first_not_of(" \t,0123456789-") == std::string::npos
            && !contents.empty()) {
        // Only digits, minus, commas, spaces → Integer list
        // Make sure there's at least one digit
        if (contents.find_first_of("0123456789") != std::string::npos)
            return "Integer";
    }
    // Check for float literals  e.g.  1.0, 3.14
    bool allFloat = true;
    std::istringstream ss(contents);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok.erase(0, tok.find_first_not_of(" \t"));
        auto end = tok.find_last_not_of(" \t");
        if (end != std::string::npos) tok = tok.substr(0, end + 1);
        if (tok.empty()) { allFloat = false; break; }
        bool hasDigit = false, hasDot = false;
        for (char c : tok) {
            if (std::isdigit((unsigned char)c)) hasDigit = true;
            else if (c == '.' || c == '-') hasDot = true;
            else { allFloat = false; break; }
        }
        if (!hasDigit) allFloat = false;
    }
    if (allFloat && contents.find('.') != std::string::npos)
        return "Float";
    return "";
}

// Walk backwards through `line` from position `from` over a matching bracket
// pair (character at `from` should be the closing bracket).
// Returns the position of the matching opening bracket, or -1 if not found.
inline auto matchBracketBack(const std::string& line, int from) -> int {
    char close = line[from];
    char open  = (close == ']') ? '[' : (close == ')') ? '(' : '{';
    int depth = 1;
    for (int i = from - 1; i >= 0; i--) {
        if (line[i] == close) depth++;
        else if (line[i] == open) { if (--depth == 0) return i; }
    }
    return -1;
}

// Map the raw token before a dot to a logical type name for DB lookup.
// "[1,2,3]" -> "List", "42" -> "Integer", "'a'" -> "Char",
// "\"hi\"" -> "String", "SomeName" -> "SomeName" (identity).
inline auto qualifierFromBefore(const std::string& before) -> std::string {
    if (before.empty()) return "";
    if (before.back() == ']') return "List";
    if (before.back() == '}') return "Map";
    if (before[0] == '\'' || before[0] == '"') {
        // char vs string literal
        return (before[0] == '\'' && before.size() <= 3) ? "Char" : "String";
    }
    bool allDigits = !before.empty()
        && std::all_of(before.begin(), before.end(),
                       [](char c){ return std::isdigit((unsigned char)c); });
    if (allDigits) return "Integer";
    // Plain identifier / module name
    return before;
}

// Given a line buffer and a potential lambda parameter name, try to infer the
// type of that parameter by examining the surrounding context.
//
// Pattern recognised:
//   ReceiverExpr.method { |param[, ...]| ... }
//
// If ReceiverExpr is:
//   [int, int, ...]  → "Integer"
//   [float, ...]     → "Float"
//   "string"         → "Char"   (iterating chars)
//   Identifier       → the identifier (treated as type name)
//
// Returns "" when the type cannot be determined.
inline auto inferLambdaParamType(const std::string& line,
                                 const std::string& param) -> std::string {
    // Find the rightmost "|param" occurrence
    std::string needle = "|" + param;
    auto pipePos = line.rfind(needle);
    if (pipePos == std::string::npos) return "";

    // Walk back from pipePos to find the '{' that opens this block
    int pos = static_cast<int>(pipePos) - 1;
    while (pos >= 0 && (line[pos] == ' ' || line[pos] == '\t')) pos--;
    if (pos < 0 || line[pos] != '{') return "";

    // Walk back over optional whitespace before '{'
    pos--;
    while (pos >= 0 && (line[pos] == ' ' || line[pos] == '\t')) pos--;
    if (pos < 0) return "";

    // Skip the method name (e.g. "filter", "map", "each")
    while (pos >= 0 && (std::isalnum((unsigned char)line[pos])
                         || line[pos] == '_' || line[pos] == '?' || line[pos] == '!'))
        pos--;
    if (pos < 0 || line[pos] != '.') return "";

    // Now at the dot; the receiver is everything before it
    pos--; // move onto last char of receiver
    if (pos < 0) return "";

    char last = line[pos];

    if (last == ']') {
        int open = matchBracketBack(line, pos);
        if (open < 0) return "List";
        std::string contents = line.substr(static_cast<size_t>(open + 1),
                                           static_cast<size_t>(pos - open - 1));
        std::string elemType = elementTypeOfList(contents);
        return elemType.empty() ? "List" : elemType;
    }

    if (last == '"' || last == '\'') return "Char";

    if (last == ')') return ""; // result of a call — can't infer without types

    // Plain identifier or integer literal — map through qualifierFromBefore so
    // e.g. "234" → "Integer" rather than returning the raw literal.
    int i = pos;
    while (i >= 0 && (std::isalnum((unsigned char)line[i]) || line[i] == '_')) i--;
    std::string raw = line.substr(static_cast<size_t>(i + 1),
                                  static_cast<size_t>(pos - i));
    return qualifierFromBefore(raw);
}

// Infer the type of a function parameter from its role in a pattern match
// within a `make X do ... end` block.
//
// makeTarget  — the type the enclosing `make X` block extends (e.g. "String")
//
// Patterns recognised (in the current line):
//   @[param|...]   → head of cons pattern → "Char" if makeTarget=="String",
//                    otherwise ""
//   @[...|param]   → tail of cons pattern → makeTarget (same type continues)
//   (param)        → simple first param   → makeTarget (the receiver type)
//
// Returns "" when the type cannot be determined.
inline auto inferPatternParamType(const std::string& line, const std::string& param,
                                  const std::string& makeTarget) -> std::string {
    if (makeTarget.empty() || param.empty()) return "";

    // Head of cons: @[param|
    if (line.find("@[" + param + "|") != std::string::npos)
        return (makeTarget == "String") ? "Char" : "";

    // Tail of cons: |param]  (or |param,)
    if (line.find("|" + param + "]") != std::string::npos ||
        line.find("|" + param + ",") != std::string::npos)
        return makeTarget;

    // Simple named parameter: (param)  (param,  , param)  , param,
    if (line.find("(" + param + ")") != std::string::npos ||
        line.find("(" + param + ",") != std::string::npos ||
        line.find(", " + param + ")") != std::string::npos ||
        line.find(", " + param + ",") != std::string::npos)
        return makeTarget;

    return "";
}

// Result of resolving a completion query from the readline context.
struct CompletionQuery {
    std::string dbQuery;     // passed to SemanticDB::completionsFor()
    std::string rewriteFrom; // prefix to remove from each result  (e.g. "List.")
    std::string rewriteTo;   // prefix to insert in its place       (e.g. "[1,2,3].")
};

// Given the raw line buffer and the word readline identified as `text`
// (which may or may not have been split at '.'), return a CompletionQuery
// describing:
//   - what to ask the DB
//   - how to rewrite results so readline inserts the right text
//
// Two scenarios:
//   A. readline DID split on '.':  text="ma", start=8, linebuf="[1,2,3].ma"
//      → detect ']' before dot → CompletionQuery{"List.ma", "List.", ""}
//      → completionsFor("List.ma") = ["List.map"]
//      → rewrite: strip "List.", prepend "" → "map"
//      → readline replaces buffer[8..10] with "map" → "[1,2,3].map" ✓
//
//   B. readline did NOT split:     text="[1,2,3].ma", start=0
//      → find '.' in text, before="]", logical="List"
//      → CompletionQuery{"List.ma", "List.", "[1,2,3]."}
//      → completionsFor("List.ma") = ["List.map"]
//      → rewrite: strip "List.", prepend "[1,2,3]." → "[1,2,3].map"
//      → readline replaces buffer[0..10] with "[1,2,3].map" ✓
inline auto resolveCompletionQuery(const char* linebuf, int start,
                                   const char* text) -> CompletionQuery
{
    std::string textStr(text);

    std::string lineStr(linebuf);

    // Extract the trailing identifier from `before` (the last run of [a-zA-Z0-9_]).
    // e.g. "[1,2,3].filter { |x| x" → "x"
    auto trailingIdent = [](const std::string& before) -> std::string {
        if (before.empty()) return "";
        int i = static_cast<int>(before.size()) - 1;
        while (i >= 0 && (std::isalnum((unsigned char)before[i]) || before[i] == '_'))
            i--;
        return before.substr(static_cast<size_t>(i + 1));
    };

    // Determine the type qualifier given the raw token before the dot.
    // When the token is a plain identifier (or ends with one), check if it is
    // a lambda parameter and try to infer the element type from the enclosing
    // collection.
    auto resolveQualifier = [&](const std::string& before,
                                const std::string& originalDot,
                                const std::string& memberSuffix)
        -> CompletionQuery
    {
        // First try lambda-param inference on the trailing identifier: this
        // must win over qualifierFromBefore because the trailing ident may be
        // a loop variable inside a complex expression like
        //   "hello".each { |c| c      → before = "hello".each { |c| c
        // where qualifierFromBefore would wrongly return "String".
        std::string ident = trailingIdent(before);
        if (!ident.empty() && ident != before
                && std::islower((unsigned char)ident[0])) {
            std::string inferred = inferLambdaParamType(lineStr, ident);
            if (!inferred.empty() && inferred != ident)
                return {inferred + "." + memberSuffix, inferred + ".", originalDot};
        }

        std::string qual = qualifierFromBefore(before);
        // When qualifierFromBefore returns the whole string unchanged (plain ident),
        // also try the trailing-ident lambda inference.
        if (qual == before && !ident.empty()
                && std::islower((unsigned char)ident[0])) {
            std::string inferred = inferLambdaParamType(lineStr, ident);
            if (!inferred.empty() && inferred != ident)
                qual = inferred;
        }
        return {qual + "." + memberSuffix, qual + ".", originalDot};
    };

    // ── Case A: readline split on '.' ──────────────────────────────────────
    if (start > 0 && linebuf[start - 1] == '.') {
        int i = start - 2;
        if (i >= 0 && linebuf[i] == ']') {
            // List literal receiver → "List" (element-type inference is only
            // for lambda parameters, not the collection itself).
            return {"List." + textStr, "List.", ""};
        }
        while (i >= 0 && (std::isalnum((unsigned char)linebuf[i])
                           || linebuf[i] == '_'))
            i--;
        std::string before(linebuf + i + 1, linebuf + start - 1);
        if (!before.empty())
            return resolveQualifier(before, "", textStr);
        return {textStr, "", ""};
    }

    // ── Case B: readline did not split; text may contain '.' ───────────────
    auto dotPos = textStr.rfind('.');
    if (dotPos != std::string::npos) {
        std::string beforeDot   = textStr.substr(0, dotPos);
        std::string afterDot    = textStr.substr(dotPos + 1);
        std::string originalDot = beforeDot + ".";
        return resolveQualifier(beforeDot, originalDot, afterDot);
    }

    // ── No dot at all ───────────────────────────────────────────────────────
    return {textStr, "", ""};
}

// Apply a (rewriteFrom → rewriteTo) substitution on the leading prefix of
// each string in `raw`.  Used to turn DB results into what readline inserts.
inline auto rewriteCompletions(std::vector<std::string> raw,
                                const std::string& rewriteFrom,
                                const std::string& rewriteTo)
    -> std::vector<std::string>
{
    if (rewriteFrom.empty()) return raw;
    std::vector<std::string> out;
    out.reserve(raw.size());
    for (auto& s : raw) {
        if (s.rfind(rewriteFrom, 0) == 0)
            out.push_back(rewriteTo + s.substr(rewriteFrom.size()));
        else
            out.push_back(std::move(s));
    }
    return out;
}

// Convenience: strip a prefix (rewriteTo = "").
inline auto stripCompletionPrefix(std::vector<std::string> raw,
                                  const std::string& prefix)
    -> std::vector<std::string>
{
    return rewriteCompletions(std::move(raw), prefix, "");
}

} // namespace kex
