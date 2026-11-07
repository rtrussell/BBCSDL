# Plan

Status: DRAFT v0.2 (2026-11-07).

| ID | Step | Spec | Status |
|----|------|------|--------|
| P0 | Planning docs (architecture/, Changes.md) | S9 | done |
| P1 | Clarify scope | S1–S8 | done (except D1–D3) |
| P2 | Decide byte order (D1) and alignment strategy (D2) | S2.4, S2.5 | open |
| P3 | Host-side check: build the console edition on Linux, run the test programs → reference outputs | S6.3 | todo |
| P4 | Build skeleton: makefile in docker, `bbdata_m68k`, stub assembler, OS glue (`bbccon` replacement for rosco) → links | S3, S5 | todo |
| P5 | Boot to `>` prompt on r68k; PRINT, loops, strings | S5.1, S5.2 | todo |
| P6 | Fix alignment/endianness faults until the test programs pass on r68k | S2.4, S2.5, S6.3 | todo |
| P7 | Files on SD card, TIME, ESCape, *BYE | S5.2 | todo |
| P8 | Run on MAME; size and benchmark comparison vs bbc-basic-68k | S6.1, S6.2 | todo |
| P9 | Compatibility comparison report | S6.3, S6.5 | todo |
| P10 | CI workflow (pinned SHAs / digests) | S8, S9 | todo |
| P11 | ACK build and gcc-vs-ACK comparison | S3.2, S6.4 | later |
| P12 | Inline assembler (D3) | S5.4 | later |
