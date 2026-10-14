#!/usr/bin/env python3
"""Relink the supplied application objects against a compatible kindle5 sysroot."""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--toolchain', required=True, type=Path)
parser.add_argument('--profile', choices=['ui','diagnostic'], default='ui')
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parent
compiler = args.toolchain.resolve()/'bin/arm-kindle5-linux-gnueabi-g++'
if args.profile == 'ui':
    receipt = json.loads((root/'ui-build.json').read_text())
    objects = [root/'ui-objects'/obj['name'] for obj in receipt['objects']]
    flags = receipt['link_flags']
else:
    objects = sorted((root/'diagnostic-objects').glob('*.o'))
    flags = ['-static','-mcpu=cortex-a8','-mfpu=neon','-mfloat-abi=softfp','-lrt']
if not objects or not all(path.is_file() for path in objects):
    raise SystemExit('Missing application objects')
subprocess.run([str(compiler),*[str(path) for path in objects],*flags,'-o',str(args.output.resolve())], check=True)
