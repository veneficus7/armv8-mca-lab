// The MCA ARMv8 axiomatic model of Pulte et al., "Simplifying ARM Concurrency" (POPL 2018), §7.
#pragma once

#include "litmus.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mca {

// One memory or barrier event of a candidate execution.
struct Event {
    enum Kind { Read, Write, Fence } kind = Read;
    int thread = -1;        // -1 for the initial writes
    int loc = -1;
    Value val;
    bool acquire = false;   // A: LDAR and the acquire forms of the atomics
    bool acquirePC = false; // Q: LDAPR
    bool release = false;   // L: STLR and the release forms
    bool isb = false;       // a Fence that is an ISB rather than a DMB
    Barrier barrier = Barrier::Full;
    // Dependencies, as bit sets over this thread's events (bit i = the thread's i-th event).
    uint64_t addr = 0, data = 0, ctrl = 0;
    int rmwRead = -1;       // for the write of an atomic or a successful store-exclusive: its read
    std::string text;       // the instruction it came from
};

// A final state: a value for each observed variable.
using State = std::map<Var, Value>;

struct Result {
    std::string name;
    bool supported = true;
    std::string error;
    std::set<State> states;       // final states of the executions the model allows
    long long candidates = 0;     // candidate executions examined
    long long allowed = 0;        // ... of which the model allows
    long long positive = 0;       // allowed executions satisfying the condition
    bool conditionHolds = false;  // herd's "Ok"
};

// Which axioms to apply; all three by default. Turning one off is how the test suite proves it
// matters (a mutation that must change some verdicts).
struct Axioms {
    bool internal = true;
    bool external = true;
    bool atomic = true;
};

Result check(const Test& test, const Axioms& axioms = {});

}  // namespace mca
