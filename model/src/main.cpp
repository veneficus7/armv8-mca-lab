// mca-check: what the MCA ARMv8 axiomatic model allows for each litmus test.
//
//   mca-check [--json] [--no-internal] [--no-external] [--no-atomic] test.litmus...
//
// Text output follows herd7's layout (Test / States / Ok|No / Observation), so a result can be read
// beside the model logs published with the paper. --json prints one object per line for tools.
#include "model.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

const char* observation(const mca::Result& r) {
    if (r.positive == 0) return "Never";
    if (r.positive == r.allowed) return "Always";
    return "Sometimes";
}

void printText(const mca::Test& t, const mca::Result& r) {
    std::cout << "Test " << t.name << "\n";
    if (!r.supported) {
        std::cout << "Unsupported: " << r.error << "\n\n";
        return;
    }
    std::cout << "States " << r.states.size() << "\n";
    for (const auto& st : r.states) {
        for (const auto& v : t.observed) std::cout << t.varName(v) << "=" << t.valueText(st.at(v)) << "; ";
        std::cout << "\n";
    }
    std::cout << (r.conditionHolds ? "Ok" : "No") << "\n";
    std::cout << "Observation " << t.name << " " << observation(r) << " " << r.positive << " "
              << (r.allowed - r.positive) << "\n";
    std::cout << "Candidates " << r.candidates << ", allowed " << r.allowed << "\n\n";
}

void printJson(const mca::Test* t, const std::string& file, const mca::Result& r) {
    std::cout << "{\"file\":\"" << jsonEscape(file) << "\",\"name\":\"" << jsonEscape(r.name) << "\"";
    std::cout << ",\"supported\":" << (r.supported ? "true" : "false");
    if (!r.supported) {
        std::cout << ",\"error\":\"" << jsonEscape(r.error) << "\"}\n";
        return;
    }
    std::cout << ",\"observation\":\"" << observation(r) << "\",\"ok\":" << (r.conditionHolds ? "true" : "false")
              << ",\"candidates\":" << r.candidates << ",\"allowed\":" << r.allowed << ",\"states\":[";
    bool firstState = true;
    for (const auto& st : r.states) {
        std::cout << (firstState ? "" : ",") << "{";
        bool firstVar = true;
        for (const auto& v : t->observed) {
            std::cout << (firstVar ? "" : ",") << "\"" << t->varName(v) << "\":\"" << t->valueText(st.at(v)) << "\"";
            firstVar = false;
        }
        std::cout << "}";
        firstState = false;
    }
    std::cout << "]}\n";
}

// The condition as a C expression over v[k], the k-th observed variable, for the hardware runner.
std::string conditionC(const mca::Test& t, const mca::CondExpr& e) {
    using K = mca::CondExpr;
    switch (e.kind) {
        case K::True: return "1";
        case K::Atom: {
            size_t k = std::find(t.observed.begin(), t.observed.end(), e.var) - t.observed.begin();
            if (e.value.isAddress()) return "0 /* compares with an address */";
            return "(v[" + std::to_string(k) + "] == " + std::to_string(static_cast<uint64_t>(e.value.v)) + "ull)";
        }
        case K::And: return "(" + conditionC(t, e.kids[0]) + " && " + conditionC(t, e.kids[1]) + ")";
        case K::Or: return "(" + conditionC(t, e.kids[0]) + " || " + conditionC(t, e.kids[1]) + ")";
        case K::Not: return "(!" + conditionC(t, e.kids[0]) + ")";
    }
    return "0";
}

