#!/usr/bin/env python3
"""Stock helpers and their descendants must not retain a launcher flock."""
import argparse
import os
import shlex
import subprocess
import tempfile
import time
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--power-binary', required=True, type=Path)
parser.add_argument('--session-binary', required=True, type=Path)
parser.add_argument('--qemu', type=Path)
parser.add_argument('--sysroot', type=Path)
args = parser.parse_args()
if bool(args.qemu) != bool(args.sysroot):
    parser.error('--qemu and --sysroot must be supplied together')
prefix = [str(args.qemu.resolve()), '-L', str(args.sysroot.resolve())] if args.qemu else []
power = [*prefix, str(args.power_binary.resolve())]
session = [*prefix, str(args.session_binary.resolve())]
for mode in ('query', 'subscriber'):
    with tempfile.TemporaryDirectory(prefix='crossink-descriptors-') as directory:
        root = Path(directory)
        runner = root/'runner.sh'
        runner.write_text('''#!/bin/sh
if [ "$2" = --noop ]; then exit 0; fi
exec 8>&9
"$POWER_RUN" "$POWER_INPUT" "$POWER_EXPECTED"
''')
        wrapper = root/'power-run'
        wrapper.write_text('#!/bin/sh\nexec '+shlex.join(power)+' "$@"\n')
        wrapper.chmod(0o755)
        helper = root/('recovery-query' if mode=='query' else 'stock-helper')
        helper.write_text('''#!/usr/bin/env python3
import os,signal,time
from pathlib import Path
root=Path(os.environ['CROSSINK_ROOT'])
lock=(root/'state/operation.lock').stat()
for name in os.listdir('/proc/self/fd'):
    try: info=os.fstat(int(name))
    except OSError: continue
    assert (info.st_dev,info.st_ino)!=(lock.st_dev,lock.st_ino), 'stock helper inherited operation lock'
pid=os.fork()
if pid==0:
    for fd in (0,1,2):
        try: os.close(fd)
        except OSError: pass
    with open(root/'heartbeat','ab',buffering=0) as heartbeat:
        deadline=time.monotonic()+20
        while not (root/'stop').exists() and time.monotonic()<deadline:
            heartbeat.write(b'x');time.sleep(.01)
    os._exit(0)
if os.environ['STOCK_MODE']=='query': print('active',flush=True)
else:
    print('goingToScreenSaver 2',flush=True)
    signal.signal(signal.SIGTERM,signal.SIG_IGN)
    while True: time.sleep(1)
''')
        helper.chmod(0o755)
        home = root/'recovery-home'
        home.write_text('#!/bin/sh\nexit 0\n')
        home.chmod(0o755)
        env = dict(os.environ, CROSSINK_ROOT=directory, POWER_RUN=str(wrapper), STOCK_MODE=mode,
                   POWER_INPUT=directory if mode=='query' else str(helper),
                   POWER_EXPECTED='recovery-active' if mode=='query' else '1')
        guard = [*session, '--session-guard', str(runner)]
        try:
            result = subprocess.run(guard, env=env, capture_output=True, text=True, timeout=10)
            assert result.returncode==0, result.stdout+result.stderr
            heartbeat = root/'heartbeat'
            deadline = time.monotonic()+3
            while not heartbeat.exists():
                assert time.monotonic()<deadline
                time.sleep(.01)
            size = heartbeat.stat().st_size
            time.sleep(.05)
            assert heartbeat.stat().st_size>size, 'descendant fixture exited before reopen'
            reopened = subprocess.run([*guard,'--noop'],env=env,capture_output=True,text=True,timeout=3)
            assert reopened.returncode==0, reopened.stdout+reopened.stderr
            print(f'descriptor_check {mode}-descendant-and-reopen=ok',flush=True)
        finally:
            (root/'stop').touch()
            time.sleep(.03)
