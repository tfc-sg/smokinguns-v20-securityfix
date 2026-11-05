# Bot combat tactics

Enabled by default with `bot_combat_tactics 1`. Set it to `0` to use the preceding combat selection and movement policy.

Ordinary bots prefer loaded firearms (including two pistols). Explosives are occasional tactical choices: a visible target 300–850 units away, no teammate near the impact area, and a slow target or a group. A decision window has a 20 percent opportunity by default and a minimum 12 second cooldown. Buying explosives is also limited, with a cash reserve. Bank robbery objective dynamite remains available independently. Existing sniper and Gatling roles retain their selection policy.

Bots can briefly stop during visible gunfights, beyond close range. The default pause lasts at most 250 ms, ends after a shot, and has a 1500 ms interval after the pause. The opening 45 percent of a pause allows settling before firing. Damage, reloading, airborne movement, ladders, escape and activation tasks prevent pauses. Weapon spread, skill values and human players are unchanged. This can reduce movement-related spread; it does not guarantee a higher measured hit rate.

| Server cvar | Default | Effective range |
| --- | --- | --- |
| bot_combat_tactics | 1 | 0 or 1 |
| bot_throw_interval | 12 | 6–60 seconds |
| bot_throw_chance | 0.20 | 0–0.50 |
| bot_aim_pause_ms | 250 | 100–500 ms |
| bot_aim_pause_interval_ms | 1500 | 800–4000 ms |

`bot_combat_status` prints current controls and per-bot counters for firearm shots, actual throws, pauses, and movement commands held still. Counters reset with bot state; they do not measure hits. A throw from a bank robbery activation task is also counted.

Run `sh tests/run_bot_combat.sh` for the isolated policy regression tests. Build both native and QVM modules from a clean object directory because bot_state_t changed internally. No client module is required.
