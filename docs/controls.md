# Controls

The port copies the control scheme of the 2004 Game Boy Advance version of
Donkey Kong Country 2. It was read off that cartridge running headless in
mGBA (`tools/mgba/gbarun.c`), by pressing each button in Pirate Panic and
on the menus and comparing pictures. The game has no controls screen.

## In a level (GBA version, seen on screen)

| GBA button | Action |
|---|---|
| A | Jump; held longer, higher (peak 56 px for 5 frames held, 68 px held throughout) |
| B, held, with a direction | Run |
| B, tapped | Attack: Diddy's cartwheel, Dixie's ponytail spin |
| A during a cartwheel | Roll-jump |
| B, held, next to a barrel | Pick it up; releasing B throws it |
| R | Team-up (partner onto the lead's shoulders); then B throws the partner, A jumps, R again cancels |
| L or Select | Swap the lead Kong (the same action, pixel for pixel) |
| B, held in the air, Dixie leading | Helicopter spin, from about the top of the jump; A does not do it |
| Down | Crouch |
| Start | Pause and resume |

## Menus and the world map

A confirms everywhere and advances dialogue; Start finishes name entry and
skips the intro. On the world map Start opens the save/help menu, whose
pages L and R turn; Start or B closes it.

## The port

The GBA's A acts as the SNES's B (jump) and its B as the SNES's Y (run,
attack, carry), R as team-up, L and Select as the swap. The translation goes
in the input layer, as the SNES joypad bits the game reads, so the game's own
logic is untouched. Where the SNES game uses another button than the GBA
version (to be checked for the helicopter spin), the recompiled game keeps
the SNES's; whether to add the GBA version's is a separate decision.
