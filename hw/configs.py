#!/usr/bin/env python3
"""Prints the core configurations to run, one per line: `name cpu,cpu,...` (no list: unpinned).

A cluster is the set of CPUs that share a maximum clock. Up to four CPUs are used per
configuration, because the biggest test has four threads.
"""
import json
import sys
from collections import defaultdict

info = json.load(open(sys.argv[1]))
clusters = defaultdict(list)
for c in info["cpus"]:
    clusters[c["max_khz"]].append(c["cpu"])
by_speed = sorted(clusters.items())  # slowest first

print("unpinned")
for khz, cpus in by_speed:
    if len(cpus) >= 2:
        print("cluster-%dMHz %s" % (khz // 1000, ",".join(map(str, cpus[:4]))))
if len(by_speed) >= 2:
    slow, fast = by_speed[0][1], by_speed[-1][1]
    mixed = [fast[0], slow[0], fast[min(1, len(fast) - 1)], slow[min(1, len(slow) - 1)]]
    print("cross %s" % ",".join(map(str, mixed)))
