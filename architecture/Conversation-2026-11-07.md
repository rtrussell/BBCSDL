# Conversation log — 2026-11-07

## Session setup
- User set working rules: daily conversation log, Plan.md, Spec.md, Tests.md
  (tests traced to Spec items), regular Spec/Plan/code crosschecks, confirm
  corrections to unclear input before proceeding, `Changes.md` per release.
- GitHub Actions / action steps: pin to full commit SHA (tag in a comment),
  use the latest workable version, explain when that is not possible.
- Confirmed: (1) docs folder `architecture/`; (2) commit on the existing branch
  `mboot-68k-dev` (repo is a clone of rtrussell/BBCSDL, not a new project);
  (3) `Changes.md` at repo root.

## Scenario (as given, pending confirmation of corrections)
> we have done a port of R.T. Russels BBCBASIC Z80 to the rosco-m68k board and
> now want to compate that to a imlenetation of this project.

References given by the user:
- board: `../rosco-m68k/` (actually present as `../rosco_m68k/`)
- ported BASIC: `../bbc-basic-68k/` (Z80 → 68k assembly translation)
- compilers: ACK at `../ack`; alternative: rosco's dockerized gcc-13
- emulators: `../r68k` (fast, inaccurate timing), `../rosco-emulator` (MAME, rosco-only build)

Open questions sent to the user — see the next entry once answered.

## Answers (user)
"yes to all, console only, 68010 bare metal, gcc first" — corrections accepted;
edition: console only; CPU 68010, bare metal (no Minix); gcc-13 first, ACK later;
compare gcc vs ACK; comparison criteria as proposed; code under this branch; CI yes.
Q3 (inline assembler) was a/b/c: read as phase 1 = none (a), decision later (D3).

## Investigation
- Board refs at `../rosco_m68k`; gcc image `roscopeco/rosco_m68k` = m68k-elf-rosco-gcc
  13.3.1 + newlib; digest `sha256:91fd8f0c…ad205` (2024-11-03).
- bbc-basic-68k bare build uses ACK (tools/mkbare.py), not gcc; benchmarks
  (BM1-BM8, sieve, EhBASIC) are being prepared there (Handover-benchmark-2026-11-07).
- BBCSDL portability findings:
  - Assumes little-endian throughout; no byte-order handling anywhere → D1.
  - ~110 raw pointer casts (`*(int*)…`), 95 in bbexec.c; on the 68010 an odd
    address faults → D2.
  - `VAR` uses `long double` except on ARM/wasm → use `double` on m68k.
  - Register globals per CPU (esi/esp); the `__llvm__` path uses plain globals → use that.
  - bbccon.c uses pthreads, termios, timer_create, mmap, dlopen → rosco glue needed.
  - Data module exists in C (`bbdata_wasm32.c`) → model for m68k.
- Spec v0.2, Plan v0.2, Tests T1–T13 written.

## Paused
Asked D1 (byte order: a native BE / b LE like bbc-basic-68k / c BE with LE
files) and D2 (unaligned access: a source helpers / b fault handler);
recommended D1a + D2a. User: "we will continue later when upstream has a
preference." → Work paused at P2, waiting for upstream (R.T. Russell) input on D1/D2.
