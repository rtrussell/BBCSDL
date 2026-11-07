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
