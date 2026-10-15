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
        boot_path = identities / "sys/kernel/random"
        boot_path.mkdir(parents=True)
        (boot_path/"boot_id").write_bytes(Path("/proc/sys/kernel/random/boot_id").read_bytes())
        native = base / "mock.cpp"
        native.write_text('''#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include "LauncherSession.h"
#include "SessionSupervisor.h"
#include "RenderWorker.h"
void logPrintf(const char*,const char*,const char*,...) {}
void render(const kindle::UiJob&,void*) {
 if(getenv("MOCK_RENDER_STALL")) for(;;) pause();
}
int main(int argc,char** argv) {
 setvbuf(stdout,nullptr,_IOLBF,0);
 if(argc==3 && !strcmp(argv[1],"--supervise-ui")) {
   kindle::SupervisorLimits limits;
   if(getenv("MOCK_SHORT_WATCHDOG")) limits={8000,1600,1800,4000};
   return kindle::superviseSession(argv[2],limits);
 }
 if(argc==2 && !strcmp(argv[1],"--session-unlock")) return kindle::unlockSessionGuard() ? 0 : 1;
 if(argc==2 && !strcmp(argv[1],"--session-check")) return kindle::checkSessionGuard() ? 0 : 1;
 if(argc>=3 && !strcmp(argv[1],"--session-guard")) return kindle::guardSession(argv[2],argc-3,argv+3);
 if(argc==2 && !strcmp(argv[1],"--recover-display")) {
   puts("mock_display_recovery=home"); return getenv("MOCK_REPAINT_FAIL") ? 17 : 0;
 }
 if(argc==3 && !strcmp(argv[1],"--heartbeat")) {
   FILE* file=fopen(argv[2],"wb"); if(!file) return 1;
   for(;;) { fputc('x',file); fflush(file); usleep(50000); }
 }
 if(argc==3 && !strcmp(argv[1],"--power-state")) {
   printf("mock_power_state stage=%s\\n",argv[2]); return 0;
 }
 if(argc==3 && !strcmp(argv[1],"--status-bar")) {
   printf("mock_status_bar=%s\\n",argv[2]);
   if(!strcmp(argv[2],"show") && getenv("MOCK_SHOW_BARRIER")) {
     FILE* f=fopen(getenv("MOCK_SHOW_READY"),"w"); if(f){fputs("ready",f);fclose(f);}
     while(access(getenv("MOCK_SHOW_BARRIER"),F_OK)) usleep(10000);
   }
   if(getenv("MOCK_DESCENDANT")) {
     const pid_t daemon=fork();
     if(daemon==0) {
       close(0);close(1);close(2);
       FILE* f=fopen(getenv("MOCK_DESCENDANT_READY"),"a"); if(f){fputs("ready\\n",f);fclose(f);}
       for(unsigned i=0;i<2000 && access(getenv("MOCK_DESCENDANT_STOP"),F_OK);++i) usleep(10000);
       _exit(0);
     }
   }
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
   if(mode && (!strcmp(mode,"healthy") || !strcmp(mode,"input-stall") || !strcmp(mode,"render-stall") || !strcmp(mode,"whole-stall") || !strcmp(mode,"startup-stall") || !strcmp(mode,"supervisor-death") || !strcmp(mode,"runner-death"))) {
     signal(SIGTERM,SIG_IGN);signal(SIGUSR2,SIG_IGN);signal(SIGPIPE,SIG_IGN);
     const char* identities=getenv("CROSSINK_IDENTITY_PROC_ROOT");
     if(!kindle::captureLauncher(identities) || !kindle::registerReader(identities)) return 19;
     while(access(getenv("MOCK_REQUEST_READY"),F_OK)) usleep(10000);
     if(!strcmp(mode,"startup-stall")) for(;;) pause();
     kindle::SessionHeartbeat input, drawing;
     if(!input.begin('I') || !drawing.begin('R')) return 19;
     if(!strcmp(mode,"render-stall")) setenv("MOCK_RENDER_STALL","1",1);
     kindle::RenderWorker worker(render,nullptr,&drawing);
     if(!worker.start()) return 19;
     if(!strcmp(mode,"whole-stall")) raise(SIGSTOP);
     if(!strcmp(mode,"input-stall")) for(;;) pause();
     const auto start=kindle::monotonicMilliseconds();
     while(strcmp(mode,"healthy") || kindle::monotonicMilliseconds()-start<6000) {
       if(!input.pulse()) return 19;
       usleep(20000);
     }
     worker.stop();return 0;
   }
   if(mode && (!strcmp(mode,"child-power") || !strcmp(mode,"child-emergency") || !strcmp(mode,"child-power-wrapper"))) {
     signal(SIGTERM,SIG_IGN); signal(SIGUSR2,SIG_IGN);
     const char* identities=getenv("CROSSINK_IDENTITY_PROC_ROOT");
     if(!kindle::captureLauncher(identities) || !kindle::registerReader(identities)) return 19;
     while(access(getenv("MOCK_REQUEST_READY"),F_OK)) usleep(10000);
     if(!kindle::notifyLauncher(strcmp(mode,"child-emergency") ? SIGUSR2 : SIGUSR1,identities)) return 19;
     for(;;) pause();
   }
   if(mode && (!strcmp(mode,"stuck") || !strcmp(mode,"power-stuck"))) { signal(SIGTERM,SIG_IGN); signal(SIGUSR2,SIG_IGN); for(;;) pause(); }
   if(mode && !strcmp(mode,"wait")) for(;;) pause();
   return mode && !strcmp(mode,"fail") ? 23 : 0;
 }
 return 2;
}
''')
        mock = base / "crossink-pw1"
        subprocess.run(["g++", str(native), *[str(ROOT/"platform/kindle"/name) for name in ("LauncherSession.cpp","SessionSupervisor.cpp","SessionHeartbeat.cpp","ProcessDescriptors.cpp","RenderWorker.cpp")], "-pthread", "-I"+str(ROOT/"platform/kindle"), "-o", str(mock)], check=True)
        cases = list(itertools.product(("diagnostic", "ui"), ("normal", "fail", "probe-fail", "interrupt", "recover", "emergency", "stale-pid", "stale-boot", "legacy-pid", "power", "power-stuck", "child-power", "child-emergency", "setup-race", "pause-journal-fail", "child-power-wrapper", "reopen", "stale-reopen")))
        cases += [("ui",mode) for mode in ("bar-hide-fail","bar-show-fail","framework-stopped","bar-journal-fail","repaint-fail","helper-descendant","reopen-during-cleanup","healthy","input-stall","render-stall","whole-stall","startup-stall","supervisor-death","runner-death")]
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
                           MOCK_REQUEST_READY=str(app/"send-request"),
                           MOCK_MODE="stuck" if mode == "emergency" else "wait" if mode in ("interrupt", "recover", "power") else mode)
                if mode in ("healthy","input-stall","render-stall","whole-stall","startup-stall","supervisor-death","runner-death"):
                    env["MOCK_SHORT_WATCHDOG"]="1"
                if mode=="reopen-during-cleanup":
                    env["MOCK_SHOW_BARRIER"]=str(app/"show-release")
                    env["MOCK_SHOW_READY"]=str(app/"show-ready")
                if mode=="helper-descendant":
                    env["MOCK_DESCENDANT"]="1"
                    env["MOCK_DESCENDANT_READY"]=str(app/"descendant-ready")
                    env["MOCK_DESCENDANT_STOP"]=str(app/"descendant-stop")
                if mode=="repaint-fail":
                    env["MOCK_MODE"]="fail"
                    env["MOCK_REPAINT_FAIL"]="1"
                if mode=="pause-journal-fail":
                    source=(app/"runner.sh").read_text().replace('ci_paused=0', 'rm "$CI_LOCK/paused"\nmkdir "$CI_LOCK/paused"\nci_paused=0')
                    (app/"runner.sh").write_text(source)
                if mode=="bar-journal-fail":
                    source=(app/"runner.sh").read_text().replace('echo hidden >', 'mkdir "$CI_LOCK/status-bar"\necho hidden >')
                    (app/"runner.sh").write_text(source)
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
                if mode=="child-power-wrapper":
                    source=(app/"runner.sh").read_text().replace('(exec 9>&-; exec "$CI_ROOT/bin/$ci_binary" "$ci_argument") &', '(exec 9>&-; "$CI_ROOT/bin/$ci_binary" "$ci_argument"; wait) &')
                    (app/"runner.sh").write_text(source)
                if mode=="stale-reopen":
                    stale=app/"state/launch.lock"
                    stale.mkdir(parents=True)
                    (stale/"owner").write_text(str(ui.pid)+"\n")
                    (stale/"identity").write_text("old-boot 1\n")
                if mode=="setup-race":
                    source=(app/"runner.sh").read_text()
                    needle='ci_started=$(process_start "$$") || exit 1'
                    assert source.count(needle)==1
                    source=source.replace(needle,'echo ready > "$CI_ROOT/setup-ready"\nwhile [ ! -f "$CI_ROOT/setup-release" ]; do sleep 0.01; done\n'+needle)
                    (app/"runner.sh").write_text(source)
                runner = subprocess.Popen(["/bin/sh", str(app / "runner.sh"), *arguments], env=env,
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if mode=="setup-race":
                    wait_for((app/"setup-ready").exists,"setup barrier not reached")
                    for _ in range(5):
                        recovery=subprocess.run(["/bin/sh",str(app/"runner.sh"),"--recover"],env=env,capture_output=True,timeout=4)
                        assert recovery.returncode==75, "concurrent recovery entered guarded setup"
                        assert (app/"state/launch.lock").is_dir(), "setup records removed"
                    (app/"setup-release").touch()
                if mode in ("interrupt", "recover", "emergency", "power", "power-stuck", "child-power", "child-emergency", "child-power-wrapper","healthy","input-stall","render-stall","whole-stall","startup-stall","supervisor-death","runner-death"):
                    child_file = app / "state/launch.lock/child"
                    wait_for(child_file.exists, "diagnostic not started")
                    pid = int(child_file.read_text().split()[0])
                    (proc / str(pid)).mkdir()
                    (proc / str(pid) / "status").write_text(f"Name:\t{'sh' if mode=='child-power-wrapper' else binary_name}\nState:\tS (fixture)\n")
                    if mode in ("child-power","child-emergency","child-power-wrapper","healthy","input-stall","render-stall","whole-stall","startup-stall","supervisor-death","runner-death"):
                        wait_for((app/"state/launch.lock/reader").exists,"native reader not registered")
                        reader_pid=int((app/"state/launch.lock/reader").read_text().split()[0])
                        (proc/str(reader_pid)).mkdir(exist_ok=True)
                        (proc/str(reader_pid)/"status").write_text(f"Name:\t{binary_name}\nState:\tS\n")
                        if mode=="child-power-wrapper":
                            assert reader_pid!=pid, "wrapper fixture exec optimized away"
                            (identities/str(pid)/"cmdline").write_bytes(b'/bin/sh\0'+str(app/"runner.sh").encode()+b'\0--guarded\0')
                    before=child_file.read_bytes()
                    stale=subprocess.run(["/bin/sh",str(app/"runner.sh"),"--cleanup","old-session-token","TERM","active","143"],env=env,capture_output=True,timeout=4)
                    assert stale.returncode==0 and child_file.read_bytes()==before, "old cleanup touched replacement session"
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
                    request_time=time.monotonic()
                    if mode in ("child-power", "child-emergency", "child-power-wrapper"):
                        (app/"send-request").touch()
                    elif mode == "interrupt":
                        runner.terminate()
                    elif mode == "emergency":
                        runner.send_signal(signal.SIGUSR1)
                    elif mode in ("power", "power-stuck"):
                        runner.send_signal(signal.SIGUSR2)
                    elif mode in ("healthy","input-stall","render-stall","whole-stall","startup-stall"):
                        (app/"send-request").touch()
                    elif mode=="supervisor-death":
                        runner.kill()
                    elif mode=="runner-death":
                        os.kill(int((app/"state/launch.lock/owner").read_text()),signal.SIGKILL)
                    else:
                        subprocess.run(["/bin/sh", str(app / "runner.sh"), "--recover"],
                                       env=env, check=True, capture_output=True, timeout=6)
                if mode=="reopen-during-cleanup":
                    wait_for((app/"show-ready").exists,"cleanup barrier not reached")
                    next_env=dict(env);next_env.pop("MOCK_SHOW_BARRIER")
                    reopening=subprocess.Popen(["/bin/sh",str(app/"runner.sh"),*arguments],env=next_env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                    time.sleep(.2)
                    assert reopening.poll() is None,"reopen refused instead of waiting for cleanup"
                    (app/"show-release").touch()
                    assert reopening.wait(timeout=12)==0,"immediate reopen failed after cleanup"
                result = runner.wait(timeout=12)
                if mode in ("child-power","child-emergency","child-power-wrapper"):
                    assert time.monotonic()-request_time<4.5, "app-to-launcher recovery exceeded grace window"
                expected = {"normal": 0, "fail": 23, "probe-fail": 1, "interrupt": 143, "recover": 143, "emergency": 130, "stale-pid": 0, "stale-boot": 0, "legacy-pid": 0, "power": 0, "power-stuck": 0,"bar-hide-fail":1,"bar-show-fail":0,"framework-stopped":1,"child-power":0,"child-emergency":130,"child-power-wrapper":0,"reopen":0,"stale-reopen":0,"helper-descendant":0,"reopen-during-cleanup":0,"setup-race":0,"pause-journal-fail":1,"bar-journal-fail":1,"repaint-fail":23,"healthy":0,"input-stall":1,"render-stall":1,"whole-stall":1,"startup-stall":1,"supervisor-death":-9,"runner-death":137}[mode]
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
                if mode=="repaint-fail":
                    assert (app/"state/launch.lock").exists(), "failed repaint lost recovery records"
                    retry_env=dict(env);retry_env.pop("MOCK_REPAINT_FAIL")
                    subprocess.run(["/bin/sh",str(app/"runner.sh"),"--recover"],env=retry_env,check=True,capture_output=True,timeout=6)
                wait_for(lambda: not (app/"state/launch.lock").exists(), "lock leaked")
                log = (app / "logs" / (profile+".log")).read_text()
                if mode in ("probe-fail","pause-journal-fail"):
                    assert "paused pid=" not in log, "UI touched after failed probe"
                else:
                    assert "paused pid=" in log and "resumed pid=" in log, "handoff missing"
                    assert (f"paused pid={volume.pid} name=volumd" in log) == (profile=="ui"), "USB ownership mismatch"
                if profile=="ui" and mode not in ("probe-fail","pause-journal-fail"):
                    assert f"paused pid={ui.pid} name=cvm" not in log
                    assert f"paused pid={wm.pid} name=awesome" not in log
                    assert f"resumed pid={ui.pid} name=cvm" not in log
                    assert f"resumed pid={wm.pid} name=awesome" not in log
                    assert "mock_power_state stage=before-prepare" in log
                    if mode not in ("bar-hide-fail","framework-stopped","bar-journal-fail"):
                        assert "mock_power_state stage=after-prepare" in log
                        assert "framework_policy=cooperative" in log
                    if mode not in ("framework-stopped","bar-journal-fail"):
                        assert "mock_status_bar=hide" in log and "mock_status_bar=show" in log
                    if mode in ("bar-hide-fail","framework-stopped","bar-journal-fail"):
                        assert "mock_app=started" not in log, "reader launched after failed preparation"
                    if mode in ("normal","fail","stale-pid","stale-boot","legacy-pid"):
                        assert "mock_power_state stage=before-release" in log
                if mode in ("child-power","child-emergency","child-power-wrapper"):
                    assert "launcher_signal=" in log and "sent=1 owner=" in log, "production notifier did not signal launcher"
                if mode in ("power", "power-stuck", "child-power"):
                    assert "power_handoff=launcher grace_seconds=2" in log, "power exit not acknowledged"
                if mode in ("input-stall","render-stall","whole-stall","startup-stall"):
                    watch=(app/"logs/launcher.log").read_text()
                    assert "supervisor=timeout reason=" in watch,"stalled loop was not detected"
                    assert "reader_termination pid=" in log,"stalled reader was not recovered"
                if mode=="healthy":
                    assert "supervisor=timeout" not in (app/"logs/launcher.log").read_text(),"healthy idle reader timed out"
                if mode in ("reopen","stale-reopen","helper-descendant"):
                    second_env=dict(env);second_env.pop("MOCK_DESCENDANT",None)
                    if mode=="helper-descendant":
                        assert (app/"descendant-ready").exists(), "retained-descriptor helper not started"
                    second=subprocess.run(["/bin/sh",str(app/"runner.sh"),*arguments],env=second_env,capture_output=True,timeout=12)
                    assert second.returncode==0,("reopen failed",mode,second.stderr)
                    assert not (app/"state/launch.lock").exists(),"reopened session not released"
                if mode=="child-power-wrapper":
                    assert "wrapper_termination pid=" in log, "shell intermediary not terminated"
                assert (app/"logs/launcher.log").stat().st_size>0,"early launch attempt not logged"
                print(f"launcher_check {profile}/{mode}=ok",flush=True)
            except BaseException:
                for name in ("launcher.log",profile+".log",profile+".previous.log"):
                    path=app/"logs"/name
                    if path.exists(): print(f"failure_evidence {profile}/{mode} {name}\n"+path.read_text(),flush=True)
                raise
            finally:
                (app/"descendant-stop").touch()
                (app/"show-release").touch()
                if runner and runner.poll() is None:
                    runner.kill()
                    runner.wait()
                for process in (ui, wm, previously_stopped, volume):
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    main()
