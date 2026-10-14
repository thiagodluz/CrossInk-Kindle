#!/usr/bin/env python3
"""Build and audit the KUAL TXT preview, diagnostic, notices and relink objects."""
import argparse
import hashlib
import json
import re
import runpy
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRIPLE = 'arm-kindle5-linux-gnueabi'
def run(*args):
    return subprocess.check_output([str(a) for a in args], cwd=ROOT, text=True)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def audit(binary, tools):
    readelf = tools / (TRIPLE + '-readelf')
    sections = {flag: run(readelf, flag, binary) for flag in ('-h','-A','-l','-d','-n')}
    if not all(value in sections['-h'] for value in ('ELF32','ARM','little endian','soft-float ABI')):
        raise RuntimeError('Expected ARM32 LE base calling convention')
    if 'INTERP' in sections['-l'] or 'NEEDED' in sections['-d'] or 'Tag_ABI_VFP_args: VFP registers' in sections['-A']:
        raise RuntimeError('Expected static ARM softfp executable')
    match = re.search(r'OS: Linux, ABI: (\d+\.\d+\.\d+)', sections['-n'])
    if not match or tuple(map(int, match[1].split('.'))) > (2,6,31):
        raise RuntimeError('Executable does not match PW1 minimum Linux ABI')
    return '\n'.join(sections.values())

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, default=ROOT.parent/'toolchains/x-tools'/TRIPLE)
    parser.add_argument('--output', type=Path, default=ROOT/'build/kindle-pw1/release-ui')
    args = parser.parse_args()
    toolchain, output = args.toolchain.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    tools = toolchain/'bin'
    if run(tools/(TRIPLE+'-g++'), '-dumpmachine').strip() != TRIPLE:
        raise RuntimeError('Use the kindle5 toolchain')
    ui = runpy.run_path(str(ROOT/'scripts/kindle/build-ui.py'))['build'](
        toolchain, output/'ui', ROOT/'platform/kindle/app/PortApp.cpp', False)
    run('python3', ROOT/'scripts/kindle/build-package.py', '--toolchain', toolchain,
        '--output', output/'diagnostic')
    diagnostic = output/'diagnostic/crossink-pw1'
    receipt = json.loads((output/'ui/arm-build.json').read_text())
    if receipt['host'] or receipt['binary_sha256'] != sha(ui):
        raise RuntimeError('UI build receipt is not for the packaged ARM binary')
    for name, expected in receipt['sources_sha256'].items():
        if sha(ROOT/name) != expected:
            raise RuntimeError('Source changed during compilation: '+name)
    if receipt['sdk_commit'] != '699370183fa3a0e33c9cb83a36f701bbb6022095':
        raise RuntimeError('Unexpected SDK pin')
    if run('git','-C','freeink-sdk','status','--porcelain').strip():
        raise RuntimeError('SDK has unrecorded source changes')
    ui_audit = output/'ui-elf-audit.txt'
    ui_audit.write_text(audit(ui, tools))
    audit(diagnostic, tools)
    run(tools/(TRIPLE+'-strip'),'--strip-unneeded',ui)
    entries = {
        'crossink/bin/crossink-ui':ui, 'crossink/bin/crossink-pw1':diagnostic,
        'crossink/runner.sh':ROOT/'platform/kindle/runner.sh',
        'crossink/collect-diagnostics.sh':ROOT/'scripts/kindle/collect-diagnostics.sh',
        'extensions/crossink/config.xml':ROOT/'kual/crossink/config.xml',
        'extensions/crossink/menu.json':ROOT/'kual/crossink/menu.json',
        'extensions/crossink/start.sh':ROOT/'kual/crossink/start.sh',
        'crossink/data/books/Boas-vindas.txt':ROOT/'platform/kindle/examples/Boas-vindas.txt',
        'LEIA-ME.md':ROOT/'platform/kindle/UI-README.md',
        'crossink/development/ui-elf-audit.txt':ui_audit,
        'crossink/development/diagnostic-elf-audit.txt':output/'diagnostic/elf-audit.txt',
        'crossink/development/ui-build.json':output/'ui/arm-build.json',
        'crossink/development/diagnostic-build.json':output/'diagnostic/build-manifest.json',
        'crossink/development/relink.py':ROOT/'scripts/kindle/relink.py',
        'crossink/development/README.md':ROOT/'platform/kindle/RELINK.md',
        'crossink/licenses/CrossInk-MIT.txt':ROOT/'LICENSE',
        'crossink/licenses/THIRD_PARTY_NOTICES.md':ROOT/'THIRD_PARTY_NOTICES.md',
        'crossink/licenses/FreeInk-MIT.txt':ROOT/'freeink-sdk/LICENSE',
        'crossink/licenses/FreeInk-NOTICE.txt':ROOT/'freeink-sdk/NOTICE',
        'crossink/licenses/Simulator-compat-MIT.txt':ROOT/'platform/kindle/compat/SIMULATOR-COMPAT-LICENSE.txt',
        'crossink/licenses/Bitter-OFL.txt':ROOT/'lib/EpdFont/scalableFonts/licenses/Bitter-OFL.txt',
        'crossink/licenses/LexendDeca-OFL.txt':ROOT/'lib/EpdFont/scalableFonts/licenses/LexendDeca-OFL.txt',
        'crossink/licenses/MiniBidi-MIT.txt':ROOT/'platform/kindle/licenses/MiniBidi-MIT.txt',
        'crossink/licenses/uzlib-notice.c':ROOT/'lib/uzlib/src/tinflate.c',
    }
    for library, files in {
        'glibc':['COPYING','COPYING.LIB','LICENSES'],
        'gcc':['COPYING3','COPYING3.LIB','COPYING.RUNTIME'],
    }.items():
        for name in files:
            entries[f'crossink/licenses/{library}-{name}']=toolchain/'share/licenses'/library/name
    for item in receipt['objects']:
        path = output/'ui/arm-objects'/item['name']
        if sha(path) != item['sha256']: raise RuntimeError('Changed object: '+str(path))
        entries['crossink/development/ui-objects/'+path.name]=path
    for path in (output/'diagnostic/diagnostic-objects').glob('*.o'):
        entries['crossink/development/diagnostic-objects/'+path.name]=path
    manifest = {'version':'0.2.0','is_full_reader':False,'formats':['TXT UTF-8'],
                'source_commit':receipt['source_commit'],'source_dirty':receipt['source_dirty'],
                'sdk_commit':receipt['sdk_commit'],'session_timeout_seconds':300,
                'watchdog_seconds':360,'linkage':'static','minimum_kernel':'2.6.31',
                'files_sha256':{name:sha(path) for name,path in entries.items()}}
    archive = output/'CrossInk-PW1-KUAL-previa-TXT-0.2.0.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as package:
        for name,path in entries.items():
            info=zipfile.ZipInfo(name,(2026,10,14,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED
            executable=name.endswith(('.sh','.py')) or '/bin/' in name
            info.external_attr=(0o100755 if executable else 0o100644)<<16
            package.writestr(info,path.read_bytes())
        package.writestr('crossink/build-manifest.json',json.dumps(manifest,indent=2)+'\n')
    print(archive)
if __name__ == '__main__':
    main()
