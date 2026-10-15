#!/usr/bin/env python3
"""Check the production notifier and kernel lock, including owner death."""
import argparse
import fcntl
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
launch = [str(args.binary.resolve())]
if bool(args.qemu) != bool(args.sysroot):
    parser.error('--qemu and --sysroot must be supplied together')
if args.qemu:
    launch = [str(args.qemu.resolve()), '-L', str(args.sysroot.resolve()), *launch]
with tempfile.TemporaryDirectory(prefix='crossink-session-') as directory:
    root = Path(directory)
    (root/'state/launch.lock').mkdir(parents=True)
    (root/'proc/sys/kernel/random').mkdir(parents=True)
    env = dict(os.environ, CROSSINK_ROOT=directory)
    subprocess.run([*launch, '--notify-test'], env=env, check=True, timeout=10)
    runner = root/'runner.sh'
    runner.write_text('''#!/bin/sh
case "$2" in
  --hold) echo ready > "$CROSSINK_ROOT/ready"; read -r token ;;
  --touch|--cleanup) echo changed > "$CROSSINK_ROOT/changed" ;;
  *) exit 2 ;;
esac
''')
    guard = [*launch, '--session-guard', str(runner)]
    holder = subprocess.Popen([*guard, '--hold'], env=env, stdin=subprocess.PIPE)
    try:
        deadline = time.monotonic()+5
        while not (root/'ready').exists():
            assert time.monotonic()<deadline, 'lock holder did not start'
            time.sleep(.01)
        inode = (root/'state/operation.lock').stat().st_ino
        for _ in range(4):
            blocked = subprocess.run([*guard, '--touch'], env=env, capture_output=True, timeout=3)
            assert blocked.returncode == 75, 'two operations acquired the same kernel lock'
        assert not (root/'changed').exists(), 'blocked operation mutated state'
        cleanup = subprocess.Popen([*guard,'--cleanup'],env=env)
        time.sleep(.2)
        assert cleanup.poll() is None, 'cleanup was dropped during transient contention'
        holder.stdin.write(b'release\n')
        holder.stdin.flush()
        holder.wait(timeout=3)
        cleanup.wait(timeout=3)
        assert cleanup.returncode==0, 'cleanup did not retry transient contention'
        holder.stdin.close()
        # Acquire again and verify that SIGKILL releases the same lock inode.
        (root/'ready').unlink()
        holder = subprocess.Popen([*guard,'--hold'],env=env,stdin=subprocess.PIPE)
        deadline = time.monotonic()+5
        while not (root/'ready').exists():
            assert time.monotonic()<deadline, 'second lock holder did not start'
            time.sleep(.01)
        blocked = subprocess.run([*guard,'--touch'],env=env,capture_output=True,timeout=3)
        assert blocked.returncode==75
        holder.kill()
        holder.wait(timeout=3)
        subprocess.run([*guard, '--touch'], env=env, check=True, timeout=3)
        assert (root/'changed').read_text().strip() == 'changed'
        assert (root/'state/operation.lock').stat().st_ino == inode, 'lock inode replaced'
        print('session_check serialization-and-owner-death=ok', flush=True)

        # Explicit unlock must release inherited copies, but a delayed unlock
        # through that old open-file description must leave a new owner alone.
        (root/'ready').unlink()
        previous_nine = None
        try:
            previous_nine = fcntl.fcntl(9, fcntl.F_DUPFD_CLOEXEC, 10)
        except OSError:
            pass
        descriptor = os.open(root/'state/operation.lock', os.O_RDWR)
        try:
            if descriptor != 9:
                os.dup2(descriptor, 9)
                os.close(descriptor)
            fcntl.flock(9, fcntl.LOCK_EX | fcntl.LOCK_NB)
            subprocess.run([*launch, '--session-unlock'], env=env, pass_fds=(9,), check=True, timeout=3)
            holder = subprocess.Popen([*guard,'--hold'],env=env,stdin=subprocess.PIPE)
            deadline = time.monotonic()+5
            while not (root/'ready').exists():
                assert time.monotonic()<deadline, 'explicit unlock left inherited copies locked'
                time.sleep(.01)
            subprocess.run([*launch, '--session-unlock'], env=env, pass_fds=(9,), check=True, timeout=3)
            blocked = subprocess.run([*guard,'--touch'],env=env,capture_output=True,timeout=3)
            assert blocked.returncode==75, 'delayed old unlock released a replacement operation'
            holder.stdin.write(b'release\n')
            holder.stdin.flush()
            holder.wait(timeout=3)
            assert holder.returncode==0
            print('session_check inherited-unlock-and-replacement-isolation=ok', flush=True)
        finally:
            os.close(9)
            if previous_nine is not None:
                os.dup2(previous_nine, 9)
                os.close(previous_nine)
    finally:
        if holder.poll() is None:
            holder.kill()
            holder.wait()
        holder.stdin.close()
