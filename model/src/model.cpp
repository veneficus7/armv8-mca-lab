// Candidate executions of a litmus test, and the MCA ARMv8 axioms that judge them.
//
// 1. Each thread is run on its own, forking at every read over each value that location could
//    hold. A register remembers which reads its value came from (its "taint"), which is how
//    addr, data and ctrl dependencies are found: syntactically, as the architecture defines them.
// 2. The possible values of each location are found by iterating 1 to a fixpoint.
// 3. Every combination of thread paths is joined with every reads-from (rf) and coherence (co)
//    choice that agrees with the values, giving the candidate executions.
// 4. Each candidate is kept if it satisfies the paper's three axioms (§7, page 19:19-20).
#include "model.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <stdexcept>

namespace mca {
namespace {

struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

constexpr int kMaxEvents = 64;  // relations are 64-bit rows
constexpr int kMaxSteps = 512;  // a thread that runs longer than this is looping

bool valueLess(const Value& a, const Value& b) { return std::tie(a.loc, a.v) < std::tie(b.loc, b.v); }
struct ValueLess {
    bool operator()(const Value& a, const Value& b) const { return valueLess(a, b); }
};
using Domains = std::vector<std::set<Value, ValueLess>>;

// ---- 1. running one thread ---------------------------------------------------------------------

struct Reg {
    Value v;
    uint64_t taint = 0;  // bit i: depends on this thread's i-th event, which is a read
};

struct Path {
    std::vector<Event> events;
    std::array<Value, 32> regs{};
};

struct ThreadRunner {
    const Test& test;
    int th;
    const Domains& domains;
    std::map<std::string, size_t> labels;
    std::vector<Path>& out;

    struct S {
        std::array<Reg, 32> regs{};
        Reg flags;             // Z, as 1 or 0, from the last CMP
        uint64_t ctrl = 0;     // reads that some branch before here depended on
        size_t pc = 0;
        int steps = 0;
        std::vector<Event> events;
        int monitorRead = -1;  // the load-exclusive a store-exclusive may pair with
        int monitorLoc = -1;
    };

    Reg get(const S& s, int r, bool w) const {
        if (r == kZeroReg) return {};
        Reg x = s.regs[r];
        if (w && !x.v.isAddress()) x.v.v &= 0xffffffffLL;
        return x;
    }
    static void set(S& s, int r, Reg x, bool w) {
        if (r == kZeroReg) return;
        if (w && !x.v.isAddress()) x.v.v &= 0xffffffffLL;
        s.regs[r] = x;
    }
    Reg operand(const S& s, const Operand& o, bool w) const {
        if (o.isReg) return get(s, o.reg, w);
        return Reg{Value{-1, o.imm}, 0};
    }

    static Value arith(Op op, Value a, Value b, const Instr& in) {
        if (!a.isAddress() && !b.isAddress()) {
            int64_t r = 0;
            switch (op) {
                case Op::Add: r = a.v + b.v; break;
                case Op::Sub: r = a.v - b.v; break;
                case Op::Eor: r = a.v ^ b.v; break;
                case Op::And: r = a.v & b.v; break;
                case Op::Orr: r = a.v | b.v; break;
                default: break;
            }
            return Value{-1, r};
        }
        if ((op == Op::Eor || op == Op::Sub) && a == b) return Value{-1, 0};
        if (op == Op::Add && a.isAddress() && !b.isAddress()) return Value{a.loc, a.v + b.v};
        if (op == Op::Add && b.isAddress() && !a.isAddress()) return Value{b.loc, b.v + a.v};
        if (op == Op::Sub && a.isAddress() && !b.isAddress()) return Value{a.loc, a.v - b.v};
        throw Unsupported("arithmetic on an address: " + in.text);
    }

    // The location an instruction accesses, and the reads its address depends on.
    std::pair<int, uint64_t> address(const S& s, const Instr& in) const {
        Reg base = get(s, in.addr.base, false);
        Value a = base.v;
        uint64_t taint = base.taint;
        if (in.addr.index >= 0) {
            Reg idx = get(s, in.addr.index, in.addr.indexIsW);
            int64_t i = idx.v.v;
            if (idx.v.isAddress()) throw Unsupported("an address used as an index: " + in.text);
            if (in.addr.indexIsW) i = static_cast<int32_t>(i);  // SXTW
            a.v += i;
            taint |= idx.taint;
        }
        a.v += in.addr.offset;
        if (!a.isAddress() || a.v != 0)
            throw Unsupported("an access that is not exactly a shared location: " + in.text);
        return {a.loc, taint};
    }

