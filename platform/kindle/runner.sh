#!/bin/sh
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH
CI_ROOT=${CROSSINK_ROOT:-/mnt/us/crossink}
# Alternate proc tree is for isolated launcher checks; KUAL uses /proc.
CI_PROC=${CROSSINK_PROC_ROOT:-/proc}
CI_IDENTITY_PROC=${CROSSINK_IDENTITY_PROC_ROOT:-/proc}
CI_LOCK="$CI_ROOT/state/launch.lock"
CI_CHILD=
CI_WATCHDOG=
CI_SESSION=
CI_EXIT_SIGNAL=TERM

# boot_id and starttime are present on the PW1's 2.6.31 kernel. Never trust a
# persisted PID alone: both a reboot and PID reuse can give it another owner.
CI_BOOT=
read -r CI_BOOT < /proc/sys/kernel/random/boot_id || exit 1
process_start() {
  case "$1" in ''|*[!0-9]*) return 1 ;; esac
  ci_stat=
  read -r ci_stat 2>/dev/null < "$CI_IDENTITY_PROC/$1/stat" || return 1
  ci_stat=${ci_stat##*) }
  ci_index=0
  for ci_field in $ci_stat; do
    ci_index=$((ci_index + 1))
    if [ "$ci_index" -eq 20 ]; then echo "$ci_field"; return 0; fi
  done
  return 1
}
owner_alive() {
  [ -f "$CI_LOCK/identity" ] && [ -f "$CI_LOCK/owner" ] || return 1
  read -r ci_owner < "$CI_LOCK/owner"
  read -r ci_boot ci_started < "$CI_LOCK/identity"
  [ "$ci_boot" = "$CI_BOOT" ] && [ -n "$ci_started" ] || return 1
  ci_current=$(process_start "$ci_owner") || return 1
  [ "$ci_current" = "$ci_started" ] && kill -0 "$ci_owner" 2>/dev/null
}
legacy_owner_alive() {
  # Packages before 0.2.1 have no identity. Refuse to interrupt a live old
  # launcher, but a reused PID belonging to another command is not its owner.
  [ ! -f "$CI_LOCK/identity" ] && [ -f "$CI_LOCK/owner" ] || return 1
  read -r ci_owner < "$CI_LOCK/owner"
  case "$ci_owner" in ''|*[!0-9]*) return 1 ;; esac
  [ -r "$CI_IDENTITY_PROC/$ci_owner/cmdline" ] || return 1
  tr '\000' '\n' < "$CI_IDENTITY_PROC/$ci_owner/cmdline" | {
    while IFS= read -r ci_arg; do
      [ "$ci_arg" = "$CI_ROOT/runner.sh" ] && exit 0
    done
    exit 1
  }
}
lock_same_boot() {
  [ -f "$CI_LOCK/identity" ] || return 1
  read -r ci_boot ci_started < "$CI_LOCK/identity"
  [ "$ci_boot" = "$CI_BOOT" ]
}

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
    while read -r ci_pid ci_expected ci_started; do
      case "$ci_pid" in ''|*[!0-9]*) continue ;; esac
      case "$ci_expected" in cvm|awesome|volumd) ;; *) continue ;; esac
      if read_process "$ci_pid"; then
        if [ "$ci_name" = "$ci_expected" ] && [ -n "$ci_started" ] &&
           [ "$(process_start "$ci_pid")" = "$ci_started" ]; then
          kill -CONT "$ci_pid" 2>/dev/null
          echo "resumed pid=$ci_pid name=$ci_expected"
        fi
      fi
    done < "$CI_LOCK/paused"
  fi
}

terminate_child() {
  if [ -f "$CI_LOCK/child" ]; then
    read -r ci_pid ci_started < "$CI_LOCK/child"
    case "$ci_pid" in ''|*[!0-9]*) return ;; esac
    if [ -n "$ci_started" ] && [ "$(process_start "$ci_pid")" = "$ci_started" ] && read_process "$ci_pid"; then
      if [ "$ci_name" = crossink-pw1 ] || [ "$ci_name" = crossink-ui ]; then
        kill -"$CI_EXIT_SIGNAL" "$ci_pid" 2>/dev/null
        sleep 2
        if [ "$(process_start "$ci_pid")" = "$ci_started" ] && read_process "$ci_pid"; then
          case "$ci_name" in crossink-pw1|crossink-ui) kill -KILL "$ci_pid" 2>/dev/null ;; esac
        fi
      fi
    fi
  fi
}

recover() {
  # Old-boot PIDs must never receive signals, even when their names match.
  lock_same_boot || return 0
  terminate_child
  resume_ui
  # Request home repaint if the stock helper is available. No settings changed.
  if [ -x /usr/bin/lipc-set-prop ]; then
    /usr/bin/lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home 2>/dev/null
  fi
}

