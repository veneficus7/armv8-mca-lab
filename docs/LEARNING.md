# Learning it: the Arm memory model, step by step

Read [PAPER.md](PAPER.md) first. Then this file, with `model/src/model.cpp` open beside it.

## 0. The one-sentence version

"I implemented the formal Arm memory model from a POPL 2018 paper as a C++ model checker. It
reproduces the paper's published results exactly on 45 tests. I then ran the same tests as real
AArch64 assembly on a Galaxy S25 Ultra: in 147 million runs the cores never produced an outcome the
model forbids, and every reordering the model allows in one test and forbids in its neighbour
showed up the same way in the silicon."

## 1. Ideas you need

**Litmus test.** A tiny concurrent program plus a question: can this final state happen? `MP`
(message passing):

```
P0: x = 1; y = 1        P1: r0 = y; r1 = x        exists (r0 = 1 /\ r1 = 0)
```

Seeing the flag (`y=1`) but not the data (`x=0`) is the bug every lock-free queue fears. On Arm
it is **allowed** without barriers: P0's two stores, or P1's two loads, can be reordered.

**The six shapes.** Their names are worth knowing by heart.
- `MP`: message passing (above).
- `SB`: store buffering. Each thread writes one variable and reads the other; both read 0. Allowed
  even on x86.
- `LB`: load buffering. Each thread reads one variable, then writes the other; both reads see the
  other's write.
- `S`, `R`, `2+2W`: the other two-thread mixes of reads and writes.
- `IRIW`: four threads. Do two readers agree on the order of two independent writes? This is the
  multicopy-atomicity test.

**What stops a reordering on Arm** (the `dob` and `bob` relations):
- an **address dependency** (a load's address computed from an earlier load: `EOR W2,W0,W0` then
  `[X4,W2,SXTW]`, the classic way to make a dependency that is always zero);
