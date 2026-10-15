#!/usr/bin/env python3
"""Exercise launcher rollback with isolated fake proc entries and real child signals."""

import os
import itertools
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
        # Isolate identity as well as status. Some execution sandboxes expose a
        # host /proc tree while subprocess PIDs belong to a nested namespace.
        identities = base / "identities"
        for pid in range(os.getpid(), os.getpid()+8192):
            item = identities / str(pid)
            item.mkdir(parents=True)
            (item / "stat").write_text(f"{pid} (fixture) S "+"0 "*18+f"{pid*10} 0\n")
            (item / "cmdline").write_bytes(b"unrelated-process\0")
        native = base / "mock.cpp"
        native.write_text('''#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <signal.h>
int main(int argc,char** argv) {
 if(argc==3 && !strcmp(argv[1],"--heartbeat")) {
   FILE* file=fopen(argv[2],"wb"); if(!file) return 1;
   for(;;) { fputc('x',file); fflush(file); usleep(50000); }
 }
 if(argc==3 && !strcmp(argv[1],"--power-state")) {
   printf("mock_power_state stage=%s\\n",argv[2]); return 0;
 }
 if(argc==3 && !strcmp(argv[1],"--status-bar")) {
   printf("mock_status_bar=%s\\n",argv[2]);
   if(!strcmp(argv[2],"hide") && getenv("MOCK_BAR_FAIL_HIDE")) return 17;
   if(!strcmp(argv[2],"show") && getenv("MOCK_BAR_FAIL_SHOW")) return 17;
   return 0;
 }
 if(argc!=2) return 2;
 if(!strcmp(argv[1],"--version")) return 0;
 if(!strcmp(argv[1],"--probe")) return getenv("MOCK_PROBE_FAIL") ? 19 : 0;
 if(!strcmp(argv[1],"--diagnostic") || !strcmp(argv[1],"--app")) {
   puts("mock_app=started");
   const char* mode=getenv("MOCK_MODE");
   if(mode && (!strcmp(mode,"stuck") || !strcmp(mode,"power-stuck"))) { signal(SIGTERM,SIG_IGN); signal(SIGUSR2,SIG_IGN); for(;;) pause(); }
   if(mode && !strcmp(mode,"wait")) for(;;) pause();
   return mode && !strcmp(mode,"fail") ? 23 : 0;
 }
 return 2;
}
''')
        mock = base / "crossink-pw1"
        subprocess.run(["g++", str(native), "-o", str(mock)], check=True)
        cases = list(itertools.product(("diagnostic", "ui"), ("normal", "fail", "probe-fail", "interrupt", "recover", "emergency", "stale-pid", "stale-boot", "legacy-pid", "power", "power-stuck")))
        cases += [("ui",mode) for mode in ("bar-hide-fail","bar-show-fail","framework-stopped")]
        for profile, mode in cases:
            app = base / (profile+"-"+mode)
            binary_name = "crossink-ui" if profile=="ui" else "crossink-pw1"
            arguments = ["--ui"] if profile=="ui" else []
            (app / "bin").mkdir(parents=True)
            (app / "bin" / binary_name).write_bytes(mock.read_bytes())
            (app / "bin" / binary_name).chmod(0o755)
            (app / "runner.sh").write_bytes((ROOT / "platform/kindle/runner.sh").read_bytes())
            proc = app / "proc"
            heartbeat = app / "ui-heartbeat"
            wm_heartbeat = app / "wm-heartbeat"
            stopped_heartbeat = app / "stopped-heartbeat"
            volume_heartbeat = app / "volume-heartbeat"
            ui = subprocess.Popen([str(mock), "--heartbeat", str(heartbeat)])
            wm = subprocess.Popen([str(mock), "--heartbeat", str(wm_heartbeat)])
            previously_stopped = subprocess.Popen([str(mock), "--heartbeat", str(stopped_heartbeat)])
            volume = subprocess.Popen([str(mock), "--heartbeat", str(volume_heartbeat)])
            runner = None
            try:
                wait_for(lambda: heartbeat.exists() and heartbeat.stat().st_size > 0, "UI fixture not started")
                wait_for(lambda: stopped_heartbeat.exists() and stopped_heartbeat.stat().st_size > 0, "stopped fixture not started")
                wait_for(lambda: volume_heartbeat.exists() and volume_heartbeat.stat().st_size > 0, "volume fixture not started")
                wait_for(lambda: wm_heartbeat.exists() and wm_heartbeat.stat().st_size > 0, "WM fixture not started")
                if mode=="framework-stopped": os.kill(wm.pid,signal.SIGSTOP)
                os.kill(previously_stopped.pid, signal.SIGSTOP)
                time.sleep(0.2)
                stopped_size = stopped_heartbeat.stat().st_size
                for pid, name, status in ((ui.pid, "cvm", "S"), (wm.pid,"awesome","T" if mode=="framework-stopped" else "S"), (previously_stopped.pid, "awesome", "T"), (volume.pid, "volumd", "S")):
                    directory = proc / str(pid)
                    directory.mkdir(parents=True)
                    (directory / "status").write_text(f"Name:\t{name}\nState:\t{status} (fixture)\n")
                env = dict(os.environ, CROSSINK_ROOT=str(app), CROSSINK_PROC_ROOT=str(proc),
                           CROSSINK_IDENTITY_PROC_ROOT=str(identities),
                           MOCK_MODE="stuck" if mode == "emergency" else "wait" if mode in ("interrupt", "recover", "power") else mode)
                if mode=="bar-hide-fail": env["MOCK_BAR_FAIL_HIDE"]="1"
                if mode=="bar-show-fail": env["MOCK_BAR_FAIL_SHOW"]="1"
                if mode == "probe-fail":
                    env["MOCK_PROBE_FAIL"] = "1"
                if mode in ("stale-pid", "stale-boot", "legacy-pid", "power", "power-stuck"):
                    lock = app / "state/launch.lock"
                    lock.mkdir(parents=True)
                    (lock / "owner").write_text(str(ui.pid)+"\n")
                    if mode != "legacy-pid":
                        boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
                        started = (identities / str(ui.pid) / "stat").read_text().rsplit(") ",1)[1].split()[19]
                        (lock / "identity").write_text(("old-boot" if mode == "stale-boot" else boot)+" "+
                                                      (started if mode == "stale-boot" else "0")+"\n")
                    # Restore must discard the lock without signalling this unrelated live PID.
                    subprocess.run(["/bin/sh", str(app / "runner.sh"), "--recover"],
                                   env=env, check=True, capture_output=True, timeout=6)
                    assert not lock.exists(), "stale PID/boot lock survived recovery"
                    assert ui.poll() is None, "recovery killed unrelated owner PID"
                runner = subprocess.Popen(["/bin/sh", str(app / "runner.sh"), *arguments], env=env,
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if mode in ("interrupt", "recover", "emergency", "power", "power-stuck"):
                    child_file = app / "state/launch.lock/child"
                    wait_for(child_file.exists, "diagnostic not started")
                    pid = int(child_file.read_text().split()[0])
                    (proc / str(pid)).mkdir()
                    (proc / str(pid) / "status").write_text(f"Name:\t{binary_name}\nState:\tS (fixture)\n")
                    paused_size = heartbeat.stat().st_size
                    wm_size = wm_heartbeat.stat().st_size
                    time.sleep(0.2)
                    if profile=="ui":
                        assert heartbeat.stat().st_size>paused_size, "cooperative CVM was frozen"
                        assert wm_heartbeat.stat().st_size>wm_size, "cooperative WM was frozen"
                    else:
                        assert heartbeat.stat().st_size==paused_size, "diagnostic CVM wasn't paused"
                        assert wm_heartbeat.stat().st_size==wm_size, "diagnostic WM wasn't paused"
                    # A second launch must refuse to acquire the existing lock.
                    second = subprocess.run(["/bin/sh", str(app / "runner.sh"), *arguments], env=env, capture_output=True, timeout=4)
                    assert second.returncode == 1, "duplicate launch accepted"
                    if mode == "interrupt":
                        runner.terminate()
                    elif mode == "emergency":
                        runner.send_signal(signal.SIGUSR1)
                    elif mode in ("power", "power-stuck"):
                        runner.send_signal(signal.SIGUSR2)
                    else:
                        subprocess.run(["/bin/sh", str(app / "runner.sh"), "--recover"],
                                       env=env, check=True, capture_output=True, timeout=6)
                result = runner.wait(timeout=8)
                expected = {"normal": 0, "fail": 23, "probe-fail": 1, "interrupt": 143, "recover": 143, "emergency": 130, "stale-pid": 0, "stale-boot": 0, "legacy-pid": 0, "power": 0, "power-stuck": 0,"bar-hide-fail":1,"bar-show-fail":0,"framework-stopped":1}[mode]
                assert result == expected, (mode, result, expected, (app / "logs" / (profile+".log")).read_text() if (app / "logs" / (profile+".log")).exists() else "no log")
                resumed_size = heartbeat.stat().st_size
                wait_for(lambda: heartbeat.stat().st_size > resumed_size, "UI not resumed")
                resumed_volume = volume_heartbeat.stat().st_size
                wait_for(lambda: volume_heartbeat.stat().st_size > resumed_volume, "volume service not resumed")
                assert stopped_heartbeat.stat().st_size == stopped_size, "previously stopped UI was incorrectly resumed"
                if mode=="bar-show-fail":
                    assert (app / "state/launch.lock/status-bar").read_text().strip()=="hidden", "failed restore lost recovery record"
                    retry_env=dict(env);retry_env.pop("MOCK_BAR_FAIL_SHOW")
                    subprocess.run(["/bin/sh",str(app/"runner.sh"),"--recover"],env=retry_env,check=True,capture_output=True,timeout=6)
                assert not (app / "state/launch.lock").exists(), "lock leaked"
                log = (app / "logs" / (profile+".log")).read_text()
                if mode == "probe-fail":
                    assert "paused pid=" not in log, "UI touched after failed probe"
                else:
                    assert "paused pid=" in log and "resumed pid=" in log, "handoff missing"
                    assert (f"paused pid={volume.pid} name=volumd" in log) == (profile=="ui"), "USB ownership mismatch"
                if profile=="ui" and mode!="probe-fail":
                    assert f"paused pid={ui.pid} name=cvm" not in log
                    assert f"paused pid={wm.pid} name=awesome" not in log
                    assert f"resumed pid={ui.pid} name=cvm" not in log
                    assert f"resumed pid={wm.pid} name=awesome" not in log
                    assert "mock_power_state stage=before-prepare" in log
                    if mode not in ("bar-hide-fail","framework-stopped"):
                        assert "mock_power_state stage=after-prepare" in log
                        assert "framework_policy=cooperative" in log
                    if mode!="framework-stopped":
                        assert "mock_status_bar=hide" in log and "mock_status_bar=show" in log
                    if mode in ("bar-hide-fail","framework-stopped"):
                        assert "mock_app=started" not in log, "reader launched after failed preparation"
                    if mode in ("normal","fail","stale-pid","stale-boot","legacy-pid"):
                        assert "mock_power_state stage=before-release" in log
                if mode in ("power", "power-stuck"):
                    assert "power_handoff=launcher grace_seconds=2" in log, "power exit not acknowledged"
                print(f"launcher_check {profile}/{mode}=ok",flush=True)
            finally:
                if runner and runner.poll() is None:
                    runner.kill()
                    runner.wait()
                for process in (ui, wm, previously_stopped, volume):
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    main()
