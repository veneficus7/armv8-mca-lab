# The paper: *Simplifying ARM Concurrency* (POPL 2018)

Christopher Pulte, Shaked Flur, Will Deacon (ARM Ltd.), Jon French, Susmit Sarkar, Peter Sewell.
*Simplifying ARM Concurrency: Multicopy-Atomic Axiomatic and Operational Models for ARMv8.*
Proc. ACM Program. Lang. 2, POPL, Article 19 (January 2018). [doi:10.1145/3158107](https://doi.org/10.1145/3158107) ·
[paper, tests and model results](https://www.cl.cam.ac.uk/~pes20/armv8-mca/)

**In one sentence:** Arm changed its architecture's memory model to be *multicopy-atomic*. This
paper explains why, gives the new model formally in two styles (axiomatic and operational), and
proves the two agree.

## Why it was chosen

Five candidates were compared on four criteria: hard; about the Arm architecture itself; checkable,
with an executable model and published results; and testable on hardware already owned (Android
phones).

| Paper | Hard | Arm architecture | Checkable | On a phone |
| --- | --- | --- | --- | --- |
| **Pulte et al., POPL 2018 (this)** | formal relational semantics, two models, equivalence proof | the memory model in the Arm ARM | the paper's own tests and ~8,900 model results | yes: any Armv8 core |
| Stephens et al., *The ARM Scalable Vector Extension*, IEEE Micro 2017 | yes | yes | no executable model | needs SVE silicon; its point is performance |
| Reid, *Trustworthy Specifications of ARM v8-A and v8-M*, FMCAD 2016 | yes | yes | validated with Arm's internal test suite | an emulator, not a measurement |
| Armstrong et al., *ISA Semantics for ARMv8-A, RISC-V, and CHERI-MIPS*, POPL 2019 | yes | yes | yes | a project the size of a toolchain |
| Alglave et al., *Armed Cats*, TOPLAS 2021 | yes | Arm's current model | its herd model has grown far past 2018 | partly |

This paper is also the one that **changed the architecture**: a research collaboration that ended
with the Arm Architecture Reference Manual carrying a formal model.

## The problem

Each core of an Arm phone runs instructions out of order and speculatively, and buffers its stores.
So another core can see one thread's memory accesses in a different order from the program's. Two
things decide which reorderings are possible:

- **Inside a thread:** what a core may reorder, and what stops it. That means *dependencies* (an
  address, data or control path from a load to a later access), *barriers* (`DMB`, `ISB`) and
  *acquire/release* accesses (`LDAR`, `STLR`, `LDAPR`).
- **Between threads:** whether a write becomes visible to every other core at the same moment.
  That property is **multicopy atomicity (MCA)**.

ARMv7 and early ARMv8 were specified in prose, and that prose was **non-MCA**: a write could reach
some cores before others, as on IBM POWER. That freedom made the model much more complex, for
example "barrier cumulativity". Yet no production ARMv8 implementation from any vendor used it. Arm
confirmed this with its architecture partners.

## What changed in ARMv8 (§4)

1. **MCA.** A write is seen by every other core at once (Arm calls this "other-multicopy-atomic":
   the writing core may still read its own write early). This forbids tests such as `IRIW+addrs`
   and `WRC+addrs`, and barrier cumulativity is no longer needed.
2. **No write subsumption.** A data-dependent write is now ordered before later writes to the
   same address.
3. **Simpler dependencies**, defined syntactically through registers.
4. **A formal model in the architecture document**, written by Will Deacon in herd's `cat`
   language.

What remains relaxed comes from each core alone: out-of-order and speculative execution, and
store buffering. A thread can still read its own write before other threads see it, even on a
speculative path (the `PPOCA` test).

## The axiomatic model (§7), the part implemented here

A *candidate execution* is a set of events (reads, writes, barriers) and relations over them:
program order `po`, reads-from `rf`, coherence order `co` (the order of all writes to one location),
the derived from-reads `fr = rf⁻¹;co`, and the dependencies `addr`, `data`, `ctrl` and `rmw`. Each
splits into an internal part (same thread, `-i`) and an external one (`-e`). On page 19:

```
let ca  = fr | co                                     (* coherence-after *)
let obs = rfe | fre | coe                             (* observed-by *)
let dob = addr | data | ctrl;[W]                      (* dependency-ordered-before *)
        | (ctrl | (addr;po));[ISB];po;[R]
        | addr;po;[W] | (ctrl | data);coi | (addr | data);rfi
let aob = rmw | [range(rmw)];rfi;[A | Q]              (* atomic-ordered-before *)
let bob = po;[dmb.full];po | [L];po;[A]               (* barrier-ordered-before *)
        | [R];po;[dmb.ld];po | [A | Q];po
        | [W];po;[dmb.st];po;[W] | po;[L] | po;[L];coi
let ob  = (obs | dob | aob | bob)+                    (* ordered-before *)
acyclic po-loc | ca | rf as internal
irreflexive ob as external
empty rmw & (fre; coe) as atomic
```

- **internal:** each location on its own behaves as if there were one copy ("SC per location").
- **external:** the thread-local orderings and the inter-thread observations together never form
  a cycle. MCA lives here: `obs` only relates *different* threads, and a write is observed or
  not, with no "visible to some but not others".
- **atomic:** nothing writes to the location between an atomic's read and its write.

The whole model fits on half a page. That brevity is the point of the paper: it was much longer
before the change.

**`IRIW+addrs` shows the change.** Two writers, `x=1` and `y=1`, and two readers that read the
two in opposite orders, each with an address dependency between its two loads. The paper's ob
cycle is `rfe; addr; fre; rfe; addr; fre`. Each step is in `ob`, so the outcome is forbidden. A
non-MCA machine would allow it, because the readers could see the writes propagate in different
orders.

**`PPOCA` against `PPOAA`.** Same test, except that `PPOCA` has a control dependency to a write and
`PPOAA` has an address dependency. `dob` has `(addr | data);rfi` but no `ctrl;rfi`. So a core may
forward a speculative write (after an unresolved branch) to a later read on its own thread: `PPOCA`
is allowed and `PPOAA` forbidden. That is a microarchitectural fact, store-to-load forwarding on a
speculative path, written as one missing clause.

## The rest of the paper

- **Flat (§6):** an operational model, an abstract machine with out-of-order, speculative threads
  over a single flat memory, which is what MCA allows. It is executable in the authors' `rmem` tool.
- **Equivalence (§8):** Flat and the axiomatic model allow the same behaviours. Per the paper's own
  caveat, this is **a hand proof, not mechanised**, for finite executions and the intersection of
  the two models' coverage.
- **Validation (§9):** both models were checked against each other and against hardware runs of
  litmus tests.
- **rmem and a Linux spinlock (§10–11):** exhaustive checking of a loop-unrolled kernel lock found
  a previously known bug caused by unprevented speculation, and confirmed the fixed version.

## Limits, by the paper's own account

- The axiomatic model has no mixed-size accesses (a 32-bit read of half a 64-bit write) and no
  integration with the full instruction set. Flat has both, but not `LDAPR` or the Armv8.1 atomics.
- Neither covers systems features: virtual memory, page-table walks, TLB maintenance.
- The proof is by hand.

## What happened next

Arm now develops its concurrency model formally, in herd's `cat` language (Alglave, Deacon,
Grisenthwaite, Hacquard and Maranget, *Armed Cats*, TOPLAS 2021). Arm's newsroom says the
specification text is now generated from that formal model. herdtools7's current `aarch64.cat` is
192 lines. It covers explicit and implicit memory effects, MTE tags, TLB invalidation and page-table
updates, and handles instructions such as `CSEL` with "pick" dependencies. That last point accounts
for differences this project found against today's verdicts (see [FINDINGS.md](FINDINGS.md)). So
this repository implements **the 2018 model**, and says so.

## Why it matters for a GPU software team (this project's view, not the paper's)

A GPU driver is concurrent code on Arm cores that share memory with a GPU. Command buffers,
doorbells, fences and ring-buffer indices are message-passing patterns: the `MP` shape, where a
missing barrier or acquire shows up as a rare corruption under load. Knowing which orderings the
architecture guarantees, and testing for them on real cores, is the skill this paper and this
project build.