    int push(S& s, Event e) {
        if (static_cast<int>(s.events.size()) >= kMaxEvents) throw Unsupported("too many events");
        e.thread = th;
        e.ctrl = s.ctrl;
        s.events.push_back(e);
        return static_cast<int>(s.events.size()) - 1;
    }

    Event memEvent(Event::Kind k, int loc, uint64_t addrTaint, const Instr& in) const {
        Event e;
        e.kind = k;
        e.loc = loc;
        e.addr = addrTaint;
        e.text = in.text;
        return e;
    }

    void jump(S& s, const std::string& label) {
        auto it = labels.find(label);
        if (it == labels.end()) throw Unsupported("no such label: " + label);
        if (it->second <= s.pc) throw Unsupported("backward branch (a loop): " + label);
        s.pc = it->second;
    }

    void run(S s) {
        const auto& code = test.threads[th];
        while (s.pc < code.size()) {
            if (++s.steps > kMaxSteps) throw Unsupported("thread does not terminate");
            const Instr& in = code[s.pc];
            switch (in.op) {
                case Op::Label:
                case Op::Nop:
                    s.pc++;
                    break;
                case Op::Mov:
                    set(s, in.rd, operand(s, in.a, in.w), in.w);
                    s.pc++;
                    break;
                case Op::Add: case Op::Sub: case Op::Eor: case Op::And: case Op::Orr: {
                    Reg a = operand(s, in.a, in.w), b = operand(s, in.b, in.w);
                    set(s, in.rd, Reg{arith(in.op, a.v, b.v, in), a.taint | b.taint}, in.w);
                    s.pc++;
                    break;
                }
                case Op::Cmp: {
                    Reg a = operand(s, in.a, in.w), b = operand(s, in.b, in.w);
                    s.flags = Reg{Value{-1, a.v == b.v ? 1 : 0}, a.taint | b.taint};
                    s.pc++;
                    break;
                }
                case Op::Csel: {
                    // A data dependency through the flags, not a control dependency: no branch.
                    bool z = s.flags.v.v == 1;
                    bool pick = in.cond == Cond::Eq ? z : !z;
                    Reg a = operand(s, in.a, in.w), b = operand(s, in.b, in.w);
                    Reg r = pick ? a : b;
                    r.taint = a.taint | b.taint | s.flags.taint;
                    set(s, in.rd, r, in.w);
                    s.pc++;
                    break;
                }
                case Op::B:
                    jump(s, in.label);
                    break;
                case Op::BCond: case Op::Cbz: case Op::Cbnz: {
                    bool taken;
                    if (in.op == Op::BCond) {
                        bool z = s.flags.v.v == 1;
                        taken = in.cond == Cond::Eq ? z : !z;
                        s.ctrl |= s.flags.taint;
                    } else {
                        Reg r = get(s, in.rt, in.w);
                        bool zero = !r.v.isAddress() && r.v.v == 0;
                        taken = in.op == Op::Cbz ? zero : !zero;
                        s.ctrl |= r.taint;
                    }
                    // Every event after a conditional branch is control-dependent on what it read,
                    // whichever way it went.
                    if (taken) jump(s, in.label); else s.pc++;
                    break;
                }
                case Op::Dmb: {
                    Event e;
                    e.kind = Event::Fence;
                    e.barrier = in.barrier;
                    e.text = in.text;
                    push(s, e);
                    s.pc++;
                    break;
                }
                case Op::Isb: {
                    Event e;
                    e.kind = Event::Fence;
                    e.isb = true;
                    e.text = in.text;
                    push(s, e);
                    s.pc++;
                    break;
                }
                case Op::Str: case Op::Stlr: {
                    auto [loc, at] = address(s, in);
                    Reg d = get(s, in.rt, in.w);
                    Event e = memEvent(Event::Write, loc, at, in);
                    e.val = d.v;
                    e.data = d.taint;
                    e.release = in.release;
                    push(s, e);
                    s.pc++;
                    break;
                }
                case Op::Ldr: case Op::Ldar: case Op::Ldapr: case Op::Ldxr: {
                    auto [loc, at] = address(s, in);
                    for (const Value& v : domains[loc]) {  // fork: every value the location could hold
                        S n = s;
                        Event e = memEvent(Event::Read, loc, at, in);
                        e.val = v;
                        e.acquire = in.acquire;
                        e.acquirePC = in.acquirePC;
                        int idx = push(n, e);
                        set(n, in.rt, Reg{v, 1ull << idx}, in.w);
                        if (in.op == Op::Ldxr) { n.monitorRead = idx; n.monitorLoc = loc; }
                        n.pc++;
                        run(n);
                    }
                    return;
                }
                case Op::Stxr: {
                    auto [loc, at] = address(s, in);
                    {   // it fails: status 1, no write
                        S n = s;
                        set(n, in.rd, Reg{Value{-1, 1}, 0}, true);
                        n.monitorRead = n.monitorLoc = -1;
                        n.pc++;
                        run(n);
                    }
                    if (s.monitorRead >= 0 && s.monitorLoc == loc) {  // it succeeds: paired with the load
                        S n = s;
                        Reg d = get(n, in.rt, in.w);
                        Event e = memEvent(Event::Write, loc, at, in);
                        e.val = d.v;
                        e.data = d.taint;
                        e.release = in.release;
                        e.rmwRead = n.monitorRead;
                        push(n, e);
                        set(n, in.rd, Reg{Value{-1, 0}, 0}, true);
                        n.monitorRead = n.monitorLoc = -1;
                        n.pc++;
                        run(n);
                    }
                    return;
                }
                case Op::Swp: case Op::Ldadd: case Op::Cas: {
                    auto [loc, at] = address(s, in);
                    for (const Value& old : domains[loc]) {
                        S n = s;
                        Event r = memEvent(Event::Read, loc, at, in);
                        r.val = old;
                        r.acquire = in.acquire;
                        int ri = push(n, r);
                        Reg rs = get(n, in.rs, in.w);
                        Reg rt = get(n, in.rt, in.w);
                        bool writes = true;
                        Event w = memEvent(Event::Write, loc, at, in);
                        w.release = in.release;
                        w.rmwRead = ri;
                        if (in.op == Op::Swp) {
                            w.val = rs.v;
                            w.data = rs.taint;
                        } else if (in.op == Op::Ldadd) {
                            w.val = arith(Op::Add, old, rs.v, in);
                            if (in.w && !w.val.isAddress()) w.val.v &= 0xffffffffLL;
                            w.data = rs.taint | (1ull << ri);
                        } else {  // CAS: writes Rt only if memory held Rs
                            writes = (old == rs.v);
                            w.val = rt.v;
                            // Only Rt is a data dependency. Adding Rs (the comparison) was tried: it
                            // turned LB+rel+CAS to Forbidden as herd's kinds.txt says, and CAS+data2
                            // and MP+rel+CAS-addr to Forbidden where kinds.txt says Allowed. CAS's
                            // register dependencies are settled beyond the 2018 paper (docs/FINDINGS.md).
                            w.data = rt.taint;
                        }
                        if (writes) push(n, w);
                        Reg result{old, 1ull << ri};
                        if (in.op == Op::Cas) set(n, in.rs, result, in.w);
                        else set(n, in.rt, result, in.w);
                        n.pc++;
                        run(n);
                    }
                    return;
                }
            }
        }
        Path p;
        p.events = std::move(s.events);
        for (int r = 0; r < 32; r++) p.regs[r] = s.regs[r].v;
        out.push_back(std::move(p));
    }
};

std::vector<Path> runThread(const Test& test, int th, const Domains& domains) {
    std::vector<Path> out;
    ThreadRunner tr{test, th, domains, {}, out};
    const auto& code = test.threads[th];
    for (size_t i = 0; i < code.size(); i++)
        if (code[i].op == Op::Label) tr.labels[code[i].label] = i;
    ThreadRunner::S s;
    for (auto& [reg, v] : test.regInit[th]) s.regs[reg] = Reg{v, 0};
    tr.run(s);
    return out;
}

// ---- 2. the values each location can hold -----------------------------------------------------

Value initialValue(const Test& t, int loc) {
    auto it = t.memInit.find(loc);
    return Value{-1, it == t.memInit.end() ? 0 : it->second};
}

// Round k finds every value made by a chain of k writes, each reading what the one before wrote.
// No execution has more writes than the program has write instructions, so that many rounds find
// every value any execution can read, even when the domain itself keeps growing (LDADD feeding
// LDADD). Extra values are harmless: a read of a value nobody wrote has no rf source.
Domains valueDomains(const Test& t) {
    Domains d(t.locations.size());
    for (size_t l = 0; l < t.locations.size(); l++) d[l].insert(initialValue(t, static_cast<int>(l)));
    int writeInstrs = 0;
    for (const auto& code : t.threads)
        for (const Instr& in : code)
            if (in.op == Op::Str || in.op == Op::Stlr || in.op == Op::Stxr || in.op == Op::Swp ||
                in.op == Op::Ldadd || in.op == Op::Cas)
                writeInstrs++;
    for (int round = 0; round <= writeInstrs; round++) {
        Domains next = d;
        for (size_t th = 0; th < t.threads.size(); th++)
            for (const Path& p : runThread(t, static_cast<int>(th), d))
                for (const Event& e : p.events)
                    if (e.kind == Event::Write) next[e.loc].insert(e.val);
        for (auto& s : next)
            if (s.size() > 32) throw Unsupported("too many distinct values");
        if (next == d) break;
        d = std::move(next);
    }
    return d;
}

// ---- 3. relations ------------------------------------------------------------------------------

using Set = uint64_t;

struct Rel {
    int n = 0;
    std::array<uint64_t, kMaxEvents> row{};
    void add(int i, int j) { row[i] |= 1ull << j; }
    bool has(int i, int j) const { return (row[i] >> j) & 1; }
};

Rel operator|(Rel a, const Rel& b) { for (int i = 0; i < a.n; i++) a.row[i] |= b.row[i]; return a; }
Rel operator&(Rel a, const Rel& b) { for (int i = 0; i < a.n; i++) a.row[i] &= b.row[i]; return a; }
Rel seq(const Rel& a, const Rel& b) {  // a;b
    Rel c{a.n, {}};
    for (int i = 0; i < a.n; i++)
        for (uint64_t m = a.row[i]; m; m &= m - 1) c.row[i] |= b.row[__builtin_ctzll(m)];
    return c;
}
Rel dom(Set s, Rel a) {  // [S];a
    for (int i = 0; i < a.n; i++) if (!((s >> i) & 1)) a.row[i] = 0;
    return a;
}
Rel ran(Rel a, Set s) {  // a;[S]
    for (int i = 0; i < a.n; i++) a.row[i] &= s;
    return a;
}
Rel inverse(const Rel& a) {
    Rel c{a.n, {}};
    for (int i = 0; i < a.n; i++)
        for (uint64_t m = a.row[i]; m; m &= m - 1) c.add(__builtin_ctzll(m), i);
    return c;
}
bool acyclic(Rel a) {  // the transitive closure has no i -> i
    for (int k = 0; k < a.n; k++)
        for (int i = 0; i < a.n; i++)
            if ((a.row[i] >> k) & 1) a.row[i] |= a.row[k];
    for (int i = 0; i < a.n; i++) if ((a.row[i] >> i) & 1) return false;
    return true;
}
bool empty(const Rel& a) {
    for (int i = 0; i < a.n; i++) if (a.row[i]) return false;
    return true;
}

// ---- 4. joining the threads, and the axioms ----------------------------------------------------

struct Checker {
    Checker(const Test& t, const Axioms& a, Result& r) : test(t), axioms(a), result(r) {}
    const Test& test;
    const Axioms& axioms;
    Result& result;

