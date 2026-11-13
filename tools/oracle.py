#!/usr/bin/env python3
"""Checks mca-check against answers it did not produce.

Two references, strongest first:
  1. The ARMv8-axiomatic model logs published with Pulte et al. (POPL 2018). For every test that
     has one, the *whole set* of final states mca-check allows must equal the published set.
  2. herdtools7's kinds.txt (Allowed / Forbidden), for catalogue tests with no published log.
     That file reflects herd's current aarch64.cat, which has grown since 2018, so a mismatch
     there is a question to look into, not automatically a bug.

Then each axiom is switched off in turn. A rule that changes no verdict is not being tested.

    python3 tools/oracle.py            (after tools/fetch_tests.sh and make -C model)
"""
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TP = os.path.join(ROOT, "third_party")
CHECK = os.path.join(ROOT, "model", "mca-check")


def run_check(files, *flags):
    out = subprocess.run([CHECK, "--json", *flags, *files], capture_output=True, text=True, check=False).stdout
    return {r["file"]: r for r in map(json.loads, out.splitlines())}  # by path: two dirs may share a name


def published_logs():
    logs = {}
    for p in glob.glob(os.path.join(TP, "paper-results", "**", "*.model_log"), recursive=True):
        logs[os.path.basename(p)[:-len(".litmus.model_log")]] = p
    return logs


def parse_log(path):
    """A herd model log: its final states (each {var: value}), observation and condition."""
    text = open(path).read().splitlines()
    n = int(next(l for l in text if l.startswith("States ")).split()[1])
    start = next(i for i, l in enumerate(text) if l.startswith("States ")) + 1
    states = []
    for line in text[start:start + n]:
        st = {}
        for item in filter(None, (s.strip() for s in line.split(";"))):
            var, val = item.split("=", 1)
            st[var.strip()] = norm(val.strip())
        states.append(st)
    obs = next((l.split()[2] for l in text if l.startswith("Observation ")), None)
    cond = next((l[len("Condition "):] for l in text if l.startswith("Condition ")), "")
    return states, obs, cond


def norm_cond(c):
    """A condition with spacing, brackets and hex removed, so two spellings of one test compare."""
    c = re.sub(r"0x[0-9a-fA-F]+", lambda m: str(int(m.group(0), 16)), c)
    return re.sub(r"[\s()\[\]]", "", c)


def our_condition(litmus_path):
    text = open(litmus_path).read()
    m = re.search(r"(~exists|exists|forall)(.*)$", text, re.S)
    return (m.group(1) + m.group(2)) if m else ""


def project(states, variables):
    return {frozenset((k, v) for k, v in st.items() if k in variables) for st in states}


def norm(v):
    try:
        return str(int(v, 0))
    except ValueError:
        return v  # a location name


def ours(r):
    return {frozenset((k, norm(v)) for k, v in s.items()) for s in r["states"]}


def kinds():
    p = os.path.join(TP, "herd-catalogue", "kinds.txt")
    out = {}
    for line in open(p):
        parts = line.split()
        if len(parts) >= 2:
            out[parts[0]] = parts[1]
    return out


def verdict(r):
    return "Forbidden" if r["observation"] == "Never" else "Allowed"


def main():
    files = sorted(glob.glob(os.path.join(TP, "paper-tests", "*.litmus")) +
                   glob.glob(os.path.join(TP, "herd-catalogue", "*.litmus")) +
                   glob.glob(os.path.join(ROOT, "tests", "hand", "*.litmus")))
    results = run_check(files)
    logs = published_logs()
    kind = kinds()

    rows, exact_ok, exact_bad, kinds_ok, kinds_bad, unsupported, other_version = [], 0, 0, 0, 0, [], []
    for path in sorted(results, key=lambda p: (os.path.basename(p), p)):
        r = results[path]
        name = os.path.basename(path)[:-len(".litmus")]
        if not r["supported"]:
            unsupported.append((name, r["error"]))
            continue
        if name in logs:
            pub, pub_obs, pub_cond = parse_log(logs[name])
            mine = ours(r)
            our_vars = {k for st in r["states"] for k in st}
            # herd's logs also list the final value of some locations the condition does not name;
            # compare on the variables mca-check reports.
            same = mine == project(pub, our_vars)
            if norm_cond(our_condition(path)) != norm_cond(pub_cond):
                other_version.append((name, our_condition(path).strip(), pub_cond,
                                      "states agree on shared variables" if same else "states differ"))
                continue
            exact_ok += same
            exact_bad += not same
            rows.append((name, "paper log", f"{len(pub)} states, {pub_obs}", f"{len(mine)} states, {r['observation']}",
                         "match" if same else "DIFFERENT"))
        elif name in kind and kind[name] in ("Allowed", "Forbidden"):
            same = verdict(r) == kind[name]
            kinds_ok += same
            kinds_bad += not same
            rows.append((name, "kinds.txt", kind[name], verdict(r), "match" if same else "DIFFERENT"))

    # Each axiom off in turn: which verdicts change?
    supported = [f for f in files if results[f]["supported"]]
    mutations = {}
    for flag in ("--no-internal", "--no-external", "--no-atomic"):
        alt = run_check(supported, flag)
        mutations[flag] = sorted({os.path.basename(p)[:-7] for p, r in alt.items() if ours(r) != ours(results[p])})

    lines = ["# mca-check against published answers", "",
             f"- Whole state sets equal to the paper's published ARMv8-axiomatic logs: **{exact_ok} of {exact_ok + exact_bad}**",
             f"- A log under the same name but for a different version of the test (its condition differs): {len(other_version)}",
             f"- Verdicts equal to herdtools7 `kinds.txt` (current model, tests with no paper log): **{kinds_ok} of {kinds_ok + kinds_bad}**",
             f"- Not supported by mca-check: {len(unsupported)}", "",
             "| Test | Reference | Reference says | mca-check says | |", "| --- | --- | --- | --- | --- |"]
    lines += [f"| {a} | {b} | {c} | {d} | {e} |" for a, b, c, d, e in rows]
    lines += ["", "## Each axiom switched off", ""]
    for flag, changed in mutations.items():
        lines.append(f"- `{flag}` changes the allowed states of {len(changed)} tests"
                     + (f": {', '.join(changed[:12])}{' …' if len(changed) > 12 else ''}" if changed else ""))
    if other_version:
        lines += ["", "## Same name, different test", "",
                  "The paper's log and this file share a name but not a condition, so their states are not compared.", "",
                  "| Test | This file | The paper's log | |", "| --- | --- | --- | --- |"]
        lines += [f"| {n} | `{a}` | `{b}` | {c} |" for n, a, b, c in other_version]
    if unsupported:
        lines += ["", "## Not supported", ""] + [f"- {n}: {e}" for n, e in unsupported]
    os.makedirs(os.path.join(ROOT, "results"), exist_ok=True)
    report = "\n".join(lines) + "\n"
    open(os.path.join(ROOT, "results", "oracle.md"), "w").write(report)
    print(report)
    return 0 if exact_bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