if [ "${1:-}" = --watchdog ]; then
  # This independent process survives a killed runner or a blocked display ioctl.
  ci_timeout=${3:-120}
  case "$ci_timeout" in 120|360) ;; *) exit 1 ;; esac
  sleep "$ci_timeout"
  if [ -f "$CI_LOCK/owner" ]; then
    read -r ci_owner < "$CI_LOCK/owner"
    read -r ci_boot ci_started < "$CI_LOCK/identity"
    if [ "$ci_owner:$ci_boot:$ci_started" = "${2:-}" ]; then
      echo 'ERROR watchdog expired; restoring Kindle UI'
      recover
    fi
  fi
  exit 0
fi

if [ "${1:-}" = --recover ]; then
  recover
  if ! owner_alive && ! legacy_owner_alive; then rm -rf "$CI_LOCK"; fi
  exit 0
fi

ci_binary=crossink-pw1
ci_argument=--diagnostic
ci_log=diagnostic
ci_timeout=120
if [ "${1:-}" = --ui ]; then
  ci_binary=crossink-ui
  ci_argument=--app
  ci_log=ui
  ci_timeout=360
elif [ -n "${1:-}" ]; then
  echo 'ERROR unknown launcher mode'
  exit 1
fi

mkdir -p "$CI_ROOT/state" "$CI_ROOT/logs" || exit 1
if ! mkdir "$CI_LOCK" 2>/dev/null; then
  echo 'ERROR CrossInk already running or stale lock; use Restore in KUAL'
  exit 1
fi
ci_started=$(process_start "$$") || exit 1
CI_SESSION="$$:$CI_BOOT:$ci_started"
echo "$CI_BOOT $ci_started" > "$CI_LOCK/identity"
echo "$$" > "$CI_LOCK/owner"
: > "$CI_LOCK/paused"
cleanup() {
  trap - 0
  trap '' INT TERM USR1 USR2
  [ -n "$CI_CHILD" ] && terminate_child
  resume_ui
  [ -n "$CI_WATCHDOG" ] && kill -TERM "$CI_WATCHDOG" 2>/dev/null
  if owner_alive; then rm -rf "$CI_LOCK"; fi
}
trap cleanup 0
trap 'exit 143' TERM
trap 'exit 130' INT
trap 'echo "Emergency exit requested"; exit 130' USR1
trap 'CI_EXIT_SIGNAL=USR2; echo "power_handoff=launcher grace_seconds=2"; exit 0' USR2
trap '' HUP

# Keep the current and previous log for each mode. Sessions are time-bounded.
[ -f "$CI_ROOT/logs/$ci_log.log" ] && mv -f "$CI_ROOT/logs/$ci_log.log" "$CI_ROOT/logs/$ci_log.previous.log"
exec >> "$CI_ROOT/logs/$ci_log.log" 2>&1
date
echo "launcher=CrossInk PW1 package 0.3.0 mode=$ci_log"

# Verify the ARM loader/runtime and device identities before pausing the UI.
"$CI_ROOT/bin/$ci_binary" --version || exit 1
"$CI_ROOT/bin/$ci_binary" --probe || exit 1

/bin/sh "$CI_ROOT/runner.sh" --watchdog "$CI_SESSION" "$ci_timeout" &
CI_WATCHDOG=$!

# Pause only running stock UI processes; record names before sending signals.
# A process which was already stopped is deliberately left as it was.
ci_paused=0
for ci_file in "$CI_PROC"/[0-9]*/status; do
  [ -r "$ci_file" ] || continue
  ci_pid=${ci_file%/status}; ci_pid=${ci_pid##*/}
  read_process "$ci_pid" || continue
  case "$ci_name" in
    cvm|awesome) ;;
    volumd) [ "$ci_log" = ui ] || continue ;;
    *) continue ;;
  esac
  case "$ci_state" in T|t|'') continue ;; esac
  ci_started=$(process_start "$ci_pid") || continue
  echo "$ci_pid $ci_name $ci_started" >> "$CI_LOCK/paused"
  if kill -STOP "$ci_pid" 2>/dev/null; then
    # In UI mode volumd is also paused, preventing simultaneous USB ownership.
    case "$ci_name" in cvm|awesome) ci_paused=$((ci_paused + 1)) ;; esac
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
CROSSINK_LAUNCHER_PID=$$
export CROSSINK_LAUNCHER_PID
"$CI_ROOT/bin/$ci_binary" "$ci_argument" &
CI_CHILD=$!
ci_started=$(process_start "$CI_CHILD")
echo "$CI_CHILD $ci_started" > "$CI_LOCK/child"
wait "$CI_CHILD"
ci_result=$?
CI_CHILD=
echo "exit_status=$ci_result"
exit "$ci_result"
