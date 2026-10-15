#!/usr/bin/env python3
"""Run all Kindle port checks using a normal Linux compiler; no device required."""
import runpy
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def run(*args, timeout=180):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True, timeout=timeout)

def main():
    build = runpy.run_path(str(ROOT / 'scripts/kindle/build-ui.py'))['build']
    output = ROOT / 'build/kindle-pw1/host-checks'
    output.mkdir(parents=True, exist_ok=True)
    backend = output / 'backend-checks'
    run('g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
        'platform/kindle/KindleBackends.cpp', 'platform/kindle/BackendChecks.cpp', '-o', backend)
    run(backend)
    display = output / 'display-checks'
    run('g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
        'platform/kindle/KindleBackends.cpp', 'platform/kindle/DisplayChecks.cpp',
        '-Wl,--wrap=open,--wrap=close,--wrap=mmap,--wrap=munmap,--wrap=ioctl', '-o', display)
    run(display)
    power = output / 'power-checks'
    run('g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
        'platform/kindle/KindlePower.cpp', 'platform/kindle/KindleSystem.cpp', 'platform/kindle/ProcessDescriptors.cpp', 'platform/kindle/PowerChecks.cpp', '-o', power)
    run('python3', 'platform/kindle/check-power.py', '--binary', power)
    session = output / 'session-checks'
    run('g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
        'platform/kindle/LauncherSession.cpp', 'platform/kindle/SessionChecks.cpp', '-o', session)
    run('python3', 'platform/kindle/check-session.py', '--binary', session)
    run('python3', 'platform/kindle/check-descriptors.py', '--power-binary', power, '--session-binary', session)
    supervisor = output / 'supervisor-checks'
    run('g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
        'platform/kindle/SessionHeartbeat.cpp', 'platform/kindle/SessionSupervisor.cpp',
        'platform/kindle/LauncherSession.cpp', 'platform/kindle/ProcessDescriptors.cpp',
        'platform/kindle/SupervisorChecks.cpp', '-o', supervisor)
    run('python3', 'platform/kindle/check-supervisor.py', '--binary', supervisor)
    run('python3', 'platform/kindle/check-launcher.py', timeout=300)
    ui = build(Path(), output / 'ui', ROOT / 'platform/kindle/app/PortApp.cpp', True)
    run('python3', 'platform/kindle/check-ui.py', '--binary', ui)
    native = build(Path(), output / 'native', ROOT / 'platform/kindle/NativeChecks.cpp', True)
    with tempfile.TemporaryDirectory(prefix='crossink-native-') as temporary:
        data = Path(temporary) / 'data'
        data.mkdir()
        run(native, data)
    print('All Kindle host checks passed', flush=True)

if __name__ == '__main__':
    main()
