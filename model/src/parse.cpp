// Reading herd's AArch64 litmus format: header, init block, code table, condition.
#include "litmus.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace mca {
namespace {

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Splits on `sep` at bracket depth 0, so "[X7,W5,SXTW]" stays one operand.
std::vector<std::string> splitTop(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    for (char c : s) {
        if (c == '[' || c == '(') depth++;
        if (c == ']' || c == ')') depth--;
        if (c == sep && depth == 0) {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(trim(cur));
    return out;
}

int64_t parseInt(const std::string& raw) {
    std::string s = trim(raw);
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    size_t used = 0;
    int64_t v = std::stoll(s, &used, 0);  // base 0: decimal, 0x hex
    if (used != s.size()) throw ParseError("not a number: " + raw);
    return v;
}

bool looksLikeInt(const std::string& raw) {
    std::string s = trim(raw);
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    if (!s.empty() && s[0] == '-') s = s.substr(1);
    return !s.empty() && std::isdigit(static_cast<unsigned char>(s[0]));
}

// "W5" -> (5, true); "X5" -> (5, false); "WZR"/"XZR" -> (31, ...).
std::pair<int, bool> parseReg(const std::string& raw) {
    std::string s = upper(trim(raw));
    if (s.size() < 2 || (s[0] != 'W' && s[0] != 'X')) throw ParseError("not a register: " + raw);
    bool w = s[0] == 'W';
    if (s.substr(1) == "ZR") return {kZeroReg, w};
    int n = std::stoi(s.substr(1));
    if (n < 0 || n > 30) throw ParseError("register out of range: " + raw);
    return {n, w};
}

Operand parseOperand(const std::string& raw) {
    Operand o;
    if (looksLikeInt(raw)) {
        o.imm = parseInt(raw);
    } else {
        o.isReg = true;
        o.reg = parseReg(raw).first;
    }
    return o;
}

Address parseAddress(const std::string& raw) {
    std::string s = trim(raw);
    if (s.size() < 3 || s.front() != '[' || s.back() != ']') throw ParseError("not an address: " + raw);
    auto parts = splitTop(s.substr(1, s.size() - 2), ',');
    Address a;
    a.base = parseReg(parts[0]).first;
    if (parts.size() >= 2) {
        if (looksLikeInt(parts[1])) {
            a.offset = parseInt(parts[1]);
        } else {
            auto [r, w] = parseReg(parts[1]);
            a.index = r;
            a.indexIsW = w;
        }
    }
    if (parts.size() >= 3) {
        std::string ext = upper(parts[2]);
        // SXTW/UXTW of a non-negative index, or LSL #0, all leave the index as it is.
        if (ext != "SXTW" && ext != "UXTW" && ext != "LSL #0" && ext != "LSL#0")
            throw ParseError("unsupported addressing: " + raw);
    }
    if (parts.size() > 3) throw ParseError("unsupported addressing: " + raw);
    return a;
}

Barrier parseBarrier(const std::string& raw) {
    std::string s = upper(trim(raw));
    if (s == "SY" || s == "ISH" || s == "OSH" || s == "NSH") return Barrier::Full;
    if (s == "LD" || s == "ISHLD" || s == "OSHLD" || s == "NSHLD") return Barrier::Ld;
    if (s == "ST" || s == "ISHST" || s == "OSHST" || s == "NSHST") return Barrier::St;
    throw ParseError("unknown barrier: DMB " + raw);
}

// Atomics come in plain, A (acquire), L (release) and AL forms: SWP, SWPA, SWPL, SWPAL.
bool atomicForm(const std::string& m, const std::string& base, Instr& in) {
    if (m.rfind(base, 0) != 0) return false;
    std::string sfx = m.substr(base.size());
    if (sfx == "") return true;
    if (sfx == "A") { in.acquire = true; return true; }
    if (sfx == "L") { in.release = true; return true; }
    if (sfx == "AL") { in.acquire = in.release = true; return true; }
    return false;
}

Instr parseInstr(const std::string& text) {
    Instr in;
    in.text = trim(text);
    std::string s = in.text;
    // A label on its own: "LC00:"
    if (!s.empty() && s.back() == ':') {
        in.op = Op::Label;
        in.label = s.substr(0, s.size() - 1);
        return in;
    }
    auto sp = s.find_first_of(" \t");
    std::string m = upper(s.substr(0, sp));
    std::vector<std::string> ops;
    if (sp != std::string::npos) ops = splitTop(s.substr(sp + 1), ',');
    auto need = [&](size_t n) {
        if (ops.size() != n) throw ParseError("expected " + std::to_string(n) + " operands: " + text);
    };
    auto dataReg = [&](const std::string& r) {
        auto [n, w] = parseReg(r);
        in.w = w;
        return n;
    };

    if (m == "MOV") {
        need(2); in.op = Op::Mov; in.rd = dataReg(ops[0]); in.a = parseOperand(ops[1]);
    } else if (m == "ADD" || m == "SUB" || m == "EOR" || m == "AND" || m == "ORR") {
        need(3);
        in.op = m == "ADD" ? Op::Add : m == "SUB" ? Op::Sub : m == "EOR" ? Op::Eor : m == "AND" ? Op::And : Op::Orr;
        in.rd = dataReg(ops[0]); in.a = parseOperand(ops[1]); in.b = parseOperand(ops[2]);
    } else if (m == "CMP") {
        need(2); in.op = Op::Cmp; in.a = parseOperand(ops[0]); in.b = parseOperand(ops[1]);
    } else if (m == "CSEL") {
        need(4); in.op = Op::Csel; in.rd = dataReg(ops[0]); in.a = parseOperand(ops[1]); in.b = parseOperand(ops[2]);
        std::string c = upper(ops[3]);
        if (c == "EQ") in.cond = Cond::Eq; else if (c == "NE") in.cond = Cond::Ne; else throw ParseError("unsupported condition: " + text);
    } else if (m == "NOP") {
        in.op = Op::Nop;
    } else if (m == "B") {
        need(1); in.op = Op::B; in.label = ops[0];
    } else if (m == "B.EQ" || m == "BEQ" || m == "B.NE" || m == "BNE") {
        need(1); in.op = Op::BCond; in.label = ops[0];
        in.cond = (m == "B.EQ" || m == "BEQ") ? Cond::Eq : Cond::Ne;
    } else if (m == "CBZ" || m == "CBNZ") {
        need(2); in.op = m == "CBZ" ? Op::Cbz : Op::Cbnz; in.rt = dataReg(ops[0]); in.label = ops[1];
    } else if (m == "LDR" || m == "LDAR" || m == "LDAPR" || m == "LDXR" || m == "LDAXR") {
        need(2);
        in.op = m == "LDR" ? Op::Ldr : m == "LDAR" ? Op::Ldar : m == "LDAPR" ? Op::Ldapr : Op::Ldxr;
        in.acquire = (m == "LDAR" || m == "LDAXR");
        in.acquirePC = (m == "LDAPR");
        in.rt = dataReg(ops[0]); in.addr = parseAddress(ops[1]);
    } else if (m == "STR" || m == "STLR") {
        if (ops.size() == 3) throw ParseError("post-indexed addressing is not supported: " + text);
        need(2); in.op = m == "STR" ? Op::Str : Op::Stlr; in.release = (m == "STLR");
        in.rt = dataReg(ops[0]); in.addr = parseAddress(ops[1]);
    } else if (m == "STXR" || m == "STLXR") {
        need(3); in.op = Op::Stxr; in.release = (m == "STLXR");
        in.rd = parseReg(ops[0]).first; in.rt = dataReg(ops[1]); in.addr = parseAddress(ops[2]);
    } else if (atomicForm(m, "SWP", in) || atomicForm(m, "LDADD", in) || atomicForm(m, "CAS", in)) {
        need(3);
        in.op = m.rfind("SWP", 0) == 0 ? Op::Swp : m.rfind("LDADD", 0) == 0 ? Op::Ldadd : Op::Cas;
        in.rs = dataReg(ops[0]); in.rt = dataReg(ops[1]); in.addr = parseAddress(ops[2]);
    } else if (atomicForm(m, "STADD", in)) {
        need(2); in.op = Op::Ldadd; in.rs = dataReg(ops[0]); in.rt = kZeroReg; in.addr = parseAddress(ops[1]);
        in.acquire = false;  // STADD has no acquire form
    } else if (m == "DMB") {
        need(1); in.op = Op::Dmb; in.barrier = parseBarrier(ops[0]);
    } else if (m == "ISB") {
        in.op = Op::Isb;
    } else {
        throw ParseError("unsupported instruction: " + text);
    }
    return in;
}

// --- the final condition ---------------------------------------------------------------------

struct CondParser {
    Test& test;
    std::string s;
    size_t i = 0;

    void skip() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++; }
    bool eat(const std::string& tok) {
        skip();
        if (s.compare(i, tok.size(), tok) == 0) { i += tok.size(); return true; }
        return false;
    }

    CondExpr parseOr() {
        CondExpr left = parseAnd();
        while (eat("\\/")) {
            CondExpr e; e.kind = CondExpr::Or; e.kids = {left, parseAnd()}; left = e;
        }
        return left;
    }
    CondExpr parseAnd() {
        CondExpr left = parseUnary();
        while (eat("/\\")) {
            CondExpr e; e.kind = CondExpr::And; e.kids = {left, parseUnary()}; left = e;
        }
        return left;
    }
    CondExpr parseUnary() {
        if (eat("~") || eat("not ")) {
            CondExpr e; e.kind = CondExpr::Not; e.kids = {parseUnary()}; return e;
        }
        if (eat("(")) {
            CondExpr e = parseOr();
            if (!eat(")")) throw ParseError("expected ) in condition");
            return e;
        }
        if (eat("true")) return CondExpr{};
        return parseAtom();
    }
    CondExpr parseAtom() {
        skip();
        size_t start = i;
        while (i < s.size() && s[i] != '=' && s[i] != ')' && !std::isspace(static_cast<unsigned char>(s[i]))) i++;
        std::string lhs = s.substr(start, i - start);
        if (!eat("=")) throw ParseError("expected = in condition near " + lhs);
        skip();
        start = i;
        while (i < s.size() && s[i] != ')' && s[i] != '/' && s[i] != '\\' && !std::isspace(static_cast<unsigned char>(s[i]))) i++;
        std::string rhs = s.substr(start, i - start);

        CondExpr e;
        e.kind = CondExpr::Atom;
        e.var = parseVar(test, lhs);
        if (looksLikeInt(rhs)) e.value.v = parseInt(rhs);
        else e.value.loc = test.locationIndex(rhs);
        return e;
    }

    static Var parseVar(Test& t, std::string name) {
        name = trim(name);
        if (name.size() > 2 && name.front() == '[' && name.back() == ']') name = name.substr(1, name.size() - 2);
        Var v;
        auto colon = name.find(':');
        if (colon != std::string::npos) {
            v.thread = std::stoi(name.substr(0, colon));
            v.reg = parseReg(name.substr(colon + 1)).first;
        } else {
            v.loc = t.locationIndex(name);
        }
        return v;
    }
};

void addObserved(Test& t, const Var& v) {
    if (std::find(t.observed.begin(), t.observed.end(), v) == t.observed.end()) t.observed.push_back(v);
}

void collectVars(Test& t, const CondExpr& e) {
    if (e.kind == CondExpr::Atom) addObserved(t, e.var);
    for (auto& k : e.kids) collectVars(t, k);
}

}  // namespace

int Test::locationIndex(const std::string& raw) {
    std::string name = trim(raw);
    auto it = std::find(locations.begin(), locations.end(), name);
    if (it != locations.end()) return static_cast<int>(it - locations.begin());
    locations.push_back(name);
    return static_cast<int>(locations.size()) - 1;
}

std::string Test::varName(const Var& v) const {
    if (v.loc >= 0) return locations[v.loc];
    return std::to_string(v.thread) + ":X" + std::to_string(v.reg);
}

std::string Test::valueText(const Value& v) const {
    if (v.isAddress()) return v.v == 0 ? locations[v.loc] : locations[v.loc] + "+" + std::to_string(v.v);
    return std::to_string(v.v);
}

bool evaluate(const CondExpr& e, const std::map<Var, Value>& state) {
    switch (e.kind) {
        case CondExpr::True: return true;
        case CondExpr::Atom: {
            auto it = state.find(e.var);
            return it != state.end() && it->second == e.value;
        }
        case CondExpr::And: return evaluate(e.kids[0], state) && evaluate(e.kids[1], state);
        case CondExpr::Or: return evaluate(e.kids[0], state) || evaluate(e.kids[1], state);
        case CondExpr::Not: return !evaluate(e.kids[0], state);
    }
    return false;
}

// herd comments are (* ... *) and may sit inside a code cell.
std::string stripComments(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s.compare(i, 2, "(*") == 0) {
            auto end = s.find("*)", i + 2);
            if (end == std::string::npos) break;
            i = end + 1;
            continue;
        }
        out += s[i];
    }
    return out;
}

