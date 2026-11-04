# Map-specific powerup overlays

The server loads `powerups/<mapname>.txt` from its normal virtual filesystem at
map initialization, after the BSP entity list and before team/item registration.
A BSP/worldspawn replacement is neither necessary nor allowed.

The loader makes three bounded passes: validate all blocks and required models;
remove requested classes from original map entities; spawn additions. Removal
never touches clients, worldspawn or newly added entities. Syntax errors, unknown
classes, invalid numbers/bounds, missing models and capacity limits reject the
whole file before removal. Limits: 128 KiB, 256 blocks, the existing per-entity
spawn-variable/token limits, and 32 reserved entity slots. Allocation failure or
an error in an unrelated existing spawn function is not a general rollback
transaction; only validation is guaranteed to precede map mutations.

## Configuration

| Setting / command | Default | Meaning |
|---|---|---|
| `g_mapPowerups` | `1` | Enable overlays on map load. |
| `g_mapPowerupsVariant` | empty | Optional filename suffix, e.g. `christmastree`. Missing variants fall back to the normal file. |
| `g_mapPowerupsGametype` | `-1` | Keep SG prefix selection; `0`, `3`, `4`, `5` explicitly select DM, team DM, RTP or BR. |
| `powerups_status` | command | Show selected file, counts, fire and safe state. |

Changes take effect when the map is fully loaded again. Restarting a round does
not append duplicate entities. To test BR maps as DM, set
`g_mapPowerupsGametype 0` before loading them. Restore `-1` for the usual prefix
behavior. This override is explicit and independent of the overlay enable switch.

## Supported definitions

- Point/item entities: `info_player_deathmatch`, `info_player_start`,
  `info_player_intermission`, the four `team_CTF_*player/*spawn` classes,
  `item_botroam`, `target_speaker`, and known item/weapon/ammo classnames.
  Existing gametype filters still apply to `pickup_money` and `item_money`.
- Aliases: `bot_roam` -> `item_botroam`, `info_weapon_sawedoff` ->
  `weapon_sawedoff`, `info_item_belt` -> `item_belt`.
- `remove_target_entities`: `target` names a removable class, not a targetname.
  `remove_<classname>` is an equivalent short form. Only the spawn/item classes
  are removable; worldspawn and arbitrary movers/triggers are not.
- `message`: its `message` value becomes the map's `CS_MESSAGE` string.
- `misc_model` / `misc_externalmodel`: external MD3 decoration; optional local
  `mins` / `maxs` supply collision bounds, rotated into a world AABB. Existing
  animationStart/animationEnd/animationFPS properties are supported.
- `func_breakable`: external MD3 with explicit local bounds, health (default
  100), optional weapon restriction, existing breakable effects/round resets.
  Historical material flag `4` is metal; other definitions use wood particles.
  This interpretation is local to overlays; ordinary BSP breakables are unchanged.
- `trigger_escape`: explicit box from origin/mins/maxs, using the existing BR
  escape touch logic. No inline BSP brush is needed; `model ""` is accepted.
- `func_flare`: external MD3 with the existing flare protocol and size/color
  parameters, including the Christmas star.
- `invisible_hurt`: invisible trigger, default bounds -8..8 and default damage
  10 per second. Explicit bounds and `dmg` override these values.
- `eternal_fire`: persistent trigger, default bounds -24..24, damage 10 per second;
  uses existing client fire events and ignites nearby explosives/flammables.
  Events are refreshed for late joiners and after round-start fire clearing.

For `invisible_hurt`, fire defaults, safe linking and old material flags the
original implementation was unavailable. These are documented compatibility
choices, not claims of identical historical behavior. Numeric fields accept
finite decimal notation, not NaN/infinity/exponents or trailing characters.

## Safes

External `models/misc/safe.md3` breakables are associated with overlaid gold/money
and `safe_broken.md3` at the same X/Y (within one unit) and within 64 Z units.
The money and broken model remain hidden while the safe is closed. Breaking it
reveals them. Restoring the breakable hides them again. Existing weapon
restrictions enforce the dynamite-only safe when specified in the file.

Association uses coordinates because the provided files have no target names
for these groups. Arbitrary model filenames or nearby money at other coordinates
are not guessed to be safes. Purely server-side box collision is axis-aligned;
it is not a triangle mesh approximation of an MD3.

## Models and existing clients

Extensionless MD3 model paths receive `.md3`. Required models and sounds must
also exist in client assets. No new entity type/protocol or cgame QVM is needed.
Existing clients do not interpret `modelscale`, so the preparation tool bakes the
scale into a separate MD3 inside a new PK3:

```sh
python3 tools/prepare_powerup_models.py /path/to/smokinguns map-powerups-models-v1.pk3
```

Place the resulting PK3 in the game's search path before loading the map, and
make it available through the server's download location or install it on the
clients. The tool preserves original model/shader files, updates vertices,
frame bounds/radii and tag origins, and rejects MD3 coordinate overflow.
`modelscale 3` becomes `<model>_pu3000.md3`; missing generated models reject the
whole overlay. Scale range: 0.125..8. Model paths must start with `models/`.

## Validation and known input issues

```sh
sh tests/run_powerups.sh
sh tests/run_close_range.sh
sh tests/run_bot_hearing.sh
python3 -m unittest discover -s tests -p test_powerup_models.py -v
```

The 52 powerups checks compile production parser/state functions with GCC
undefined-behavior/division sanitizers and mocked engine services. The 29
close-range checks and six MD3 transformation tests are separate. Another 23 checks cover heard-goal
selection and bounded node-switch recording/dumping. Optional
paths passed to run_powerups.sh additionally validate real overlay files.

Of 53 active input `.txt` files inspected on the test server, 50 pass structural
and semantic validation (models checked separately during actual map startup).
Three are rejected without editing them:

- `br_elpaso.txt`: `count "8~~"` at line 69.
- `dm_fort.txt`: an extra opening brace near line 7.
- `dm_showdown.txt`: `/ entity 39` near line 285 is not a `//` comment; later
  definitions also include classes outside the supported overlay subset.

Visual rendering, sound playback, pickup accessibility and player interaction
must still be checked with a client. Successful startup/RCON and automated
state checks do not prove those visual or gameplay details.

## Deployment and rollback

Back up both `smokinguns/vm/qagame.qvm` and, when `vm_game 0` is used, the actually
loaded `smokinguns/qagamex86_64.so`. Merely replacing the QVM does not replace an
already running native module. Verify the loaded path, hashes, map startup log,
`powerups_status` and server responsiveness after a reload.

Use a separate homepath/port for the first test and verify restoring the old
module before installing on the test server. Disabling `g_mapPowerups` restores
normal BSP entities on the next map load. Binary rollback means restoring the
saved module(s), removing the added model PK3 if no longer needed, and fully
reloading the game/server. Keep the original powerups files unchanged.

## Hearing state loop found during sustained testing

The existing hearing fallback in BotFindEnemy used to return success for a sound
and enter chase. Its callers immediately promoted the same target to fight,
which returned to seek because the target was still hidden. Repeating transitions
also exceeded the node-history buffer: a node may record multiple transitions per
iteration. This was reproduced with four bots before release.

Sound investigation now runs as a separate reachable navigation goal in seek
states and yields the frame after selecting chase. BotFindEnemy retains its
visual-search contract. Both history writes and dumps have their own hard bound;
Snapshot entity numbers are checked before direct array access.
AI-budget exhaustion clears the stale lead and recovers rather than overrunning
the buffer. Diagnostic dumps are throttled. Existing sound-directed direct-fire
suppression remains available in chase.