- a **data dependency** (a store's value computed from a load);
- a **control dependency**, but only to *stores*, or to loads behind an `ISB`;
- **barriers**: `DMB SY` (everything), `DMB LD` (orders a read with everything after it),
  `DMB ST` (orders write against write);
- **acquire/release**: `LDAR` (nothing after it moves before it), `STLR` (nothing before it moves
  after it), and `LDAPR` (a weaker acquire: a release before it may still pass it).

**Candidate execution.** One way the test could run: which write each read got its value from
(`rf`), and the order of all writes to each location (`co`). The model is three rules that throw
out the impossible ones. Whatever survives is allowed.

**Multicopy atomicity (MCA).** A write becomes visible to all other cores at once. With it, two
readers can never disagree about the order of two writes, so `IRIW+addrs` is forbidden. Without it
(IBM POWER, early ARMv8 on paper) they can.

## 2. What was built, step by step

1. **Choosing the paper** (PAPER.md, "Why it was chosen"). The decisive criterion was
   *checkable*: the authors published their tests and their model's output, so the work could be
   graded by something other than itself.
2. **Reading the model** on pages 19–20 of the paper. Every clause in `model.cpp` has the paper's
   text above it as a comment.
3. **Parsing herd's real format** (`parse.cpp`), not an invented one. That way the paper's tests,
   herd's catalogue and the phone runner all read the same files.
4. **Running a thread over every value a read could see** (`ThreadRunner::run`).
   - At each load the run forks: one path per value the location could hold.
   - A register remembers which loads its value came from (its *taint*). That is how
     dependencies are found, as the architecture defines them: through registers, not values.
     `EOR W2,W0,W0` is always 0, but it still depends on the load into `W0`.
5. **Which values a location can hold** (`valueDomains`).
   - Repeat until nothing new appears, or until there have been as many rounds as the program has
     writes.
   - An `LDADD` feeding an `LDADD` would keep growing; the round limit is sound because no
     execution has more writes than that.
6. **Joining the threads** (`Checker::joinThreads`, `enumerateRf`, `enumerateCo`). Every
   combination of thread paths, `rf` choice (the value must match) and `co` order.
7. **The axioms** (`Checker::judge`). Relations are 64-bit bit-matrices:
   - `seq` is relational composition;
   - `dom` and `ran` are the paper's `[S];r` and `r;[S]`;
   - `acyclic` is a transitive closure.
8. **Checking against answers it did not produce** (`tools/oracle.py`): 45 of 45 published state
   sets match. Then each axiom was switched off, to prove it matters.
9. **The phone runner** (`hw/`).
   - `mca-check --describe` hands each test to `gen.py`, which writes each thread as inline
     assembly.
   - The C harness pins threads, starts every thread of instance *i* on the same tick of the Arm
     generic timer (`CNTVCT_EL0`, a counter every core shares), and counts every final state.
   - `verify_disasm.py` proves the compiled code is the test's code.
10. **On the phone** (`hw/run_phone.sh`). First a smoke run of the controls, then 38 tests × a million
    instances × four core layouts. `/proc/cpuinfo` showed implementer `0x51`, part `0x001`: Qualcomm's
    own Oryon cores, which implement the Arm architecture under licence.

## 3. Mistakes made on the way, and what each one taught

- **Comparing by name.** Four of herd's tests share names with different tests in the paper's
  suite. The first comparison flagged them as failures. Lesson: identify a test by its content (its
  condition), not its name.
- **The same file name in two folders.** The comparison indexed results by name, so the paper's
  `LB` and herd's `LB` were checked once. Lesson: key by path.
- **A plausible fix that was wrong.** Making a `CAS` write depend on its compare register fixed one
  test and broke two. It was reverted, and the note stays in the code. Lesson: a fix is the
  whole test suite's verdict, never one test's.
- **The verifier, not the code.** The disassembly check first failed 5 bodies, because labels in
  the asm became symbols and split each function. Lesson: when a check fails, check the check.
- **Zeros that meant nothing.** The first phone run lined threads up with a spin barrier before each
  instance. Nothing forbidden appeared, but almost nothing allowed did either: `SB` 44 times in 4
  million. A barrier's release travels core to core and takes far longer than the few nanoseconds a
  test body runs, so the threads barely overlapped. Starting them on the same timer tick raised `SB`
  to 399,821. Lesson: a "never seen" is only worth something next to a positive control that *is*
  seen.
- **`\r` from Windows.** The run script compared `adb get-state` with `device`, and Windows' `adb`
  answers `device\r`. Lesson: when a tool crosses Windows and Linux, strip line endings.

## 4. Interview questions, with answers to put in your own words

1. **What is multicopy atomicity, and how do you test for it?**
   A write is visible to every other core at once. Test with `IRIW+addrs`: if two readers can see
   two independent writes in opposite orders, the machine is not MCA.
2. **Why is `MP` allowed on Arm but forbidden on x86?**
   x86 keeps stores in order and loads in order (TSO). Arm may reorder both unless a dependency,
   barrier or acquire/release prevents it.
3. **What is the cheapest fix for `MP` on the reader side?**
   An address dependency (free, but fragile: compilers can remove it) or `LDAR` on the flag. On the
   writer side: `STLR` on the flag, or `DMB ST` between the stores.
4. **Why does a control dependency order a later store but not a later load?**
   A core can speculate a load past an unresolved branch and use the value if the branch goes the
   predicted way. It cannot let a speculative store become visible to other cores. Hence `ctrl;[W]`
   is in `dob`, and loads need the `ISB`.
5. **`PPOCA` is allowed and `PPOAA` forbidden. Why?**
   A store after an unresolved branch can be forwarded to a later load on the same thread before
   the branch resolves. With an address dependency the store's address isn't known, so it cannot
   be forwarded early. The model writes this as `(addr|data);rfi` with no `ctrl;rfi`.
6. **What does `LDAPR` give up compared with `LDAR`?**
   The `[L];po;[A]` ordering: a store-release followed by `LDAR` is ordered, followed by `LDAPR` it
   is not. `SB+dmb.sy+rel-acqpc` is allowed; `SB+dmb.sy+rel-acq` is forbidden.
7. **How did you know your checker was right?**
   It was compared against the paper's published results as whole state sets, never verdicts typed
   by hand. And each axiom was removed to show the tests can tell.
8. **Where does your checker disagree with today's herd model, and why?**
   In 9 tests, on `CSEL`, atomics with no return register, and `CAS` register dependencies:
   instruction semantics refined after 2018.
9. **How would this help in a GPU driver?** (Your own analogy; the paper does not discuss GPUs.)
   Driver/GPU communication is `MP`: write the command, then ring the doorbell. Knowing which
   barrier or release is needed, and testing it on the real core, prevents rare corruptions that
   only appear under load.
10. **If a forbidden state showed up on the phone, what would you do?**
    Suspect the harness first: pinning, reset between instances, a wrongly generated instruction.
    Then reproduce it with fewer moving parts. Only then report it as a hardware question.
11. **What did your phone actually show?**
    No forbidden outcome in 147 million runs. Every allowed relaxation that showed up needs only a
    read to overtake something: `SB`, `R`, `MP`, `IRIW`, `PPOCA`. `LB`, `2+2W`, `S` and
    `MP+po+addr` never showed up; each needs a store to overtake. So my hypothesis is that these
    Oryon cores keep stores in order. That is stronger than the architecture promises, so code must
    still use the barriers.
12. **Why synchronise threads on a timer tick and not a barrier?**
    A test body takes nanoseconds, and a barrier's release reaches the other cores at different
    times. The threads then run one after another, not together. The generic timer gives every core
    the same count, so "start at tick T" lines them up. `SB` went from 44 to 399,821 sightings.

## 5. Try it yourself

1. `./model/mca-check tests/hand/MP+dmb.sy+ctrl.litmus`, then `…ctrlisb`. Explain the difference
   using the `dob` clause that changes.
2. In `model.cpp`, remove `| ran(ctrl, W)` (the paper's `ctrl;[W]`) from `dob`, rebuild and run
   `tools/oracle.py`. Which tests change? Put it back.
3. Write `SB+dmb.st+dmb.st` and predict it before running it. Does `DMB ST` order a write against a
   later read?
4. Run `mca-hw --info` on your phone, look its CPU part numbers up in Arm's documentation, and
   add them to FINDINGS.
