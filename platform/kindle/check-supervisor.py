#!/usr/bin/env python3
"""Exercise the native pipe supervisor on host or static ARM with a legacy sysroot."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import shlex
import signal

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',required=True,type=Path)
p.add_argument('--qemu',type=Path)
p.add_argument('--sysroot',type=Path)
args=p.parse_args()
if bool(args.qemu)!=bool(args.sysroot):p.error('qemu requires sysroot')
launch=[str(args.binary.resolve())]
if args.qemu:launch=[str(args.qemu.resolve()),'-L',str(args.sysroot.resolve()),*launch]
subprocess.run(launch,check=True,timeout=5)
with tempfile.TemporaryDirectory(prefix='pw1-supervisor-') as d:
    root=Path(d)
    helper=root/'helper.py'
    helper.write_text('''import os,time,signal
fd=int(os.environ['CROSSINK_HEARTBEAT_FD'])
mode=os.environ['MOCK_SUPERVISOR_MODE']
start=time.monotonic()
if mode!='startup':os.write(fd,b'IR')
while time.monotonic()-start<6:
    os.write(fd,b'IR' if mode=='healthy' else b'I' if mode=='render' else b'R' if mode=='input' else b'')
    time.sleep(.1)
''')
    runner=root/'runner.sh'
    runner.write_text('#!/bin/sh\nexec '+shlex.join([sys.executable,str(helper)])+'\n')
    for mode in ('healthy','input','render','startup','healthy-sigchld'):
        start=time.monotonic()
        r=subprocess.run([*launch,'--supervise',str(runner)],
            env=dict(os.environ,CROSSINK_ROOT=d,MOCK_SUPERVISOR_MODE='healthy' if mode=='healthy-sigchld' else mode),
            preexec_fn=(lambda: signal.signal(signal.SIGCHLD,signal.SIG_IGN)) if mode=='healthy-sigchld' else None,
            text=True,capture_output=True,timeout=9)
        assert r.returncode==(0 if mode.startswith('healthy') else 1),r.stdout+r.stderr
        assert ('supervisor=timeout reason='+mode in r.stdout)==(not mode.startswith('healthy')),r.stdout
        if mode.startswith('healthy'):assert time.monotonic()-start>=6,'healthy session ended early'
        print('supervisor_check '+mode+'=ok',flush=True)
