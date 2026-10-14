#!/usr/bin/env python3
"""Exercise launcher rollback with isolated fake proc entries and real child signals."""

import os
import signal
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def wait_for(predicate, label: str) -> None:
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError(label)


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="crossink-launcher-") as temporary:
        base = Path(temporary)
        native = base / "mock.cpp"
        native.write_text('''#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
int main(int argc,char** argv) {
 if(argc==3 && !strcmp(argv[1],"--heartbeat")) {
   FILE* file=fopen(argv[2],"wb"); if(!file) return 1;
   for(;;) { fputc('x',file); fflush(file); usleep(50000); }
 }
 if(argc!=2) return 2;
 if(!strcmp(argv[1],"--version")) return 0;
 if(!strcmp(argv[1],"--probe")) return getenv("MOCK_PROBE_FAIL") ? 19 : 0;
 if(!strcmp(argv[1],"--diagnostic")) {
   const char* mode=getenv("MOCK_MODE");
   if(mode && !strcmp(mode,"wait")) for(;;) pause();
   return mode && !strcmp(mode,"fail") ? 23 : 0;
 }
 return 2;
}
''')
        mock = base / "crossink-pw1"
        subprocess.run(["g++", str(native), "-o", str(mock)], check=True)
        for mode in ("normal", "fail", "probe-fail", "interrupt", "recover"):
            app = base / mode
            (app / "bin").mkdir(parents=True)
            (app / "bin/crossink-pw1").write_bytes(mock.read_bytes())
            (app / "bin/crossink-pw1").chmod(0o755)
            (app / "runner.sh").write_bytes((ROOT / "platform/kindle/runner.sh").read_bytes())
            proc = app / "proc"
            heartbeat = app / "ui-heartbeat"
            stopped_heartbeat = app / "stopped-heartbeat"
            ui = subprocess.Popen([str(mock), "--heartbeat", str(heartbeat)])
            previously_stopped = subprocess.Popen([str(mock), "--heartbeat", str(stopped_heartbeat)])
            runner = None
            try:
                wait_for(lambda: heartbeat.exists() and heartbeat.stat().st_size > 0, "UI fixture not started")
                wait_for(lambda: stopped_heartbeat.exists() and stopped_heartbeat.stat().st_size > 0, "stopped fixture not started")
                os.kill(previously_stopped.pid, signal.SIGSTOP)
                time.sleep(0.2)
                stopped_size = stopped_heartbeat.stat().st_size
                for pid, name, status in ((ui.pid, "cvm", "S"), (previously_stopped.pid, "awesome", "T")):
                    directory = proc / str(pid)
                    directory.mkdir(parents=True)
                    (directory / "status").write_text(f"Name:\t{name}\nState:\t{status} (fixture)\n")
                env = dict(os.environ, CROSSINK_ROOT=str(app), CROSSINK_PROC_ROOT=str(proc),
                           MOCK_MODE="wait" if mode in ("interrupt", "recover") else mode)
                if mode == "probe-fail":
                    env["MOCK_PROBE_FAIL"] = "1"
                runner = subprocess.Popen(["/bin/sh", str(app / "runner.sh")], env=env,
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if mode in ("interrupt", "recover"):
                    child_file = app / "state/launch.lock/child"
                    wait_for(child_file.exists, "diagnostic not started")
                    pid = int(child_file.read_text())
                    (proc / str(pid)).mkdir()
                    (proc / str(pid) / "status").write_text("Name:\tcrossink-pw1\nState:\tS (fixture)\n")
                    paused_size = heartbeat.stat().st_size
                    time.sleep(0.2)
                    assert heartbeat.stat().st_size == paused_size, "stock UI wasn't paused"
                    # A second launch must refuse to acquire the existing lock.
                    second = subprocess.run(["/bin/sh", str(app / "runner.sh")], env=env, capture_output=True, timeout=4)
                    assert second.returncode == 1, "duplicate launch accepted"
                    if mode == "interrupt":
                        runner.terminate()
                    else:
                        subprocess.run(["/bin/sh", str(app / "runner.sh"), "--recover"],
                                       env=env, check=True, capture_output=True, timeout=6)
                result = runner.wait(timeout=8)
                expected = {"normal": 0, "fail": 23, "probe-fail": 1, "interrupt": 143, "recover": 143}[mode]
                assert result == expected, (mode, result, expected)
                resumed_size = heartbeat.stat().st_size
                wait_for(lambda: heartbeat.stat().st_size > resumed_size, "UI not resumed")
                assert stopped_heartbeat.stat().st_size == stopped_size, "previously stopped UI was incorrectly resumed"
                assert not (app / "state/launch.lock").exists(), "lock leaked"
                log = (app / "logs/diagnostic.log").read_text()
                if mode == "probe-fail":
                    assert "paused pid=" not in log, "UI touched after failed probe"
                else:
                    assert "paused pid=" in log and "resumed pid=" in log, "handoff missing"
                print(f"launcher_check {mode}=ok")
            finally:
                if runner and runner.poll() is None:
                    runner.kill()
                    runner.wait()
                for process in (ui, previously_stopped):
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    main()
