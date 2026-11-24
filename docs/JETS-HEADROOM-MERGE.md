# Merging `jets-headroom` (SPEC v1.40) with the other branches

Merge this branch to main FIRST; the others rebase onto it. Chip: the boot logo's ~46 KB are freed after the splash, so world 1 plays with 151 KB free on the a500 (was 105).

## w2-slice2 and w3-pilot: the `worldLoad(1)` splash free
- Both free the splash in `worldLoad(1)` (w2-slice2 commit 4ea4c31). With this branch it is a no-op (`splashDestroy` is safe to call twice): remove those lines on merge, and their note that the logo is held to the first world load.
- Their chip numbers and `tests/w2chip.agk` floors (measured at the world load) do not change: the memory was already free by then. `tests/splashfree.agk` guards world 1 (149 KB).
- Keep the floors 96 (`memory.agk`) and 64 (squeeze): never lowered.
- w2-slice2 already has the same `chopperDestroy` loop text (`ubLeft`): git takes it as one change. Its `jets` workarounds (lazy JETS_MAX, `logicUpdate` wrapping, mission-set check order) are no longer needed for headroom but are harmless; drop them if they conflict.

## Conflict zones
- `src/main.c`: `objectsQueue` (erasesMerge, people run, `pErase0`), `msgUpdate`/`msgUpdateBody` (now `ubSpent`, plan, `msgTypeFast`, `msgBodyFrom`), `msgCoverAll`, `msgCoverBob`, `textType`, the `s_ubAfterSplash` block in `genericProcess`.
- `src/logic.c`/`logic.h`: `logicDraw` (sets `g_ubDrawPeople0/1`; keep the order structures, shadow, tanks, people, sea, air: main.c relies on it, `testDrawPeopleRunAndGroundFirst`), `chopperDestroy`.
  - Anything drawn BEFORE the last person must stay on the ground (y >= 154); new air objects go after the people. w3-pilot touches `hostagesUpdate`: not changed here.
- `src/objblits.s`: only appended `erasesMergeAsm` (saves a2/a3). The native build uses `-DOBJ_BLITS_C` and the C `erasesMerge`.
- `docs/SPEC.md` version rows: this branch v1.40; the others v1.19-v1.24, v1.30, v1.31. Keep all rows; renumbering is not needed.
- `bomb-hint`'s prompt line draws into the message layer: with the typing plan a hint shown in a LOST/CLEAR state is not possible (hints hold off 60 pictures after a card), so no change needed; re-run `jetsload` and `hint`.
- `w2-sea-anim`: more BOBs in the draw list. If people stop being contiguous or an air BOB is added before them, the unit test fails: fix the order, not the test.

## After merging
Re-run all 4 profiles on the merge result; if `jets` still has to rise above 97, look at `jetsload` (94) first.
