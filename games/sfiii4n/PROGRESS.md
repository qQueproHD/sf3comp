# 4rd Strike (sfiii4n)

4rd Arrange Edition 2013, a fan rebalance of 3rd Strike, as a game of its own in the collection (issues #9 and #23).

## What the hack is to 990608

The set is 990608's (`sfiii3n`, games/sfiii3n) with its own program (the first SIMM's four files) and two graphics
files of the fifth SIMM (5.6, 5.7). The BIOS and the other SIMMs are 990608's.

The program is 990608's patched in place, not built again: nothing moves. Of its 16 MB, everything before 0x06092000 is
the same; the changes are 980 stretches (162,864 halfwords), nearly all of them data after the code (from 0x0613C000,
most in 0x06620000-0x06780000: the characters' moves). In the code only two routines change: 0x06092536 (two
`add #-1,r0` made `nop`) and 0x0609F66C (a compare made `cmp/eq #3`, a constant 0 made 6, two branches retargeted).
There is no new code.

So the memory is laid out as 990608's and its facts (`src/facts.cpp`, 990608's as they are) hold. The block list is
990608's without the 22 blocks the hack changes, and what recorded play finds (translate.sh with frames).

It has what the other games share (games/shared/sf3) and the port's own features, and none of the definitive
version's own sources: those follow 990512's code, which the hack changes.

## Checks

- Translated code: 17,476 blocks (990608's list without the hack's 22, and what attract mode and five seeds of made-up
  play found). The full port's run of made-up play (`--auto 7`, 4000 frames) ran 99.52% of its block runs translated.
- `cps3recomp/tools/check.sh` (6000 frames, 1500 block by block): interpreter and translation the same, the
  translation twice the same, 3,558,294 dispatches compared and none different.
- Rollback (`--rollback-test 3`) and run-ahead 2: the same hashes as a plain run over 4000 frames.
- `check_online`: the two sides the same over 2999 frames, names and chat across, the match the same replayed. Online
  it is its own game (`sfiii4n`), so the waiting room keeps it apart from 3rd Strike's.
- The shared training check (`games/shared/sf3/checktraining.sh sfiii4n`; the cursors start on Ryu and Ken here, as
  the script expects): every judgement passes (parries 87 of 87 at 100%, 39 of 87 at 50%, none at 0%) but the
  feature list's, which has the throw escape the script expects only of 2nd Impact (as for 990608).
