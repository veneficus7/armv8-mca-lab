#!/usr/bin/env python3
"""What the silicon did against what the model allows, for every phone and configuration run.

    python3 hw/compare.py results/hw      ->  results/hardware.md

Per test and configuration: how often the condition's state appeared, and whether any state the
model forbids appeared at all. Then the controls:
  - positive: SB must be seen somewhere, or "allowed, not seen" means nothing for that run;
  - negative: no forbidden state, in any test, in any run. The coherence tests (Co*) above all.
A state that was never seen was *not observed* in that many runs; it does not mean the core
cannot produce it.
"""
import glob
import json
import os
import sys


def load(dirpath):
    runs = {}
    for f in sorted(glob.glob(os.path.join(dirpath, "*.jsonl"))):
        cfg = os.path.basename(f)[:-len(".jsonl")]
        runs[cfg] = {}
        for line in open(f):
            line = line.strip()
            if line.startswith("{"):
                r = json.loads(line)
                runs[cfg][r["test"]] = r
    return runs


def cell(r):
    if r is None:
        return "—"
    if "skipped" in r:
        return "skipped (%s)" % r["skipped"]
    s = "%s / %s" % (format(r["condition_seen"], ","), format(r["iterations"], ","))
    if r["forbidden_seen"]:
        s += " **FORBIDDEN: %d**" % r["forbidden_seen"]
    return s


def pinned_ok(r):
    for t in r.get("threads", []):
        if t["pin_error"]:
            return False
        if t["cpu_requested"] >= 0 and t["cpus_seen"] != [t["cpu_requested"]]:
            return False
    return True


def main():
    root = sys.argv[1]
    out = ["# The silicon against the model", ""]
    for phone in sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d))):
        pdir = os.path.join(root, phone)
        info = json.load(open(os.path.join(pdir, "info.json")))
        runs = load(pdir)
        if not runs:
            continue
        parts = sorted({(c["implementer"], c["part"], c["max_khz"]) for c in info["cpus"]}, key=lambda x: x[2])
        out += ["## %s" % phone, "",
                "CPUs: " + "; ".join("part %s (implementer %s) up to %d MHz" % (p, i, k // 1000) for i, p, k in parts)
                + ". LSE atomics: %s. LDAPR: %s." % (info["lse"], info["lrcpc"]), ""]
        for name in ("thermal_before.txt", "thermal_after.txt"):
            p = os.path.join(pdir, name)
            if os.path.exists(p):
                out.append("%s: %s" % (name[:-4].replace("_", " "), open(p).read().strip() or "unknown"))
        out.append("")
        # Files are <sync>__<config>.jsonl: one table per way of synchronising the threads.
        modes = {}
        for c in runs:
            mode, _, cfg = c.partition("__") if "__" in c else ("barrier", "", c)
            modes.setdefault(mode, {})[cfg] = runs[c]
        for mode in sorted(modes, key=lambda m: m != "time"):
            mruns = modes[mode]
            cfgs = list(mruns)
            tests = []
            for c in cfgs:
                for t in mruns[c]:
                    if t not in tests:
                        tests.append(t)
            out += ["### Threads synchronised by %s" % ("the shared counter (time base)" if mode == "time" else "a spin barrier per instance"), ""]
            out.append("| Test | Model | " + " | ".join(cfgs) + " |")
            out.append("| --- | --- | " + " | ".join("---" for _ in cfgs) + " |")
            forbidden_total, sb_seen, pin_problems, never_seen = 0, 0, [], []
            for t in tests:
                model = next((mruns[c][t].get("model") for c in cfgs if t in mruns[c] and "model" in mruns[c][t]), "?")
                out.append("| %s | %s | %s |" % (t, model, " | ".join(cell(mruns[c].get(t)) for c in cfgs)))
                seen = 0
                for c in cfgs:
                    r = mruns[c].get(t)
                    if r and "skipped" not in r:
                        forbidden_total += r["forbidden_seen"]
                        seen += r["condition_seen"]
                        if t == "SB":
                            sb_seen += r["condition_seen"]
                        if c != "unpinned" and not pinned_ok(r):
                            pin_problems.append("%s/%s" % (c, t))
                if model != "Never" and seen == 0:
                    never_seen.append(t)
            out += ["", "- Forbidden states seen, all tests and configurations: **%d**" % forbidden_total,
                    "- Positive control, SB's relaxed state seen: **%s**" % format(sb_seen, ","),
                    "- Allowed by the model, never observed here: %s" % (", ".join(never_seen) or "none"),
                    "- Threads that did not stay on their requested CPU: %s" % (", ".join(pin_problems[:10]) or "none"), ""]
    os.makedirs("results", exist_ok=True)
    open(os.path.join("results", "hardware.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
