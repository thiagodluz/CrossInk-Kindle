#!/usr/bin/env python3
"""Check native TXT pagination, touch routing, progress and host/ARM pixels."""
import argparse
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', required=True, type=Path)
parser.add_argument('--qemu', type=Path)
parser.add_argument('--compare-host', type=Path)
args = parser.parse_args()
prefix = [str(args.qemu.resolve()),'-cpu','cortex-a8'] if args.qemu else []
def preview(binary, root, output, *options, success=True, emulated=True):
    result = subprocess.run((prefix if emulated else [])+[str(binary.resolve()),'--preview',
                            '--data-root',str(root),'--output',str(output),*options],
                           capture_output=True,text=True,timeout=60)
    assert (result.returncode == 0) == success, result.stdout+result.stderr
    return result.stdout
with tempfile.TemporaryDirectory(prefix='crossink-ui-') as directory:
    base = Path(directory)
    root = base/'data'; (root/'books').mkdir(parents=True)
    example = root/'books/Boas-vindas.txt'
    shutil.copyfile(ROOT/'platform/kindle/examples/Boas-vindas.txt',example)
    book = '/books/Boas-vindas.txt'
    image = base/'reading.pgm'
    log = preview(args.binary,root,image,'--book',book)
    assert 'resume_page=1' in log and 'preview page=1 count=5' in log, log
    if args.compare_host:
        hostroot=base/'host';(hostroot/'books').mkdir(parents=True)
        shutil.copyfile(example,hostroot/'books/Boas-vindas.txt')
        preview(args.compare_host,hostroot,base/'host.pgm','--book',book,emulated=False)
        assert image.read_bytes() == (base/'host.pgm').read_bytes(), 'ARM/host rendering differs'
    log = preview(args.binary,root,image,'--tap','380,180','--tap','470,960',
                  '--tap','110,960','--tap','380,180','--tap','650,960')
    assert 'resume_page=2' in log and 'preview page=2 count=5' in log, log
    log = preview(args.binary,root,image,'--book',book)
    assert 'resume_page=2' in log, log
    example.write_text('Conteúdo alterado.\n',encoding='utf-8')
    log = preview(args.binary,root,image,'--book',book)
    assert 'resume_page=1' in log and 'count=1' in log, log
    for name, content in [('vazio.txt',b''),('literal-replacement.txt','\ufffd'.encode()),('acentos.txt',('\ufeffAção e coração\r\n'+ 'á'*150+'\r\n').encode())]:
        (root/'books'/name).write_bytes(content)
        preview(args.binary,root,image,'--book','/books/'+name)
    for name,content in [('grande.txt',b'x'*(256*1024+1)),('binario.txt',b'a\0b'),
                         ('livro.epub',b'PK'),('paginas.txt',b'\n'*16000),
                         ('latin1.txt','Ação coração não'.encode('latin-1')),
                         ('cp1252.txt','“Ação” — coração'.encode('cp1252')),
                         ('truncado.txt',b'fim \xf0\x9f\x98'),('overlong.txt',b'\xc0\xaf'),
                         ('surrogate.txt',b'\xed\xa0\x80')]:
        (root/'books'/name).write_bytes(content)
        preview(args.binary,root,image,'--book','/books/'+name,success=False)
    # Only preview arguments are accepted without a device; malformed taps fail early.
    result=subprocess.run(prefix+[str(args.binary.resolve()),'--preview','--tap','-1,10'],capture_output=True)
    assert result.returncode == 2
print('UI checks passed: TXT limits, accents, empty file, touch, resume, changed-book reset'+
      (', identical ARM/host pixels' if args.compare_host else ''))
