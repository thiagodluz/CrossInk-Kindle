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
args = parser.parse_args()
binary = str(args.binary.resolve())
with tempfile.TemporaryDirectory(prefix='crossink-power-') as directory:
    base = Path(directory)
    helper = base / 'lipc-wait-event'
    helper.write_text('''#!/usr/bin/env python3
import os,sys,time,signal
assert sys.argv[1:]==['-m','-s','0','com.lab126.powerd','*'],sys.argv
mode=os.environ['MOCK_POWER_MODE']
if mode=='heartbeat':
    with open(os.environ['MOCK_POWER_HEARTBEAT'],'wb',buffering=0) as f:
        while True: f.write(b'x'); time.sleep(.03)
if mode=='failed': sys.exit(9)
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
    for mode, expected in [('split',1),('overflow',1),('late',2),('resume',3),('failed',4)]:
        env = dict(os.environ, MOCK_POWER_MODE=mode)
        result = subprocess.run([binary,str(helper),str(expected)],env=env,text=True,
                                capture_output=True,timeout=12)
        assert result.returncode==0, result.stdout+result.stderr
        print(f'power_check {mode}=ok',flush=True)
    heartbeat = base / 'heartbeat'
    env = dict(os.environ,MOCK_POWER_MODE='heartbeat',MOCK_POWER_HEARTBEAT=str(heartbeat))
    reader = subprocess.Popen([binary,str(helper),'hold'],env=env,stdout=subprocess.DEVNULL,
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
print('Power monitor checks passed',flush=True)