    std::vector<Event> ev;            // initial writes first, then each thread's events
    std::vector<int> first;           // index of each thread's first event in ev
    std::vector<const Path*> paths;   // the path chosen for each thread

    // Relations that do not depend on rf or co
    Rel po, sameLoc, ext, addr, data, ctrl, rmw;
    Set R = 0, W = 0, A = 0, Q = 0, L = 0, Ffull = 0, Fld = 0, Fst = 0, ISB = 0, rangeRmw = 0;
    std::vector<std::vector<int>> rfChoices;  // per read: the writes it could read from
    std::vector<int> readIds;
    std::vector<std::vector<int>> writesByLoc;

    void prepare() {
        int n = static_cast<int>(ev.size());
        for (Rel* r : {&po, &sameLoc, &ext, &addr, &data, &ctrl, &rmw}) *r = Rel{n, {}};
        R = W = A = Q = L = Ffull = Fld = Fst = ISB = rangeRmw = 0;
        for (int i = 0; i < n; i++) {
            const Event& e = ev[i];
            Set bit = 1ull << i;
            if (e.kind == Event::Read) R |= bit;
            if (e.kind == Event::Write) W |= bit;
            if (e.kind == Event::Read && e.acquire) A |= bit;
            if (e.kind == Event::Read && e.acquirePC) Q |= bit;
            if (e.kind == Event::Write && e.release) L |= bit;
            if (e.kind == Event::Fence) {
                if (e.isb) ISB |= bit;
                else if (e.barrier == Barrier::Full) Ffull |= bit;
                else if (e.barrier == Barrier::Ld) Fld |= bit;
                else Fst |= bit;
            }
            for (int j = 0; j < n; j++) {
                const Event& f = ev[j];
                if (i != j && e.thread != f.thread) ext.add(i, j);
                if (e.thread >= 0 && e.thread == f.thread && i < j) po.add(i, j);
                if (e.kind != Event::Fence && f.kind != Event::Fence && e.loc == f.loc) sameLoc.add(i, j);
            }
            if (e.thread >= 0) {
                int base = first[e.thread];
                auto from = [&](uint64_t m, Rel& rel) {
                    for (; m; m &= m - 1) rel.add(base + __builtin_ctzll(m), i);
                };
                from(e.addr, addr);
                from(e.data, data);
                from(e.ctrl, ctrl);
                if (e.rmwRead >= 0) { rmw.add(base + e.rmwRead, i); rangeRmw |= bit; }
            }
        }
        readIds.clear();
        rfChoices.clear();
        writesByLoc.assign(test.locations.size(), {});
        for (int i = 0; i < n; i++) {
            if (ev[i].kind == Event::Write && ev[i].thread >= 0) writesByLoc[ev[i].loc].push_back(i);
            if (ev[i].kind != Event::Read) continue;
            std::vector<int> srcs;
            for (int j = 0; j < n; j++)
                if (ev[j].kind == Event::Write && ev[j].loc == ev[i].loc && ev[j].val == ev[i].val) srcs.push_back(j);
            readIds.push_back(i);
            rfChoices.push_back(std::move(srcs));
        }
    }

