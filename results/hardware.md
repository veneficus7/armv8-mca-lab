# The silicon against the model

## SM-S938B

CPUs: part 0x001 (implementer 0x51) up to 3532 MHz; part 0x001 (implementer 0x51) up to 4473 MHz. LSE atomics: True. LDAPR: True.

thermal before: Thermal Status: 0
thermal after: Thermal Status: 0

### Threads synchronised by the shared counter (time base)

| Test | Model | cluster-3532MHz | cluster-4473MHz | cross | unpinned |
| --- | --- | --- | --- | --- | --- |
| SB | Sometimes | 61,117 / 1,000,000 | 75,575 / 1,000,000 | 142,470 / 1,000,000 | 120,659 / 1,000,000 |
| MP | Sometimes | 7,409 / 1,000,000 | 17,830 / 1,000,000 | 5 / 1,000,000 | 524 / 1,000,000 |
| LB | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| 2+2W | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| R | Sometimes | 77,367 / 1,000,000 | 31,307 / 1,000,000 | 20 / 1,000,000 | 9,121 / 1,000,000 |
| S | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| IRIW | Sometimes | 30 / 1,000,000 | skipped (4 threads, 2 distinct CPUs) | 0 / 1,000,000 | 4,888 / 1,000,000 |
| PPOCA | Sometimes | 461 / 1,000,000 | 143 / 1,000,000 | 5 / 1,000,000 | 34 / 1,000,000 |
| MP+po+addr | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+ctrl | Sometimes | 3,265 / 1,000,000 | 6,456 / 1,000,000 | 5 / 1,000,000 | 265 / 1,000,000 |
| SB+dmb.sy+rel-acqpc | Sometimes | 35,877 / 1,000,000 | 7,861 / 1,000,000 | 7,022 / 1,000,000 | 33,321 / 1,000,000 |
| CoRR | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoRW1 | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoRW2 | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoWR | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoWW | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| SB+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| 2+2W+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| R+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| S+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+rel+acq | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+rel+acqpc | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| SB+dmb.sy+rel-acq | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+rel+SWPacq | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+addr | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.st+addr | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+dmb.ld | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+ctrlisb | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+datas | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+addrs | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+ctrls | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| WRC+data+addr | Never | 0 / 1,000,000 | skipped (3 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |
| ISA2+dmb.sy+data+addr | Never | 0 / 1,000,000 | skipped (3 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |
| PPOAA | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| IRIW+addrs | Never | 0 / 1,000,000 | skipped (4 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |
| WRC+addrs | Never | 0 / 1,000,000 | skipped (3 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |

- Forbidden states seen, all tests and configurations: **0**
- Positive control, SB's relaxed state seen: **399,821**
- Allowed by the model, never observed here: LB, 2+2W, S, MP+po+addr
- Threads that did not stay on their requested CPU: none

### Threads synchronised by a spin barrier per instance

| Test | Model | cluster-3532MHz | cluster-4473MHz | cross | unpinned |
| --- | --- | --- | --- | --- | --- |
| SB | Sometimes | 21 / 1,000,000 | 11 / 1,000,000 | 2 / 1,000,000 | 10 / 1,000,000 |
| MP | Sometimes | 103 / 1,000,000 | 5 / 1,000,000 | 0 / 1,000,000 | 2 / 1,000,000 |
| LB | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| 2+2W | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| R | Sometimes | 13 / 1,000,000 | 1 / 1,000,000 | 0 / 1,000,000 | 9 / 1,000,000 |
| S | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| IRIW | Sometimes | 0 / 1,000,000 | skipped (4 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |
| PPOCA | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 2 / 1,000,000 |
| MP+po+addr | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+ctrl | Sometimes | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 1 / 1,000,000 |
| SB+dmb.sy+rel-acqpc | Sometimes | 2 / 1,000,000 | 2 / 1,000,000 | 0 / 1,000,000 | 1 / 1,000,000 |
| CoRR | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoRW1 | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoRW2 | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoWR | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| CoWW | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| SB+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| 2+2W+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| R+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| S+dmb.sys | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+rel+acq | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+rel+acqpc | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| SB+dmb.sy+rel-acq | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+rel+SWPacq | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+addr | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.st+addr | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+dmb.ld | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| MP+dmb.sy+ctrlisb | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+datas | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+addrs | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| LB+ctrls | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| WRC+data+addr | Never | 0 / 1,000,000 | skipped (3 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |
| ISA2+dmb.sy+data+addr | Never | 0 / 1,000,000 | skipped (3 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |
| PPOAA | Never | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 | 0 / 1,000,000 |
| IRIW+addrs | Never | 0 / 1,000,000 | skipped (4 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |
| WRC+addrs | Never | 0 / 1,000,000 | skipped (3 threads, 2 distinct CPUs) | 0 / 1,000,000 | 0 / 1,000,000 |

- Forbidden states seen, all tests and configurations: **0**
- Positive control, SB's relaxed state seen: **44**
- Allowed by the model, never observed here: LB, 2+2W, S, IRIW, MP+po+addr
- Threads that did not stay on their requested CPU: none

