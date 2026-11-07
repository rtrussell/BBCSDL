# Spec — BBC BASIC console edition (BBCTTY) on rosco-m68k, compared with bbc-basic-68k

Status: DRAFT v0.2 (2026-11-07). Open decisions are marked **OPEN**.

## S1 Goal
S1.1 Port this project's portable C interpreter (console edition: `bbmain.c`,
     `bbexec.c`, `bbeval.c`, `bbccos.c`, `bbccon.c` + data module) to the rosco-m68k.
S1.2 Compare it with the assembly port `../bbc-basic-68k` (derived from BBCZ80).
S1.3 Out of scope: SDL editions, graphics, sound, Minix (for now).

## S2 Target
S2.1 CPU: MC68010 (rosco_m68k Classic), no FPU → software floating point.
S2.2 Runtime: bare metal on the rosco firmware (v2.42+), loaded as `ROSCODE1.BIN`
     from the SD card, console on the serial port.
S2.3 Memory: the board's 1 MB plus the 4 MB expansion (5 MB) is the reference
     configuration, as for bbc-basic-68k; must still start with 1 MB.
S2.4 The 68010 faults on odd-address word/long accesses: the port must never
     perform them (r68k emulates this fault).
S2.5 Byte order: the 68k is big-endian; BBCSDL assumes little-endian. **OPEN** (D1).
S2.6 Floating point: `double` (64-bit) as on ARM, not `long double`
     (m68k 96-bit extended would be emulated in software and be slow).

## S3 Toolchain
S3.1 Primary: `m68k-elf-rosco-gcc` 13.3.1 (newlib) from the docker image
     `roscopeco/rosco_m68k@sha256:91fd8f0cf4a852691e93fa5c2b7cb1090cc1227baf2a7aa356a8b51ae18ad205`
     (pinned by digest; tag `latest`, created 2024-11-03), `-mcpu=68010`.
S3.2 Secondary (later phase): ACK from `../ack`. Expect C99/GNU-extension patches.
S3.3 Build products: `ROSCODE1.BIN` (+ ELF, map) under `build/rosco/`.

## S4 Emulators
S4.1 r68k (`../r68k`): functional tests, fast, inaccurate timing.
S4.2 MAME rosco-emulator (`../rosco-emulator`, image `rosco-emulator:local`):
     timing measurements (same CPU timing as the board).

## S5 Port features
S5.1 Interpreter: all of the non-graphics language the console edition supports.
S5.2 OS layer: serial console in/out, `TIME` from the firmware 100 Hz tick
     (`SDB_UPTICKS`), ESCape detection, files on the SD card (FAT) via the
     firmware/newlib, `*BYE` returns to the firmware.
S5.3 No threads, no `mmap`, no `dlopen`: replaced by single-threaded polling.
S5.4 Inline assembler: phase 1 none (`[` gives an error); a 68k assembler is a
     later decision (D3).
S5.5 Data module: a C or GNU-as `bbdata` for m68k (models: `bbdata_wasm32.c`,
     `bbdata_arm_32.s`).

## S6 Comparison (vs bbc-basic-68k)
S6.1 Size: code/data size of the binary, free memory for BASIC at start-up.
S6.2 Speed: the same benchmark programs (Rugg/Feldman BM1-BM8, sieve, as in
     bbc-basic-68k's benchmark work), timed on MAME; the board for key runs.
S6.3 Compatibility: the same BASIC test programs on both, outputs diffed;
     differences classified (bug / dialect difference BBC BASIC for Windows vs Z80).
S6.4 gcc vs ACK builds compared as well, when the ACK build exists.
S6.5 Report: Markdown in `architecture/` (or `tmp/`), tables per criterion.

## S7 Source layout
S7.1 Work lives on branch `mboot-68k-dev`. Port-specific files under `console/rosco/`
     (makefile, startup, OS glue) and `src/` (CPU-specific `bbdata_m68k.*`).
S7.2 Changes to the shared interpreter files are minimal and guarded by
     `#ifdef __m68k__`, so they can be offered upstream.

## S8 CI
S8.1 GitHub Actions on this fork: build in the pinned rosco image, smoke-test on r68k.

## S9 Project rules
S9.1 CI action steps are pinned to a full commit SHA, with the version tag in a
     comment; the latest workable version is used, or the reason it can't be is documented.
S9.2 Container images are pinned by digest for the same reason.

## Open decisions
- **D1 Byte order** (S2.5) — see Conversation log.
- **D2 Unaligned accesses** (S2.4) — strategy for ~110 raw pointer casts
  (mostly `bbexec.c`).
- **D3 Inline assembler** (S5.4).
