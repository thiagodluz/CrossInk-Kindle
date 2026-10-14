#!/bin/sh
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH
CI_ROOT=${CROSSINK_ROOT:-/mnt/us/crossink}
# Alternate proc tree is for isolated launcher checks; KUAL uses /proc.
CI_PROC=${CROSSINK_PROC_ROOT:-/proc}
CI_LOCK="$CI_ROOT/state/launch.lock"
CI_CHILD=
CI_WATCHDOG=

read_process() {
  ci_name=
  ci_state=
  [ -r "$CI_PROC/$1/status" ] || return 1
  while read -r ci_key ci_value ci_rest; do
    case "$ci_key" in Name:) ci_name=$ci_value ;; State:) ci_state=$ci_value ;; esac
    if [ -n "$ci_name" ] && [ -n "$ci_state" ]; then break; fi
  done < "$CI_PROC/$1/status"
  [ -n "$ci_name" ]
}

resume_ui() {
  if [ -f "$CI_LOCK/paused" ]; then
    while read -r ci_pid ci_expected; do
      case "$ci_pid" in ''|*[!0-9]*) continue ;; esac
      case "$ci_expected" in cvm|awesome) ;; *) continue ;; esac
      if read_process "$ci_pid"; then
        if [ "$ci_name" = "$ci_expected" ]; then
          kill -CONT "$ci_pid" 2>/dev/null
          echo "resumed pid=$ci_pid name=$ci_expected"
        fi
      fi
    done < "$CI_LOCK/paused"
  fi
}

terminate_child() {
  if [ -f "$CI_LOCK/child" ]; then
    read -r ci_pid < "$CI_LOCK/child"
    case "$ci_pid" in ''|*[!0-9]*) return ;; esac
    if read_process "$ci_pid"; then
      if [ "$ci_name" = crossink-pw1 ]; then
        kill -TERM "$ci_pid" 2>/dev/null
        sleep 2
        if read_process "$ci_pid"; then
          [ "$ci_name" = crossink-pw1 ] && kill -KILL "$ci_pid" 2>/dev/null
        fi
      fi
    fi
  fi
}

recover() {
  terminate_child
  resume_ui
  # Request home repaint if the stock helper is available. No settings changed.
  if [ -x /usr/bin/lipc-set-prop ]; then
    /usr/bin/lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home 2>/dev/null
  fi
}

if [ "${1:-}" = --watchdog ]; then
  # This independent process survives a killed runner or a blocked display ioctl.
  sleep 120
  if [ -f "$CI_LOCK/owner" ]; then
    read -r ci_owner < "$CI_LOCK/owner"
    if [ "$ci_owner" = "${2:-}" ]; then
      echo 'ERROR watchdog expired; restoring Kindle UI'
      recover
    fi
  fi
  exit 0
fi
if [ "${1:-}" = --recover ]; then
  recover
  if [ -f "$CI_LOCK/owner" ]; then
    read -r ci_owner < "$CI_LOCK/owner"
    case "$ci_owner" in ''|*[!0-9]*) ;; *)
      # Live runner cleans up its own lock. An orphan lock can be removed.
      if ! kill -0 "$ci_owner" 2>/dev/null; then rm -rf "$CI_LOCK"; fi
    ;; esac
  fi
  exit 0
fi

mkdir -p "$CI_ROOT/state" "$CI_ROOT/logs" || exit 1
if ! mkdir "$CI_LOCK" 2>/dev/null; then
  echo 'ERROR CrossInk already running or stale lock; use Restore in KUAL'
  exit 1
fi
echo "$$" > "$CI_LOCK/owner"
: > "$CI_LOCK/paused"
cleanup() {
  trap - 0 INT TERM
  [ -n "$CI_CHILD" ] && terminate_child
  resume_ui
  [ -n "$CI_WATCHDOG" ] && kill -TERM "$CI_WATCHDOG" 2>/dev/null
  rm -rf "$CI_LOCK"
}
trap cleanup 0
trap 'exit 143' TERM
trap 'exit 130' INT
trap '' HUP

# Rotate two bounded logs. The diagnostic caps captured touch data to 500 events.
[ -f "$CI_ROOT/logs/diagnostic.log" ] && mv -f "$CI_ROOT/logs/diagnostic.log" "$CI_ROOT/logs/diagnostic.previous.log"
exec >> "$CI_ROOT/logs/diagnostic.log" 2>&1
date
echo 'launcher=CrossInk PW1 hardware validation 0.1.0'

# Verify the ARM loader/runtime and device identities before pausing the UI.
"$CI_ROOT/bin/crossink-pw1" --version || exit 1
"$CI_ROOT/bin/crossink-pw1" --probe || exit 1

/bin/sh "$CI_ROOT/runner.sh" --watchdog "$$" &
CI_WATCHDOG=$!

# Pause only running stock UI processes; record names before sending signals.
# A process which was already stopped is deliberately left as it was.
ci_paused=0
for ci_file in "$CI_PROC"/[0-9]*/status; do
  [ -r "$ci_file" ] || continue
  ci_pid=${ci_file%/status}; ci_pid=${ci_pid##*/}
  read_process "$ci_pid" || continue
  case "$ci_name" in cvm|awesome) ;; *) continue ;; esac
  case "$ci_state" in T|t|'') continue ;; esac
  echo "$ci_pid $ci_name" >> "$CI_LOCK/paused"
  if kill -STOP "$ci_pid" 2>/dev/null; then
    ci_paused=$((ci_paused + 1))
    echo "paused pid=$ci_pid name=$ci_name"
  else
    echo "ERROR unable to pause pid=$ci_pid"
    exit 1
  fi
done
if [ "$ci_paused" -eq 0 ]; then
  echo 'ERROR no stock UI process found; ending before display writes'
  exit 1
fi
sleep 1
"$CI_ROOT/bin/crossink-pw1" --diagnostic &
CI_CHILD=$!
echo "$CI_CHILD" > "$CI_LOCK/child"
wait "$CI_CHILD"
ci_result=$?
CI_CHILD=
echo "exit_status=$ci_result"
exit "$ci_result"
