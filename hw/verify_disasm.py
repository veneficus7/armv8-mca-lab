#!/usr/bin/env python3
"""Checks that the compiled binary runs each test's instructions as written.

The C compiler cannot reorder inside one asm block, but a typo in gen.py, a label rewritten wrongly
or an assembler alias could still change the code. So the disassembly of every generated thread
function must contain the test's instructions, in order and contiguous, operands included
(immediates compared as numbers; branch targets by mnemonic and register only).

    python3 verify_disasm.py tests.jsonl mca-hw.dis
"""
import json
import re
import sys


def norm(ins):
    ins = ins.split("//")[0].strip().lower()
    ins = re.sub(r"#(0x[0-9a-f]+|-?\d+)", lambda m: "#%d" % int(m.group(1), 0), ins)
    ins = re.sub(r"\s+", "", ins)
    m = re.match(r"(b\.\w+|b|cbz|cbnz)(.*)", ins)
    if m:  # branch: keep the mnemonic and a register operand, drop the target
        mn, ops = m.groups()
        reg = re.match(r"([wx]\d+),", ops)
        return mn + (" " + reg.group(1) if reg else "")
    return ins


def functions(dis):
    funcs, cur = {}, None
    for line in open(dis):
        m = re.match(r"^[0-9a-f]+ <(.+)>:", line)
        if m:
            cur = m.group(1)
            funcs[cur] = []
            continue
        m = re.match(r"^\s*[0-9a-f]+:\s+(.*)$", line)
        if m and cur:
            funcs[cur].append(norm(m.group(1)))
    return funcs


def main():
    tests = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
    funcs = functions(sys.argv[2])
    checked, bad = 0, 0
    for k, t in enumerate(tests):
        if not t.get("supported"):
            continue
        base = "t%d_" % k + re.sub(r"[^A-Za-z0-9]", "_", t["name"])
        for ti, th in enumerate(t["threads"]):
            fn = "%s_p%d" % (base, ti)
            if fn not in funcs:
                continue  # skipped by gen.py, which said why
            want = [norm(c) for c in th["code"] if not c.endswith(":")]
            got = funcs[fn]
            ok = any(got[i:i + len(want)] == want for i in range(len(got) - len(want) + 1))
            checked += 1
            if not ok:
                bad += 1
                print("MISMATCH %s thread %d\n  want %s\n  got  %s" % (t["name"], ti, want, got), file=sys.stderr)
    print("disassembly: %d thread bodies checked, %d mismatched" % (checked, bad), file=sys.stderr)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