    // Judges one candidate: rf as chosen, co as the order of writesByLoc (initial write first).
    void judge(const std::vector<int>& rfSrc, const std::vector<std::vector<int>>& coOrder) {
        int n = static_cast<int>(ev.size());
        Rel rf{n, {}}, co{n, {}};
        for (size_t k = 0; k < readIds.size(); k++) rf.add(rfSrc[k], readIds[k]);
        for (size_t l = 0; l < coOrder.size(); l++) {
            const auto& order = coOrder[l];
            for (size_t a = 0; a < order.size(); a++) {
                co.add(static_cast<int>(l), order[a]);  // the initial write of l is event l
                for (size_t b = a + 1; b < order.size(); b++) co.add(order[a], order[b]);
            }
        }
        result.candidates++;

        Rel fr = seq(inverse(rf), co);
        Rel intr = Rel{n, {}};  // same thread, two different events
        const uint64_t all = n == kMaxEvents ? ~0ull : (1ull << n) - 1;
        for (int i = 0; i < n; i++) intr.row[i] = ~ext.row[i] & ~(1ull << i) & all;
        Rel rfe = rf & ext, rfi = rf & intr, coe = co & ext, coi = co & intr, fre = fr & ext;
        Rel poLoc = po & sameLoc;

        // let ca = fr | co
        Rel ca = fr | co;
        // let obs = rfe | fre | coe
        Rel obs = rfe | fre | coe;
        // let dob = addr | data | ctrl;[W] | (ctrl | (addr;po));[ISB];po;[R] | addr;po;[W]
        //         | (ctrl | data);coi | (addr | data);rfi
        Rel dob = addr | data | ran(ctrl, W)
                | ran(seq(ran(ctrl | seq(addr, po), ISB), po), R)
                | ran(seq(addr, po), W)
                | seq(ctrl | data, coi)
                | seq(addr | data, rfi);
        // let aob = rmw | [range(rmw)];rfi;[A | Q]
        Rel aob = rmw | ran(dom(rangeRmw, rfi), A | Q);
        // let bob = po;[dmb.full];po | [L];po;[A] | [R];po;[dmb.ld];po | [A | Q];po
        //         | [W];po;[dmb.st];po;[W] | po;[L] | po;[L];coi
        Rel bob = seq(ran(po, Ffull), po)
                | ran(dom(L, po), A)
                | seq(ran(dom(R, po), Fld), po)
                | dom(A | Q, po)
                | ran(seq(ran(dom(W, po), Fst), po), W)
                | ran(po, L)
                | seq(ran(po, L), coi);
        // let ob = (obs | dob | aob | bob)+
        Rel ob = obs | dob | aob | bob;

        if (axioms.internal && !acyclic(poLoc | ca | rf)) return;   // acyclic po-loc | ca | rf
        if (axioms.external && !acyclic(ob)) return;                // irreflexive ob
        if (axioms.atomic && !empty(rmw & seq(fre, coe))) return;   // empty rmw & (fre; coe)

        // Allowed: record the final state.
        result.allowed++;
        State st;
        for (const Var& v : test.observed) {
            if (v.loc >= 0) {
                const auto& order = coOrder[v.loc];
                st[v] = order.empty() ? ev[v.loc].val : ev[order.back()].val;
            } else {
                st[v] = paths[v.thread]->regs[v.reg];
            }
        }
        if (evaluate(test.condition, st)) result.positive++;
        result.states.insert(st);
    }

