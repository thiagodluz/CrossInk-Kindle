#!/bin/sh
# Explicit PATH is important when launched from a restricted terminal/KUAL.
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH
CI_ROOT=${CROSSINK_ROOT:-/mnt/us/crossink}
mkdir -p "$CI_ROOT/logs" || exit 1
if [ ! -f "$CI_ROOT/runner.sh" ] || [ ! -x "$CI_ROOT/bin/crossink-pw1" ]; then
  echo 'ERROR CrossInk package incomplete' >> "$CI_ROOT/logs/launcher.log"
  exit 1
fi
# KUAL is free to close before the validation program takes over the screen.
# HUP is ignored by the detached runner; the UI processes are paused, not stopped.
(
  trap '' HUP
  exec /bin/sh "$CI_ROOT/runner.sh"
) </dev/null >> "$CI_ROOT/logs/launcher.log" 2>&1 &
exit 0
