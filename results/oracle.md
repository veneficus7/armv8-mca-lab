# mca-check against published answers

- Whole state sets equal to the paper's published ARMv8-axiomatic logs: **45 of 45**
- A log under the same name but for a different version of the test (its condition differs): 6
- Verdicts equal to herdtools7 `kinds.txt` (current model, tests with no paper log): **33 of 42**
- Not supported by mca-check: 5

| Test | Reference | Reference says | mca-check says | |
| --- | --- | --- | --- | --- |
| 2+2W+dmb.sy+po | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| 2+2W+dmb.sys | paper log | 3 states, Never | 3 states, Never | match |
| 2+2W | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| 3.LB+poals | paper log | 7 states, Never | 7 states, Never | match |
| 3.SB+polas | paper log | 7 states, Never | 7 states, Never | match |
| CAS+data1 | kinds.txt | Allowed | Allowed | match |
| CAS+data2 | kinds.txt | Allowed | Allowed | match |
| IRIW+addrs | paper log | 15 states, Never | 15 states, Never | match |
| IRIW+poaas+LL | paper log | 15 states, Never | 15 states, Never | match |
| IRIW | paper log | 16 states, Sometimes | 16 states, Sometimes | match |
| ISA2+dmb.sy+data+addr | paper log | 7 states, Never | 7 states, Never | match |
| ISA2+poll+poal+poaa | paper log | 7 states, Never | 7 states, Never | match |
| LB+BEQ4 | kinds.txt | Forbidden | Forbidden | match |
| LB+CSEL4 | kinds.txt | Allowed | Allowed | match |
| LB+SWP-RsRt-addr+rel | kinds.txt | Allowed | Allowed | match |
| LB+addrs | paper log | 3 states, Never | 3 states, Never | match |
| LB+ctrls | paper log | 3 states, Never | 3 states, Never | match |
| LB+datas | paper log | 3 states, Never | 3 states, Never | match |
| LB+dmb.sy+data-amo.swp | kinds.txt | Forbidden | Forbidden | match |
| LB+dmb.sy+po | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| LB+dmb.sys | paper log | 3 states, Never | 3 states, Never | match |
| LB+popl+data-rmw | kinds.txt | Forbidden | Forbidden | match |
| LB+rel+BEQ | kinds.txt | Allowed | Allowed | match |
| LB+rel+BEQ2 | kinds.txt | Forbidden | Forbidden | match |
| LB+rel+BEQ3 | kinds.txt | Allowed | Allowed | match |
| LB+rel+CAS+BIS | kinds.txt | Forbidden | Forbidden | match |
| LB+rel+CAS | kinds.txt | Forbidden | Allowed | DIFFERENT |
| LB+rel+CSEL | kinds.txt | Allowed | Forbidden | DIFFERENT |
| LB+rel+CSEL2 | kinds.txt | Allowed | Allowed | match |
| LB+rel+CSEL3 | kinds.txt | Allowed | Allowed | match |
| LB+rel+LDADD | kinds.txt | Forbidden | Forbidden | match |
| LB+rel+STADD | kinds.txt | Forbidden | Forbidden | match |
| LB | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| LB | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| MP+dmb.st+addr | paper log | 3 states, Never | 3 states, Never | match |
| MP+dmb.sy+addr-[ws-rf]-addr | paper log | 9 states, Never | 9 states, Never | match |
| MP+dmb.sy+addr | paper log | 3 states, Never | 3 states, Never | match |
| MP+dmb.sy+ctrl | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| MP+dmb.sy+ctrlisb | paper log | 3 states, Never | 3 states, Never | match |
| MP+dmb.sy+dmb.ld | paper log | 3 states, Never | 3 states, Never | match |
| MP+dmb.sy+po | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| MP+dmb.sys | paper log | 3 states, Never | 3 states, Never | match |
| MP+po+addr | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| MP+po+dmb.sy | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| MP+rel+CAS-addr | kinds.txt | Allowed | Allowed | match |
| MP+rel+CAS-ok-MRs-addr | kinds.txt | Allowed | Forbidden | DIFFERENT |
| MP+rel+CAS-ok-RsRs-addr | kinds.txt | Allowed | Allowed | match |
| MP+rel+CAS-ok-dmb.ld | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+CASacq-noret-ok | kinds.txt | Allowed | Forbidden | DIFFERENT |
| MP+rel+CASacq-ok | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+CASnoret-ok-dmb.ld | kinds.txt | Allowed | Forbidden | DIFFERENT |
| MP+rel+CSEL | kinds.txt | Allowed | Forbidden | DIFFERENT |
| MP+rel+LDADD-dmb.ld | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+LDADDnoret-dmb.ld | kinds.txt | Allowed | Forbidden | DIFFERENT |
| MP+rel+SWP-dmb.ld | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+SWPacq-noret | kinds.txt | Allowed | Forbidden | DIFFERENT |
| MP+rel+SWPacq | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+SWPnoret-dmb.ld | kinds.txt | Allowed | Forbidden | DIFFERENT |
| MP+rel+acq | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+acqpc | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+addr-lrs-acq | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+addr-po-loc-addr | kinds.txt | Allowed | Allowed | match |
| MP+rel+ctrl-lrs-acq | kinds.txt | Allowed | Allowed | match |
| MP+rel+data-lrs-acq | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+rmw-lrs-acq | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+swp-acq | kinds.txt | Forbidden | Forbidden | match |
| MP+rel+swp-acqpc | kinds.txt | Forbidden | Forbidden | match |
| MP | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| PPOAA | paper log | 3 states, Never | 3 states, Never | match |
| PPOCA | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| R+dmb.sy+po | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| R+dmb.sys | paper log | 3 states, Never | 3 states, Never | match |
| R+po+dmb.sy | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| R | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| S+dmb.sy+po | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| S+dmb.sys | paper log | 3 states, Never | 3 states, Never | match |
| S+po+dmb.sy | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| S | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| SB+dmb.sy+po | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| SB+dmb.sy+rel-acq | kinds.txt | Forbidden | Forbidden | match |
| SB+dmb.sy+rel-acqpc | kinds.txt | Allowed | Allowed | match |
| SB+dmb.sys | paper log | 3 states, Never | 3 states, Never | match |
| SB | paper log | 4 states, Sometimes | 4 states, Sometimes | match |
| WRC+addrs | paper log | 7 states, Never | 7 states, Never | match |
| WRC+data+addr | paper log | 7 states, Never | 7 states, Never | match |
| WRC+poal+poaa+L | paper log | 7 states, Never | 7 states, Never | match |
| Z6.4+poll+pola+pola | paper log | 7 states, Never | 7 states, Never | match |

