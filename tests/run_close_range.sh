#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_binary=$(mktemp /tmp/sg-close-range.XXXXXX)
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
${CC:-cc} -DSMOKINGUNS -O2 -ffunction-sections -fdata-sections \
  -fsanitize=undefined,float-divide-by-zero -fno-sanitize-recover=all \
  -Wl,--gc-sections tests/test_close_range.c code/game/g_hit.c \
  code/qcommon/q_math.c -lm -o "$test_binary"
"$test_binary"