// Everything the hardware generator needs, plus the model's verdict, as one JSON line.
void printDescribe(const std::string& file, const mca::Test& t, const mca::Result& r) {
    std::cout << "{\"file\":\"" << jsonEscape(file) << "\",\"name\":\"" << jsonEscape(t.name) << "\"";
    std::cout << ",\"supported\":" << (r.supported ? "true" : "false");
    if (!r.supported) {
        std::cout << ",\"error\":\"" << jsonEscape(r.error) << "\"}\n";
        return;
    }
    std::cout << ",\"locations\":[";
    for (size_t l = 0; l < t.locations.size(); l++) {
        auto it = t.memInit.find(static_cast<int>(l));
        std::cout << (l ? "," : "") << "{\"name\":\"" << t.locations[l] << "\",\"init\":"
                  << (it == t.memInit.end() ? 0 : it->second) << "}";
    }
    std::cout << "],\"threads\":[";
    bool lse = false, rcpc = false;
    for (size_t th = 0; th < t.threads.size(); th++) {
        std::cout << (th ? "," : "") << "{\"init\":{";
        bool first = true;
        for (auto& [reg, v] : t.regInit[th]) {
            std::cout << (first ? "" : ",") << "\"" << reg << "\":";
            if (v.isAddress()) std::cout << "{\"loc\":" << v.loc << "}";
            else std::cout << "{\"int\":" << v.v << "}";
            first = false;
        }
        std::cout << "},\"code\":[";
        for (size_t k = 0; k < t.threads[th].size(); k++) {
            const auto& in = t.threads[th][k];
            if (in.op == mca::Op::Swp || in.op == mca::Op::Ldadd || in.op == mca::Op::Cas) lse = true;
            if (in.op == mca::Op::Ldapr) rcpc = true;
            std::cout << (k ? "," : "") << "\"" << jsonEscape(in.text) << "\"";
        }
        std::cout << "]}";
    }
    std::cout << "],\"observed\":[";
    for (size_t k = 0; k < t.observed.size(); k++) {
        const auto& v = t.observed[k];
        std::cout << (k ? "," : "") << "{\"name\":\"" << t.varName(v) << "\",\"thread\":" << v.thread
                  << ",\"reg\":" << v.reg << ",\"loc\":" << v.loc << "}";
    }
    std::cout << "],\"quantifier\":\""
              << (t.quantifier == mca::Quantifier::Exists ? "exists" : t.quantifier == mca::Quantifier::NotExists ? "~exists" : "forall")
              << "\",\"condition_c\":\"" << jsonEscape(conditionC(t, t.condition)) << "\"";
    std::cout << ",\"needs\":[" << (lse ? "\"lse\"" : "") << (lse && rcpc ? "," : "") << (rcpc ? "\"rcpc\"" : "") << "]";
    std::cout << ",\"observation\":\"" << observation(r) << "\",\"allowed\":[";
    bool firstState = true;
    for (const auto& st : r.states) {
        std::cout << (firstState ? "" : ",") << "\"";
        for (const auto& v : t.observed) std::cout << t.varName(v) << "=" << t.valueText(st.at(v)) << "; ";
        std::cout << "\"";
        firstState = false;
    }
    std::cout << "]}\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool json = false, describe = false;
    mca::Axioms axioms;
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--json") json = true;
        else if (a == "--describe") describe = true;
        else if (a == "--no-internal") axioms.internal = false;
        else if (a == "--no-external") axioms.external = false;
        else if (a == "--no-atomic") axioms.atomic = false;
        else if (a == "-h" || a == "--help") {
            std::cout << "usage: mca-check [--json] [--no-internal] [--no-external] [--no-atomic] test.litmus...\n";
            return 0;
        } else files.push_back(a);
    }
    if (files.empty()) {
        std::cerr << "usage: mca-check [--json] [--no-internal] [--no-external] [--no-atomic] test.litmus...\n";
        return 2;
    }
    int failures = 0;
    for (const auto& f : files) {
        std::ifstream in(f);
        if (!in) {
            std::cerr << f << ": cannot open\n";
            failures++;
            continue;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        std::string err;
        auto test = mca::parseLitmus(ss.str(), err);
        if (!test) {
            mca::Result r;
            r.supported = false;
            r.error = "parse: " + err;
            r.name = f;
            if (json) printJson(nullptr, f, r);
            else std::cout << "Test " << f << "\nUnsupported: " << r.error << "\n\n";
            continue;
        }
        mca::Result r = mca::check(*test, axioms);
        if (describe) printDescribe(f, *test, r);
        else if (json) printJson(&*test, f, r);
        else printText(*test, r);
    }
    return failures ? 1 : 0;
}
