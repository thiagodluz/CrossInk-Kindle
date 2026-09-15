#!/bin/sh
# CrossInk for Kindle — read-only PW1 environment diagnostic.
# Run from an established Kindle shell. It never writes outside its output
# directory and never opens framebuffer or input-event devices for capture.

set -u
umask 077
SCRIPT_VERSION="1"
OUTPUT_PARENT="${1:-$PWD}"
STAMP="$(date +%Y%m%d-%H%M%S 2>/dev/null || echo unknown-date)"
OUT_DIR="$OUTPUT_PARENT/crossink-pw1-diagnostics-$STAMP"
REPORT="$OUT_DIR/report.txt"
RAW_DIR="$OUT_DIR/raw"

if ! mkdir -p "$RAW_DIR"; then
  echo "Cannot create diagnostic directory: $OUT_DIR" >&2
  exit 1
fi

run() {
  label="$1"
  shift
  {
    echo
    echo "===== $label ====="
    "$@"
    status=$?
    echo "[exit=$status]"
  } >>"$REPORT" 2>&1
}

run_sh() {
  label="$1"
  command="$2"
  {
    echo
    echo "===== $label ====="
    sh -c "$command"
    status=$?
    echo "[exit=$status]"
  } >>"$REPORT" 2>&1
}

has() { command -v "$1" >/dev/null 2>&1; }

sanitize() {
  sed \
    -e 's/\([Ss]erial[[:space:]]*[:=][[:space:]]*\)[^[:space:]]*/\1<redacted>/g' \
    -e 's/\([Ss]erial[Nn]umber[[:space:]]*[:=][[:space:]]*\)[^[:space:]]*/\1<redacted>/g' \
    -e 's/\([Uu][Uu][Ii][Dd][[:space:]]*[:=][[:space:]]*\)[^[:space:]]*/\1<redacted>/g' \
    -e 's/\([0-9A-Fa-f][0-9A-Fa-f]:\)\{5\}[0-9A-Fa-f][0-9A-Fa-f]/<redacted-mac>/g'
}

copy_text_file() {
  source_path="$1"
  destination_name="$2"
  [ -r "$source_path" ] && sanitize <"$source_path" >"$RAW_DIR/$destination_name"
}

{
  echo "CrossInk for Kindle PW1 diagnostics"
  echo "script_version=$SCRIPT_VERSION"
  echo "started=$(date 2>/dev/null || true)"
  echo "output_directory=$OUT_DIR"
  echo "policy=read-only; no framebuffer or input-event capture; no Kindle settings changed"
} >"$REPORT"

run "identity: uname" uname -a
run_sh "identity: process and shell" "id; echo SHELL=\${SHELL:-unknown}; pwd"
run_sh "identity: system release files" "for f in /etc/version /etc/os-release /etc/prettyversion /etc/uks /etc/upstart; do [ -r \"\$f\" ] && { echo --- \$f; cat \"\$f\"; }; done"
run_sh "identity: CPU and memory summary" "grep -E '^(Processor|model name|Hardware|Revision|BogoMIPS|MemTotal|MemFree|SwapTotal|SwapFree)' /proc/cpuinfo /proc/meminfo 2>/dev/null"
run_sh "identity: executable format" "for f in /bin/sh /usr/bin/lipc-get-prop /usr/bin/fbset; do [ -x \"\$f\" ] && { echo --- \$f; file \"\$f\" 2>/dev/null || true; }; done"
run_sh "runtime: libc and C++ libraries" "ls -l /lib/libc.so* /usr/lib/libc.so* /lib/libstdc++* /usr/lib/libstdc++* 2>/dev/null || true; ldd --version 2>&1 || true"
run_sh "storage: mounts and free space" "mount; echo; df -h 2>/dev/null || df"
run_sh "storage: Kindle user-store landmarks" "for d in /mnt/us /mnt/us/documents /mnt/us/extensions /mnt/us/koreader /mnt/us/extensions/koreader; do if [ -d \"\$d\" ]; then echo present:\$d; else echo absent:\$d; fi; done"
run_sh "storage: writable probe in output directory" "test -w '$OUT_DIR' && echo writable || echo not-writable"

