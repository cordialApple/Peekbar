# Outward-facing docs restructure: plan (from Opus consult, 2026-07-11)

Advisory plan from an Opus consult on making `docs/` presentable to outside
readers (GitHub visitors, portfolio viewers, contributors) without breaking
CLAUDE.md's session-start protocol, which hard-codes `docs/HANDOFF.md`,
`docs/plans/stage-N.md`, and `docs/ARCHITECTURE.md`.

## Progress so far

- [x] `LICENSE`: MIT
- [x] `SECURITY.md`: titles-only reads, no URL capture, no process injection,
      no network calls, no log files, ETW-only diagnostics via separate tool
- [x] `README.md` rewritten human-first (pitch, status, install, stack,
      roadmap, contributing); screenshot/GIF left as a `TODO` placeholder
      pointing at `docs/media/` (not yet created)
- [x] Windows screen-captures recorded (8 clips + 4 stills in
      `~/Downloads/browser_progress_artifacts/`) and analyzed frame-by-frame.
      This unblocks the hero media gap.

      Footage is usable as-is (no sanitizing needed: the visible email/tab
      titles are fine to show); it just needs cropping to the taskbar strip.
      Editing plan + ffmpeg commands live in
      [`media-strategy.md`](media-strategy.md).
- [ ] Everything else below

## Direction: go public-facing and polished

The goal has firmed up from "presentable docs" to "make this genuinely
public-facing and polished": README hero media plus a LinkedIn demo reel from
the same source footage. Two audiences now drive the work:

- **Repo visitor / portfolio viewer** → README hero GIF + stills in
  `docs/media/`, `docs/overview.md`, honest status.
- **LinkedIn / social** → a 45 to 60s captioned reel (out-of-repo deliverable)
  that reuses the same clips. Storyboard in `media-strategy.md`.

The captured footage is usable as-is: cropping it to the taskbar strip (ffmpeg
commands in `media-strategy.md`) already yields README GIFs; the visible
email/tab titles are fine to show and are not a concern.

Optional single re-record only if you want a crisp one-take *minimize → chips →
hover-fan → click-restore* hero (the fan moment is scattered across the current
clips), for choreography, not privacy. Full inventory, grades, clip windows,
and ffmpeg commands: `media-strategy.md`.

## The tension (why this needs care)

Current docs (`HANDOFF.md`, `ARCHITECTURE.md`, `plans/`) are a build log for
one reader: the next Claude Code session. HANDOFF.md is a 926-line
chronological debugging saga; ARCHITECTURE.md narrates dead ends (AppBar era)
so the agent doesn't reintroduce them.

None of this is what a stranger wants: (a) what is this / can I see it
working, (b) does it work, honestly, (c) how do I install it, (d) why should I
trust a tool that hooks the taskbar and reads browser windows via UIA, (e) how
do I contribute.

Fix is **additive, not destructive**: build a human-facing layer alongside
the AI triad, don't move or rename `docs/HANDOFF.md`, `docs/ARCHITECTURE.md`,
or `docs/plans/stage-N.md` unless CLAUDE.md's session-start paths are updated
in the *same* commit (that's "Option B" below, deferred).

## Proposed tree (Option A: chosen; zero risk to session protocol)

```
README.md              REWRITTEN (done) - human-first, AI pointer at bottom
LICENSE                NEW (done) - MIT
SECURITY.md            NEW (done) - privacy/trust stance
CONTRIBUTING.md        NEW (todo) - build prereqs, how to propose changes,
                        pointer to CLAUDE.md for AI agents
CLAUDE.md              untouched
docs/
  README.md            NEW (todo) - doc index, human vs. dev track
  install.md           NEW (todo) - build/run/autostart/uninstall/troubleshoot
  faq.md                NEW (todo) - "is this malware", "does it read my URLs"
  overview.md           NEW (todo) - 1-page human architecture, one diagram,
                        no AppBar history, no §-numbers; ends with a pointer
                        to ARCHITECTURE.md for the deep reference
  media/                NEW (todo) - hero screenshot + hover-fan GIF
  HANDOFF.md            untouched (path frozen by CLAUDE.md)
  ARCHITECTURE.md       untouched (path frozen); NOT split - plans/ cross-
                        reference it by §-number
  plans/                untouched (paths frozen by CLAUDE.md)
  research/             untouched
  dashboard/            untouched
  profiler-project-structure.md   untouched
```

Nothing gets deleted: every existing doc has a real reader.

**Option B** (deferred): physically move HANDOFF/ARCHITECTURE/plans/research/
dashboard under `docs/dev/`. Tidier long-term but requires updating
CLAUDE.md's session-start paths + all cross-links in one atomic commit. Not
worth the risk unless top-level `docs/` starts feeling cluttered.

## Net-new content still needed

1. **Hero media** (`docs/media/`): highest-value gap. Raw Windows footage now
   exists; what's left is edit/clean/re-record per `media-strategy.md`.
   Deliverables: `docs/media/hero.gif` (minimize → chips → hover-fan →
   click-restore loop), `docs/media/hero.png` (staggered multi-window still),
   `docs/media/how-it-works.png` (annotated gap-measurement). Then replace the
   `<!-- TODO … -->` placeholder in `README.md`.
2. **LinkedIn reel** (out-of-repo): 45 to 60s captioned MP4 from the same clips;
   storyboard + ffmpeg concat commands in `media-strategy.md`.
3. `CONTRIBUTING.md`: prereqs, Win32-only house rules distilled from
   CLAUDE.md, explicit note that CLAUDE.md/HANDOFF.md are the AI workflow.
4. `docs/install.md`: download-or-build, run, logon autostart (ARCHITECTURE
   §13), uninstall, troubleshooting (overlay not appearing, auto-hide
   taskbar, DPI).
5. `docs/faq.md`: URL reading, taskbar removal, browser support, phone-home.
6. `docs/overview.md` + `docs/README.md` index: `overview.md` can reuse the
   annotated `how-it-works.png`.

## Priority order if picking up again

1. ~~README rewrite + LICENSE + SECURITY.md~~ (done)
2. ~~Capture + analyze Windows footage~~ (done): see `media-strategy.md`
3. **Hero media**: clean re-record (or crop-from-existing) → `docs/media/`
   hero GIF + stills → drop the README placeholder. Biggest remaining lever;
   everything in the README already assumes it exists.
4. LinkedIn reel from the same source cuts (parallel, out-of-repo).
5. `docs/install.md` + `docs/overview.md` + `docs/README.md` index
6. `CONTRIBUTING.md` + `docs/faq.md` (do once the rest lands)

Full original Opus writeup (analysis + rationale in prose) is in the
conversation this plan was extracted from, if more detail is ever needed.
This file is the durable summary.