std::optional<Test> parseLitmus(const std::string& rawText, std::string& error) {
    try {
        Test t;
        std::string text = stripComments(rawText);
        std::istringstream in(text);
        std::string line;

        // Header: "AArch64 NAME", then free-form lines up to the init block.
        while (std::getline(in, line) && trim(line).empty()) {}
        {
            std::istringstream h(line);
            std::string arch;
            h >> arch >> t.name;
            if (arch != "AArch64") throw ParseError("not an AArch64 test: " + arch);
        }
        std::string rest((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto open = rest.find('{');
        auto close = rest.find('}', open);
        if (open == std::string::npos || close == std::string::npos) throw ParseError("no init block");

        // Init block: "0:X1=x; 1:X3=y; x=1; int z=2;"
        std::vector<std::pair<int, std::pair<int, std::string>>> regInits;  // thread, reg, value text
        for (auto entry : splitTop(rest.substr(open + 1, close - open - 1), ';')) {
            entry = trim(entry);
            if (entry.empty()) continue;
            auto eq = entry.find('=');
            std::string lhs = trim(entry.substr(0, eq == std::string::npos ? entry.size() : eq));
            std::string rhs = eq == std::string::npos ? "0" : trim(entry.substr(eq + 1));
            if (lhs.find(':') != std::string::npos) {
                lhs.erase(std::remove_if(lhs.begin(), lhs.end(), [](unsigned char c) { return std::isspace(c); }), lhs.end());
            } else {
                auto lastSpace = lhs.find_last_of(" \t");  // "int x" or "uint64_t x"
                if (lastSpace != std::string::npos) lhs = lhs.substr(lastSpace + 1);
            }
            auto colon = lhs.find(':');
            if (colon != std::string::npos) {
                regInits.push_back({std::stoi(lhs.substr(0, colon)), {parseReg(lhs.substr(colon + 1)).first, rhs}});
            } else {
                int loc = t.locationIndex(lhs);
                if (looksLikeInt(rhs)) t.memInit[loc] = parseInt(rhs);
                else throw ParseError("unsupported initial value: " + entry);
            }
        }

        // Code table, up to the line that starts the condition.
        std::istringstream body(rest.substr(close + 1));
        std::vector<std::string> codeLines;
        std::string condText;
        bool inCond = false;
        while (std::getline(body, line)) {
            std::string tl = trim(line);
            if (!inCond && (tl.rfind("exists", 0) == 0 || tl.rfind("~exists", 0) == 0 ||
                            tl.rfind("forall", 0) == 0 || tl.rfind("locations", 0) == 0 ||
                            tl.rfind("filter", 0) == 0)) {
                inCond = true;
            }
            if (inCond) condText += " " + tl;
            else if (!tl.empty()) codeLines.push_back(tl);
        }
        if (codeLines.empty()) throw ParseError("no code");
        auto header = splitTop(codeLines[0].substr(0, codeLines[0].rfind(';')), '|');
        size_t nThreads = header.size();
        t.threads.resize(nThreads);
        t.regInit.resize(nThreads);
        for (size_t k = 1; k < codeLines.size(); k++) {
            std::string l = codeLines[k];
            if (l.back() == ';') l.pop_back();
            auto cells = splitTop(l, '|');
            for (size_t th = 0; th < cells.size() && th < nThreads; th++) {
                std::string c = trim(cells[th]);
                if (c.empty()) continue;
                // "LC00: MOV W0,#1" carries a label and an instruction.
                auto colon = c.find(':');
                if (colon != std::string::npos && c.find('[') == std::string::npos && colon + 1 < c.size()) {
                    t.threads[th].push_back(parseInstr(c.substr(0, colon + 1)));
                    c = trim(c.substr(colon + 1));
                }
                t.threads[th].push_back(parseInstr(c));
            }
        }

        for (auto& [th, rv] : regInits) {
            if (th < 0 || th >= static_cast<int>(nThreads)) throw ParseError("init for a thread that does not exist");
            Value v;
            if (looksLikeInt(rv.second)) v.v = parseInt(rv.second);
            else v.loc = t.locationIndex(rv.second);
            t.regInit[th][rv.first] = v;
        }

        // Condition: optional "locations [a; 1:X0;]", then exists / ~exists / forall (expr).
        condText = trim(condText);
        if (condText.rfind("locations", 0) == 0) {
            auto lb = condText.find('['), rb = condText.find(']');
            for (auto v : splitTop(condText.substr(lb + 1, rb - lb - 1), ';')) {
                if (!trim(v).empty()) addObserved(t, CondParser::parseVar(t, v));
            }
            condText = trim(condText.substr(rb + 1));
        }
        if (condText.rfind("filter", 0) == 0) throw ParseError("filter clauses are not supported");
        std::string expr;
        if (condText.rfind("~exists", 0) == 0) { t.quantifier = Quantifier::NotExists; expr = condText.substr(7); }
        else if (condText.rfind("exists", 0) == 0) { t.quantifier = Quantifier::Exists; expr = condText.substr(6); }
        else if (condText.rfind("forall", 0) == 0) { t.quantifier = Quantifier::Forall; expr = condText.substr(6); }
        else throw ParseError("no final condition");
        CondParser cp{t, expr};
        t.condition = cp.parseOr();
        collectVars(t, t.condition);
        return t;
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

}  // namespace mca
