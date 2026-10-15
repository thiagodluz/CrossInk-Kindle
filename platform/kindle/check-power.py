#!/usr/bin/env python3
"""Check power-event fragmentation, subscriber failure, cleanup and parent death."""
import argparse
import os
import subprocess
import tempfile
import time
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', required=True, type=Path)
parser.add_argument('--qemu', type=Path)
parser.add_argument('--sysroot', type=Path)
args = parser.parse_args()
binary = str(args.binary.resolve())
if bool(args.qemu) != bool(args.sysroot):
    parser.error('--qemu and --sysroot must be supplied together for the legacy runtime check')
launch = [binary]
if args.qemu:
    launch = [str(args.qemu.resolve()),'-L',str(args.sysroot.resolve()),binary]
    # Merely running an ARM static binary without a target sysroot hides the
    # legacy grantpt/NSS crash: dlopen cannot find its ARM shared libraries.
    traced = subprocess.run([launch[0],'-strace',*launch[1:],'/bin/false','4'],
                            text=True,capture_output=True,timeout=8)
    assert traced.returncode==0,traced.stdout+traced.stderr
    assert not any(name in traced.stderr for name in ('libnss_','/lib/libc.so','/lib/ld-linux')), traced.stderr
    print('power_check legacy-runtime-no-NSS=ok',flush=True)
with tempfile.TemporaryDirectory(prefix='crossink-power-') as directory:
    base = Path(directory)
    helper = base / 'lipc-wait-event'
    helper.write_text('''#!/usr/bin/env python3
import os,sys,time,signal
assert sys.argv[1:]==['-m','-s','0','com.lab126.powerd',
 'goingToScreenSaver,outOfScreenSaver,exitingScreenSaver,charging,notCharging,wakeupFromSuspend,readyToSuspend'],sys.argv
mode=os.environ['MOCK_POWER_MODE']
if mode=='heartbeat':
    with open(os.environ['MOCK_POWER_HEARTBEAT'],'wb',buffering=0) as f:
        while True: f.write(b'x'); time.sleep(.03)
if mode=='failed': sys.exit(9)
assert sys.stdout.isatty(), 'helper stdout must be a terminal'
if mode=='overflow': os.write(1,b'x'*200+b'\\n')
if mode=='resume': os.write(1,b'outOfScreenSaver 1\\n')
elif mode=='late': os.write(1,b'readyToSuspend 0\\n')
else:
    os.write(1,b'battLevelChanged 72\\ngoingToScreenSaverWrong 2\\ngoingToScreen')
    time.sleep(.05)
    os.write(1,b'Saver 2\\n')
# Test escalation in PowerMonitor.stop when the helper ignores SIGTERM.
signal.signal(signal.SIGTERM,signal.SIG_IGN)
while True: time.sleep(1)
''')
    helper.chmod(0o755)
    buffered_source = base / 'buffered.cpp'
    buffered_source.write_text('''#include <cstdio>
#include <unistd.h>
int main() {
    // Deliberately use default libc buffering: a pipe retains this small event.
    std::puts("goingToScreenSaver 2");
    for (;;) pause();
}
''')
    buffered_helper = base / 'buffered-helper'
    subprocess.run(['g++',str(buffered_source),'-o',str(buffered_helper)],check=True)
    for mode, expected in [('buffered',1),('split',1),('overflow',1),('late',2),('resume',3),('failed',4)]:
        env = dict(os.environ, MOCK_POWER_MODE=mode)
        result = subprocess.run([*launch,str(buffered_helper if mode=='buffered' else helper),str(expected)],env=env,text=True,
                                capture_output=True,timeout=12)
        assert result.returncode==0, result.stdout+result.stderr
        assert 'bytes_received=' in result.stdout and 'partial_bytes=' in result.stdout, 'subscriber diagnostics missing'
        assert 'events=goingToScreenSaver,outOfScreenSaver,' in result.stdout, 'named subscription missing'
        if mode!='failed':
            expected_lines = 4 if mode=='overflow' else 1 if mode in ('late','resume','buffered') else 3
            assert result.stdout.count(f'lines_received={expected_lines} ')==2, 'received count or restart reset incorrect'
            assert 'power_raw=' in result.stdout, 'raw event diagnostics missing'
        if mode in ('split','overflow'):
            assert 'power_raw=battLevelChanged 72' in result.stdout, 'unknown events hidden'
            assert 'power_event=goingToScreenSaverWrong' not in result.stdout, 'invalid event accepted'
        if mode=='overflow':
            assert 'power_raw=oversized-discarded' in result.stderr, 'oversized event diagnostic missing'
        print(f'power_check {mode}=ok',flush=True)
    heartbeat = base / 'heartbeat'
    env = dict(os.environ,MOCK_POWER_MODE='heartbeat',MOCK_POWER_HEARTBEAT=str(heartbeat))
    reader = subprocess.Popen([*launch,str(helper),'hold'],env=env,stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    try:
        deadline=time.monotonic()+5
        while not heartbeat.exists() or heartbeat.stat().st_size<3:
            assert time.monotonic()<deadline,'subscriber heartbeat did not start'
            time.sleep(.02)
        reader.kill()
        reader.wait(timeout=3)
        time.sleep(.1)
        size=heartbeat.stat().st_size
        time.sleep(.2)
        assert heartbeat.stat().st_size==size,'subscriber survived reader SIGKILL'
        print('power_check subscriber-parent-death=ok',flush=True)
    finally:
        if reader.poll() is None: reader.kill(); reader.wait()
    state_helper = base / 'lipc-get-prop'
    state_helper.write_text('''#!/usr/bin/env python3
import os,sys,time,signal
assert sys.argv[1:]==['com.lab126.powerd','state']
mode=os.environ['MOCK_STATE_MODE']
if mode=='ok': print('active')
elif mode=='fail': sys.exit(17)
elif mode=='overflow': print('x'*200)
elif mode=='empty': pass
else:
    signal.signal(signal.SIGTERM,signal.SIG_IGN)
    if mode=='closed': os.close(1)
    if mode=='flood':
        while True: os.write(1,b'x'*512)
    while True: time.sleep(1)
''')
    state_helper.chmod(0o755)
    for mode in ('ok','fail','overflow','empty','timeout','closed','flood'):
        started=time.monotonic()
        result=subprocess.run([*launch,str(state_helper),'state-'+mode],
            env=dict(os.environ,MOCK_STATE_MODE=mode),text=True,capture_output=True,timeout=3)
        assert result.returncode==0,result.stdout+result.stderr
        assert 'power_state stage=check' in result.stdout
        if mode=='ok': assert 'value=active exit=0 timeout=0 truncated=0' in result.stdout
        if mode in ('timeout','closed','flood'): assert 'timeout=1' in result.stdout
        assert time.monotonic()-started<2, 'state query blocked shutdown'
        print(f'power_state_check {mode}=ok',flush=True)
print('Power monitor checks passed',flush=True)
