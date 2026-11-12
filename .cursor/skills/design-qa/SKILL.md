---
name: design-qa
description: >-
  Iterative visual QA loop: compare Levels/Claude Design mockup renders to the
  live Midi Toolkit Standalone, log gaps, fix, rebuild, recapture, until closed.
---

# Design QA Loop (Midi Toolkit × Levels mockup)

## Goal
Close the visual gap between the **design-pass mockup** (Claude Design / Levels)
and the **installed Standalone** by iterating: capture → diff → fix → rebuild →
recapture until remaining gaps are cosmetic-only or explicitly deferred.

## Paths
| Role | Path |
|------|------|
| Mockup refs | `build/qa/ref/` (photos + cropped mockup frames) |
| App captures | `build/qa/current/iterNNN.png` |
| Gap log | `build/qa/logs/GAPS.md` |
| Capture helper | `scripts/design_qa_capture.sh` |

## One iteration
1. **Capture** current Standalone (dark mode preferred for Levels parity):
   ```bash
   ./scripts/design_qa_capture.sh
   ```
2. **Read** latest `build/qa/current/iter*.png` **and** `build/qa/ref/mockup-target.jpg`
   (or the newest user-compare photo) with the image tool.
3. **Diff** by chrome region (not pixel MSE). Checklist:
   - Header: Play solid / Record outline / Through labeled / Drag Me inverted /
     Piano roll + Toolkit labeled chips
   - Sidebar: surface100, LIBRARY/FAVORITES/SAVED SEARCHES caps, selsoft+2px bar,
     violet count badges
   - Browser: title + count badge, search row, Filters labeled + badge when on
   - Roll: clip status strip, Grid, note fill, playhead (`ds::play` green)
   - Toolkit: caps mono sections + violet badges, violet pill switches,
     violet slider fills (`ds::acc`), surface000/100/200 stack
4. **Log** new/changed gaps in `build/qa/logs/GAPS.md` (P0/P1/P2 + file hints).
5. **Fix** only P0/P1 this iteration (smallest code change that moves pixels).
6. **Rebuild**: `./build.sh`, quit every Midi Toolkit process, relaunch Standalone,
   restore dark Appearance if editor reset to Auto.
7. **Recapture** → bump `iterNNN` → re-read images → update GAPS (mark fixed / still open).
8. **Stop** when no P0/P1 remain, or user says stop. Arm `/loop` only if waiting on
   rebuild/host; otherwise keep iterating in-session.

## Rules
- Mockup wins over taste. Levels `DESIGN.md` + user-compare photos are source of truth.
- Prefer token/layout fixes over one-off RGB hacks.
- Do not commit unless asked.
- After each rebuild, verify the **Standalone** binary timestamp, not only plugins.
