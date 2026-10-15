#!/usr/bin/env python3
"""Compile the native PW1 renderer and UI, using the repository's pinned SDK."""
import argparse
import concurrent.futures
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CPP=[
    "platform/kindle/KindleBackends.cpp", "platform/kindle/KindlePower.cpp", "platform/kindle/KindleSystem.cpp", "platform/kindle/NativeCompat.cpp",
    "platform/kindle/NativeDisplay.cpp", "platform/kindle/NativeStorage.cpp",
    "lib/GfxRenderer/GfxRenderer.cpp", "lib/GfxRenderer/Bitmap.cpp",
    "lib/GfxRenderer/BitmapHelpers.cpp", "lib/GfxRenderer/FontCacheManager.cpp",
    "lib/EpdFont/EpdFont.cpp", "lib/EpdFont/EpdFontFamily.cpp",
    "lib/EpdFont/SdCardFont.cpp", "lib/EpdFont/FontDecompressor.cpp",
    "lib/Utf8/Utf8.cpp", "lib/MiniBidi/BidiUtils.cpp",
    "lib/Memory/BuildScratch.cpp", "lib/InflateReader/InflateReader.cpp",
    "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp",
    "lib/Txt/Txt.cpp", "lib/FsHelpers/FsHelpers.cpp", "platform/kindle/RenderWorker.cpp",
]
C=["lib/MiniBidi/minibidi.c", "lib/uzlib/src/tinflate.c", "lib/uzlib/src/uzlib_checksums.c"]
INCLUDES=["platform/kindle/compat", "platform/kindle", "lib/GfxRenderer", "lib/EpdFont",
          "lib/Memory", "lib/MemoryBudget", "lib/Utf8", "lib/MiniBidi", "lib/InflateReader", "lib/uzlib/src",
          "freeink-sdk/libs/ui/FreeInkUI/include", "freeink-sdk/libs/display/FreeInkDisplay/include",
          "lib/Txt", "lib/FsHelpers", "lib/JpegToBmpConverter"]

def command(args):
    result=subprocess.run([str(a) for a in args],cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return result.stdout

def build(toolchain,output,main=None,host=False):
    if not (ROOT/"freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp").is_file():
        raise RuntimeError("Initialize the pinned SDK: git submodule update --init freeink-sdk")
    output.mkdir(parents=True,exist_ok=True)
    obj=output/('host-objects' if host else 'arm-objects');obj.mkdir(exist_ok=True)
    triple="arm-kindle5-linux-gnueabi"
    cxx="g++" if host else toolchain/"bin"/(triple+"-g++")
    cc="gcc" if host else toolchain/"bin"/(triple+"-gcc")
    # Inherited generated font tables omit optional callback fields; C++ zero-initializes them.
    common=["-O2","-Wall","-Wextra","-Wno-missing-field-initializers","-ffunction-sections","-fdata-sections","-pthread",
            "-D_FILE_OFFSET_BITS=64","-DCROSSINK_PLATFORM_KINDLE=1","-DCROSSINK_SCALABLE_FONTS=0",
            "-DCROSSINK_APP_CAP_TOUCH=1","-DCROSSINK_APP_CAP_USB_DRIVE=0","-DFREEINK_CAP_USB_MSC=0",
            "-DXML_GE=0","-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES=1"]
    if not host: common += ["-mcpu=cortex-a8","-mfpu=neon","-mfloat-abi=softfp"]
    includes=["-I"+str(ROOT/p) for p in INCLUDES]
    i18n=output/"i18n";i18n.mkdir(exist_ok=True)
    command(["python3","scripts/gen_i18n.py","lib/I18n/translations",str(i18n),
             "--builtin-langs","EN,PT","--strip-unused","--src-dirs","platform/kindle/app"])
    for name in ("I18n.h","I18n.cpp"):shutil.copyfile(ROOT/"lib/I18n"/name,i18n/name)
    includes=["-I"+str(i18n)]+includes
    sources=[(ROOT/p,True) for p in CPP]+[(ROOT/p,False) for p in C]
    sources += [(i18n/"I18n.cpp",True),(i18n/"I18nStrings.cpp",True)]
    if main:sources.append((Path(main).resolve(),True))
    objects=[]
    def compile_one(item):
        path,cpp=item
        name=hashlib.sha256(str(path).encode()).hexdigest()[:16]+".o"
        target=obj/name
        flags=common+includes+(["-std=gnu++20","-fno-exceptions","-fno-rtti"] if cpp else ["-std=gnu99"])
        if path.is_relative_to(ROOT/"platform/kindle"):flags+=["-Werror"]
        # Headers affect these first port builds; compile explicitly each time.
        command([cxx if cpp else cc,*flags,"-c",path,"-o",target])
        return target
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        for target in pool.map(compile_one,sources):objects.append(target)
    if main:
        binary=output/("crossink-ui-host" if host else "crossink-ui")
        linkflags=["-pthread","-Wl,--gc-sections","-Wl,--build-id=sha1"]
        if not host:linkflags+=["-static","-mcpu=cortex-a8","-mfpu=neon","-mfloat-abi=softfp"]
        command([cxx,*objects,*linkflags,"-lrt","-o",binary])
        inputs = {}
        for directory in INCLUDES + ["lib/hal", "lib/I18n", "platform/kindle/app"]:
            for path in (ROOT/directory).rglob('*'):
                if path.is_file() and path.suffix in ('.h','.hpp','.c','.cpp','.yaml','.txt'):
                    inputs[str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
        for path, _ in sources:
            if path.is_relative_to(ROOT) and not path.is_relative_to(ROOT/'build'):
                inputs[str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
        for name in ('scripts/kindle/build-ui.py','scripts/gen_i18n.py'):
            inputs[name]=hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
        receipt={"source_commit":command(['git','rev-parse','HEAD']).strip(),
                 "source_dirty":bool(command(['git','status','--porcelain']).strip()),
                 "sdk_commit":command(['git','-C','freeink-sdk','rev-parse','HEAD']).strip(),
                 "compiler":command([cxx,'--version']).splitlines()[0],
                 "host":host,"common_flags":common,"link_flags":linkflags+['-lrt'],
                 "binary_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),
                 "sources_sha256":inputs,
                 "objects":[{"name":p.name,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in objects]}
        (output/('host-build.json' if host else 'arm-build.json')).write_text(json.dumps(receipt,indent=2)+'\n')
        print("Built",binary)
        return binary
    print("Compiled",len(objects),"native core objects")

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain",type=Path,default=ROOT.parent/"toolchains/x-tools/arm-kindle5-linux-gnueabi")
    parser.add_argument("--output",type=Path,default=ROOT/"build/kindle-pw1/ui")
    parser.add_argument("--main",type=Path,default=ROOT/'platform/kindle/app/PortApp.cpp')
    parser.add_argument("--host",action="store_true")
    args=parser.parse_args()
    try:build(args.toolchain.resolve(),args.output.resolve(),args.main,args.host)
    except RuntimeError as error:raise SystemExit(str(error))
