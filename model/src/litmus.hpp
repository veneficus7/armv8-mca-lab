// A parsed AArch64 litmus test: the shared locations, each thread's code, and the final condition.
//
// The text format is herd's (http://diy.inria.fr/doc/litmus.html), so the tests published with
// Pulte et al. (POPL 2018) and herdtools7's AArch64 catalogue are read as they are.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace mca {

// A register or memory value: a plain integer, or the address of a shared location.
struct Value {
    int loc = -1;     // -1 for an integer; otherwise the index of a shared location
    int64_t v = 0;    // the integer, or the byte offset from that location
    bool isAddress() const { return loc >= 0; }
    bool operator==(const Value&) const = default;
    bool operator<(const Value& o) const { return std::tie(loc, v) < std::tie(o.loc, o.v); }
};

enum class Op {
    Mov, Add, Sub, Eor, And, Orr, Cmp, Csel, Nop,
    B, BCond, Cbz, Cbnz, Label,
    Ldr, Str, Ldar, Ldapr, Stlr,
    Ldxr, Stxr,          // load/store-exclusive (acquire/release forms via the flags below)
    Swp, Ldadd, Cas,     // Armv8.1 atomics (STADD is LDADD with no result register)
    Dmb, Isb,
};

enum class Cond { Eq, Ne };
enum class Barrier { Full, Ld, St };  // dmb.full, dmb.ld, dmb.st in the paper's notation

constexpr int kZeroReg = 31;  // XZR / WZR

// A register-or-immediate operand.
struct Operand {
    bool isReg = false;
    int reg = -1;
    int64_t imm = 0;
};

// [Xn], [Xn,#imm], [Xn,Wm,SXTW], [Xn,Xm]
struct Address {
    int base = -1;
    int index = -1;
    bool indexIsW = false;
    int64_t offset = 0;
};

struct Instr {
    Op op = Op::Nop;
    std::string text;        // as written in the test
    bool w = false;          // 32-bit (W) data registers
    bool acquire = false;    // LDAR, LDAXR, SWPA, LDADDA, CASA ...
    bool acquirePC = false;  // LDAPR (Armv8.3 RCpc)
    bool release = false;    // STLR, STLXR, SWPL, LDADDL, CASL ...
    int rd = -1;             // destination (data processing, loads, the status of a store-exclusive)
    int rs = -1;             // source (atomics' Rs)
    int rt = -1;             // transfer register (loads/stores)
    Operand a, b;            // data-processing operands
    Address addr;
    Cond cond = Cond::Eq;
    Barrier barrier = Barrier::Full;
    std::string label;       // a branch's target, or the name of an Op::Label
};

// A variable the final condition, or a `locations` clause, can name.
struct Var {
    int thread = -1;  // >= 0: a register of that thread
    int reg = -1;
    int loc = -1;     // >= 0: a shared location's final value
    bool operator==(const Var&) const = default;
    bool operator<(const Var& o) const {
        return std::tie(thread, reg, loc) < std::tie(o.thread, o.reg, o.loc);
    }
};

struct CondExpr {
    enum Kind { True, Atom, And, Or, Not } kind = True;
    Var var;
    Value value;                 // the value the atom compares against
    std::vector<CondExpr> kids;
};

enum class Quantifier { Exists, NotExists, Forall };

struct Test {
    std::string name;
    std::vector<std::string> locations;           // shared locations, by index
    std::map<int, int64_t> memInit;               // location -> initial value (default 0)
    std::vector<std::map<int, Value>> regInit;    // per thread: register -> initial value
    std::vector<std::vector<Instr>> threads;
    Quantifier quantifier = Quantifier::Exists;
    CondExpr condition;
    std::vector<Var> observed;                    // what a final state lists, in order

    int locationIndex(const std::string& name);   // finds or adds
    std::string varName(const Var& v) const;      // "1:X0" or "x"
    std::string valueText(const Value& v) const;  // "1", or "x" for an address
};

// Parses a litmus test. On failure returns nothing and says why in `error`.
std::optional<Test> parseLitmus(const std::string& text, std::string& error);

bool evaluate(const CondExpr& e, const std::map<Var, Value>& state);

}  // namespace mca
