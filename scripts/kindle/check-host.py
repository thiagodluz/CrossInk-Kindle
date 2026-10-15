#!/usr/bin/env python3
"""Run all Kindle port checks using a normal Linux compiler; no device required."""
import runpy
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True, timeout=180)

def main():
    build = runpy.run_path(str(ROOT / 'scripts/kindle/build-ui.py'))['build']
    output = ROOT / 'build/kindle-pw1/host-checks'
    output.mkdir(parents=True, exist_ok=True)
    backend = output / 'backend-checks'
    run('g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
        'platform/kindle/KindleBackends.cpp', 'platform/kindle/BackendChecks.cpp', '-o', backend)
    run(backend)
    power = output / 'power-checks'
    run('g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
        'platform/kindle/KindlePower.cpp', 'platform/kindle/PowerChecks.cpp', '-o', power)
    run('python3', 'platform/kindle/check-power.py', '--binary', power)
    run('python3', 'platform/kindle/check-launcher.py')
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
