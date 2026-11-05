#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_binary=$(mktemp /tmp/sg-combat.XXXXXX)
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
${CC:-cc} -DSMOKINGUNS -DQAGAME -O2 -ffunction-sections -fdata-sections \
 -fsanitize=undefined,float-divide-by-zero -fno-sanitize-recover=all \
 -Wl,--gc-sections tests/test_bot_combat.c code/game/bg_misc.c code/qcommon/q_math.c code/qcommon/q_shared.c -lm -o "$test_binary"
"$test_binary"
