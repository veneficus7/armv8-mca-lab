# armv8-mca-lab

The Arm memory model from *Simplifying ARM Concurrency* (Pulte, Flur, Deacon, French, Sarkar,
Sewell, POPL 2018), implemented as a model checker in C++. It is checked against the paper's own
published results, and paired with a runner that executes the same tests on Arm cores in an Android
phone. On a Galaxy S25 Ultra (Qualcomm Oryon cores), **147 million test runs produced no state the
model forbids**, while every distinction the model draws, such as `PPOCA` against `PPOAA` or `LDAPR`
against `LDAR`, also appeared in the silicon ([FINDINGS.md](docs/FINDINGS.md)).

- **`model/`, `mca-check`:** reads AArch64 litmus tests in herd's format, runs each thread over
  every value its loads could see, derives the address/data/control dependencies through
  registers, lists every candidate execution, and applies the paper's three axioms word for word.
  It agrees with the paper's published results on all 45 tests where both can be identified, as
  whole state sets. Those tests cover dependencies, barriers, `LDAR` and `STLR`. Exclusives,
  Armv8.1 atomics and `LDAPR` are checked only against today's herd verdicts
  ([FINDINGS.md](docs/FINDINGS.md)).
- **`hw/`, `mca-hw`:** generates C with each test's own instructions as inline AArch64 assembly,
  pins threads to chosen cores, starts every thread on the same tick of the Arm generic timer
  (`CNTVCT_EL0`), runs each test a million times, and marks every final state the model forbids.
  Built with the Android NDK. The disassembly is checked against the tests, and the harness
  machinery is smoke-tested on x86 (`hw/host_smoke.c`).
- **`docs/`:** [PAPER.md](docs/PAPER.md) explains the paper, [FINDINGS.md](docs/FINDINGS.md) gives
  the results, and [LEARNING.md](docs/LEARNING.md) works through it step by step.

## Build and run

```bash
bash tools/fetch_tests.sh        # the paper's tests and model logs, herd's AArch64 catalogue
make -C model                    # mca-check (g++ 12+, C++20)
python3 tools/oracle.py          # checks mca-check against the published answers
./model/mca-check third_party/paper-tests/IRIW+addrs.litmus
bash hw/build.sh                 # mca-hw for arm64 Android (NDK r30)
bash hw/run_phone.sh             # on a phone with USB debugging: writes results/hardware.md
```

Run from WSL or Linux. The NDK and adb are found in the Android SDK: `ANDROID_HOME` if set, else
Android Studio's default location (on Windows, through WSL), else `~/Android/Sdk`. `NDK=` and `ADB=`
override them.

## Layout

```
model/src/litmus.hpp, parse.cpp   herd's AArch64 litmus format
model/src/model.cpp               thread execution, candidate executions, the axioms
model/src/main.cpp                CLI: herd-style text, --json, --describe
tools/oracle.py                   comparison with the paper's logs and herd's kinds.txt
hw/gen.py, harness.c              the on-device runner
hw/verify_disasm.py               the compiled code is the test's code
tests/hand/                       17 classic tests written to the paper's logged conditions
results/                          oracle.md, hardware.md
```