## Each axiom switched off

- `--no-internal` changes the allowed states of 21 tests: CoRR, CoRW1, CoRW2, CoWR, CoWW, LB+CAS-rfi-ctrl+DMBSY, LB+data+data-wsi, MP+dmb.sy+addr-[ws-rf]-addr, MP+rel+SWP-dmb.ld, MP+rel+SWPnoret-dmb.ld, MP+rel+addr-lrs-acq, MP+rel+data-lrs-acq …
- `--no-external` changes the allowed states of 58 tests: 2+2W+dmb.sys, 3.LB+poals, 3.SB+polas, IRIW+addrs, IRIW+poaas+LL, IRIW+poaps+LL, ISA2+dmb.sy+data+addr, ISA2+poll+poal+poaa, LB+BEQ4, LB+CAS-rfi-ctrl+DMBSY, LB+addrs, LB+ctrls …
- `--no-atomic` changes the allowed states of 5 tests: MP+rel+SWP-dmb.ld, MP+rel+SWPacq, MP+rel+SWPacq-noret, MP+rel+SWPnoret-dmb.ld, R+CAS+DMBLD

## Same name, different test

The paper's log and this file share a name but not a condition, so their states are not compared.

| Test | This file | The paper's log | |
| --- | --- | --- | --- |
| CoRR | `exists (1:X1=1 /\ 1:X2=0)` | `exists (not (x=0x1 /\ (1:X0=0x0 /\ (1:X2=0x0 \/ 1:X2=0x1) \/ 1:X0=0x1 /\ 1:X2=0x1)))` | states differ |
| CoRW1 | `exists (0:X1=1)` | `exists (not (0:X0=0x0 /\ x=0x1))` | states differ |
| CoRW2 | `exists ([x]=1 /\ 1:X1=1)` | `exists (not (1:X0=0x0 /\ (x=0x2 \/ x=0x1) \/ 1:X0=0x1 /\ x=0x2))` | states differ |
| CoWR | `exists (0:X2=0)` | `exists (x=0x1 /\ 0:X2=0x2)` | states differ |
| CoWW | `exists ([x]=1)` | `exists (not (x=0x2))` | states agree on shared variables |
| LB+data+data-wsi | `exists
(x=2 /\ 0:X0=2 /\ 1:X0=1)` | `exists (x=0x2 /\ y=0x1 /\ 0:X0=0x2 /\ 1:X0=0x1)` | states agree on shared variables |

## Not supported

- LB+rel+CAS-ok-MRs-addr: parse: expected = in condition near 0:
- LB+rel+CAS-ok-RsRs-addr: parse: expected = in condition near 0:
- LB+rel+data-post: parse: post-indexed addressing is not supported: STR W2,[X3],#4
- MP+rel+CAS-ok-bothRs-addr: parse: not a number: 5;
- MP+rel+CSEL-addr: parse: not an AArch64 test: ../../aarch64-pick/tests//MP+rel+CSEL-addr.litmus
