# Tests

Each test is traced to a Spec item.

| ID | Spec | Description | Status |
|----|------|-------------|--------|
| T1 | S3.1 | `make` in the pinned docker image produces `ROSCODE1.BIN` with no warnings in port files | todo |
| T2 | S2.4 | Boot on r68k with odd-address faults enabled: no address error during start-up and the test suite | todo |
| T3 | S5.1 | `PRINT 1+1`, `PRINT "A"+"B"`, `FOR`/`NEXT`, `REPEAT`, `DEF PROC/FN` give the reference output | todo |
| T4 | S2.6 | Float output: `PRINT PI`, `SQR2`, `EXP1`, `1/3` match the Linux reference (double) | todo |
| T5 | S2.5 | `!`, `?`, `$`, `^` indirection consistent with the chosen byte order (D1) | todo |
| T6 | S5.2 | `TIME` advances 100/s (±1 %) on MAME | todo |
| T7 | S5.2 | `SAVE`/`LOAD`/`OPENOUT`/`BPUT#`/`BGET#` round-trip on the SD card | todo |
| T8 | S5.2 | ESCape stops a running loop and returns to `>` | todo |
| T9 | S2.3 | Starts with 1 MB and with 5 MB; `PRINT HIMEM-LOMEM` recorded | todo |
| T10 | S5.4 | `[` gives an error, no crash | todo |
| T11 | S6.2 | Benchmarks run unmodified (except timing lines) on both BASICs on MAME | todo |
| T12 | S6.3 | Shared test programs: outputs diffed and differences classified | todo |
| T13 | S8.1, S9 | CI: every `uses:` pinned to a 40-char SHA; image pinned by digest | todo |