run_sh "display: framebuffer nodes" "ls -l /dev/fb* /sys/class/graphics/fb* 2>/dev/null || true"
run_sh "display: framebuffer metadata" "for d in /sys/class/graphics/fb*; do [ -d \"\$d\" ] || continue; echo --- \$d; for f in name virtual_size bits_per_pixel stride rotate modes; do [ -r \"\$d/\$f\" ] && { echo -n \"\$f=\"; cat \"\$d/\$f\"; }; done; done"
if has fbset; then
  run_sh "display: fbset information" "for fb in /dev/fb*; do [ -e \"\$fb\" ] && { echo --- \$fb; fbset -fb \"\$fb\" -i; }; done"
else
  echo "===== display: fbset information =====" >>"$REPORT"
  echo "fbset not available" >>"$REPORT"
fi

run_sh "input: device nodes" "ls -l /dev/input /dev/input/event* 2>/dev/null || true"
copy_text_file "/proc/bus/input/devices" "proc-bus-input-devices.txt"
if [ -r "$RAW_DIR/proc-bus-input-devices.txt" ]; then
  { echo; echo "===== input: /proc/bus/input/devices ====="; cat "$RAW_DIR/proc-bus-input-devices.txt"; } >>"$REPORT"
fi
if has getevent; then
  run "input: getevent capabilities" getevent -pl
else
  echo "===== input: getevent capabilities =====" >>"$REPORT"
  echo "getevent not available; no event capture attempted" >>"$REPORT"
fi

run_sh "power: sysfs candidates" "find /sys/class/power_supply /sys/devices/platform -maxdepth 4 -type f \\( -name capacity -o -name status -o -name online -o -name voltage_now -o -name current_now -o -name brightness -o -name max_brightness \\) -print 2>/dev/null | sort"
run_sh "power: readable values" "for f in \$(find /sys/class/power_supply /sys/devices/platform -maxdepth 4 -type f \\( -name capacity -o -name status -o -name online -o -name voltage_now -o -name current_now -o -name brightness -o -name max_brightness \\) -print 2>/dev/null); do echo --- \$f; cat \"\$f\" 2>/dev/null; done"
if has lipc-get-prop; then
  run_sh "power: Kindle LIPC read-only properties" "for p in battLevel battState isCharging; do echo --- com.lab126.powerd \$p; lipc-get-prop com.lab126.powerd \$p 2>&1 || true; done; for p in firmwareVersion deviceType; do echo --- com.lab126.deviceInfo \$p; lipc-get-prop com.lab126.deviceInfo \$p 2>&1 || true; done"
else
  echo "===== power: Kindle LIPC read-only properties =====" >>"$REPORT"
  echo "lipc-get-prop not available" >>"$REPORT"
fi

run_sh "integration: launch and logging tools" "for c in kual kterm lipc-get-prop logger sshd dropbear; do if command -v \"\$c\" >/dev/null 2>&1; then echo available:\$c=\$(command -v \"\$c\"); else echo absent:\$c; fi; done"
run_sh "integration: relevant service processes" "ps 2>/dev/null | grep -E '[k]ual|[k]oreader|[l]ipc|[p]owerd|[f]ramework' || true"

copy_text_file "/etc/version" "etc-version.txt"
copy_text_file "/etc/os-release" "etc-os-release.txt"
sanitize <"$REPORT" >"$REPORT.sanitized"
mv "$REPORT.sanitized" "$REPORT"

{
  echo
  echo "===== completion ====="
  echo "finished=$(date 2>/dev/null || true)"
  echo "share=$REPORT"
  echo "All collected text was sanitized for serial-like identifiers and MAC addresses."
} >>"$REPORT"

echo "Diagnostic report created: $REPORT"