    void enumerateCo(const std::vector<int>& rfSrc, size_t loc, std::vector<std::vector<int>>& order) {
        if (loc == writesByLoc.size()) { judge(rfSrc, order); return; }
        std::vector<int> ws = writesByLoc[loc];
        std::sort(ws.begin(), ws.end());
        do {
            order[loc] = ws;
            enumerateCo(rfSrc, loc + 1, order);
        } while (std::next_permutation(ws.begin(), ws.end()));
    }

    void enumerateRf(size_t k, std::vector<int>& rfSrc) {
        if (k == readIds.size()) {
            std::vector<std::vector<int>> order(writesByLoc.size());
            enumerateCo(rfSrc, 0, order);
            return;
        }
        for (int src : rfChoices[k]) {
            rfSrc[k] = src;
            enumerateRf(k + 1, rfSrc);
        }
    }

    void joinThreads(const std::vector<std::vector<Path>>& all, size_t th) {
        if (th == all.size()) {
            ev.resize(test.locations.size());  // keep the initial writes, drop the last join
            first.assign(all.size(), 0);
            for (size_t t = 0; t < all.size(); t++) {
                first[t] = static_cast<int>(ev.size());
                for (const Event& e : paths[t]->events) ev.push_back(e);
            }
            if (ev.size() > kMaxEvents) throw Unsupported("too many events");
            prepare();
            for (const auto& c : rfChoices) if (c.empty()) return;  // a value nobody wrote
            std::vector<int> rfSrc(readIds.size());
            enumerateRf(0, rfSrc);
            return;
        }
        for (const Path& p : all[th]) {
            paths[th] = &p;
            joinThreads(all, th + 1);
        }
    }
};

}  // namespace

Result check(const Test& test, const Axioms& axioms) {
    Result result;
    result.name = test.name;
    try {
        Domains domains = valueDomains(test);
        std::vector<std::vector<Path>> all;
        for (size_t th = 0; th < test.threads.size(); th++)
            all.push_back(runThread(test, static_cast<int>(th), domains));

        Checker c(test, axioms, result);
        for (size_t l = 0; l < test.locations.size(); l++) {  // event l is location l's initial write
            Event init;
            init.kind = Event::Write;
            init.loc = static_cast<int>(l);
            init.val = initialValue(test, static_cast<int>(l));
            init.text = "init " + test.locations[l];
            c.ev.push_back(init);
        }
        c.paths.assign(test.threads.size(), nullptr);
        c.joinThreads(all, 0);

        if (test.quantifier == Quantifier::Exists) result.conditionHolds = result.positive > 0;
        else if (test.quantifier == Quantifier::NotExists) result.conditionHolds = result.positive == 0;
        else result.conditionHolds = result.positive == result.allowed;
    } catch (const Unsupported& e) {
        result.supported = false;
        result.error = e.what();
    }
    return result;
}

}  // namespace mca
