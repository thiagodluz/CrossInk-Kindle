#!/usr/bin/env python3
"""Build and audit the standalone PW1 hardware-validation package (not the reader)."""

import argparse
import hashlib
import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TARGET = "arm-kindle5-linux-gnueabi"


def run(*args: str) -> str:
    result = subprocess.run(args, cwd=REPO, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        print(result.stdout, file=sys.stderr, end="")
        raise SystemExit(f"Command failed ({result.returncode}): {args[0]}")
    return result.stdout


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", required=True, type=Path, help="Extracted arm-kindle5-linux-gnueabi root")
    parser.add_argument("--output", type=Path, default=REPO / "build/kindle-pw1")
    args = parser.parse_args()
    tools = args.toolchain.resolve() / "bin"
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = tools / f"{TARGET}-g++"
    if run(str(compiler), "-dumpmachine").strip() != TARGET:
        raise SystemExit("Wrong toolchain: PW1 requires the kindle5 EABI target")
    binary = output / "crossink-pw1"
    flags = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-fno-exceptions", "-fno-rtti",
             "-mcpu=cortex-a8", "-mfpu=neon", "-mfloat-abi=softfp", "-static", "-pthread", "-D_FILE_OFFSET_BITS=64",
             "-Wl,--hash-style=both", "-Wl,--build-id=sha1", "-Wl,-z,relro"]
    sources = ["platform/kindle/KindleBackends.cpp", "platform/kindle/Diagnostic.cpp", "platform/kindle/KindleSystem.cpp", "platform/kindle/LauncherSession.cpp"]
    objects_dir = output / "diagnostic-objects"
    objects_dir.mkdir(exist_ok=True)
    objects = []
    for source in sources:
        target = objects_dir / (Path(source).stem + ".o")
        run(str(compiler), *flags, "-c", source, "-o", str(target))
        objects.append(str(target))
    print(run(str(compiler), *flags, *objects, "-lrt", "-o", str(binary)), end="")
    print(run(str(tools / f"{TARGET}-strip"), "--strip-unneeded", str(binary)), end="")
    readelf = str(tools / f"{TARGET}-readelf")
    header = run(readelf, "-h", str(binary))
    attributes = run(readelf, "-A", str(binary))
    program = run(readelf, "-l", str(binary))
    dynamic = run(readelf, "-d", str(binary))
    versions = run(readelf, "--version-info", str(binary))
    notes = run(readelf, "-n", str(binary))
    needed = re.findall(r"Shared library: \[(.*?)\]", dynamic)
    glibc = sorted({m for m in re.findall(r"GLIBC_(\d+\.\d+(?:\.\d+)?)", versions)},
                   key=lambda v: tuple(int(n) for n in v.split(".")))
    if "ELF32" not in header or "ARM" not in header or "little endian" not in header:
        raise SystemExit("Wrong ELF architecture")
    if "INTERP" in program or "soft-float ABI" not in header or "Tag_ABI_VFP_args: VFP registers" in attributes:
        raise SystemExit("The standalone diagnostic requires static linking and the base ARM calling convention")
    kernel_match = re.search(r"OS: Linux, ABI: (\d+\.\d+\.\d+)", notes)
    if not kernel_match or tuple(int(n) for n in kernel_match[1].split(".")) > (2, 6, 31):
        raise SystemExit("Missing or incompatible minimum Linux ABI tag")
    if any(tuple(int(n) for n in v.split(".")) > (2, 12) for v in glibc):
        raise SystemExit(f"Requires a newer glibc than PW1: {glibc}")
    if needed:
        raise SystemExit(f"Standalone diagnostic has external runtime dependencies: {needed}")
    audit = "\n".join([header, attributes, program, dynamic, versions, notes])
    (output / "elf-audit.txt").write_text(audit)
    # A diagnostic-only archive must not advertise an absent UI binary.
    menu = json.loads((REPO / "kual/crossink/menu.json").read_text())
    menu["items"][0]["items"] = [item for item in menu["items"][0]["items"] if "--ui" not in item.get("params", "")]
    (output / "diagnostic-menu.json").write_text(json.dumps(menu, indent=2) + "\n")
    config = re.sub(r"(<version>).*?(</version>)", r"\g<1>0.1.0\2",
                    (REPO / "kual/crossink/config.xml").read_text())
    (output / "diagnostic-config.xml").write_text(config)
    entries = {
        "crossink/bin/crossink-pw1": binary,
        "crossink/runner.sh": REPO / "platform/kindle/runner.sh",
        "crossink/collect-diagnostics.sh": REPO / "scripts/kindle/collect-diagnostics.sh",
        "extensions/crossink/config.xml": output / "diagnostic-config.xml",
        "extensions/crossink/menu.json": output / "diagnostic-menu.json",
        "extensions/crossink/start.sh": REPO / "kual/crossink/start.sh",
        "LEIA-ME.md": REPO / "platform/kindle/README.md",
        "crossink/elf-audit.txt": output / "elf-audit.txt",
        "crossink/LICENSE": REPO / "LICENSE",
    }
    source_paths = sources + ["platform/kindle/KindleBackends.h", "platform/kindle/DiagnosticStrings.h",
                             "platform/kindle/KindleSystem.h", "platform/kindle/LauncherSession.h",
                             "scripts/kindle/build-package.py", "platform/kindle/BackendChecks.cpp",
                             "platform/kindle/check-launcher.py"]
    manifest = {
        "package": "CrossInk PW1 hardware validation", "version": "0.1.0", "is_full_reader": False,
        "source_commit": run("git", "rev-parse", "HEAD").strip(),
        "source_dirty": bool(run("git", "status", "--porcelain").strip()),
        "toolchain_target": TARGET, "compiler": run(str(compiler), "--version").splitlines()[0],
        "toolchain_release": "https://github.com/koreader/koxtoolchain/releases/tag/2026.08",
        "flags": flags, "needed": needed, "glibc_versions": glibc,
        "linkage": "static", "minimum_kernel": kernel_match[1], "toolchain_libc": "glibc 2.12.2",
        "files_sha256": {name: digest(path) for name, path in entries.items()},
        "sources_sha256": {name: digest(REPO / name) for name in source_paths},
    }
    (output / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive = output / "CrossInk-PW1-KUAL-validacao-0.1.0.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for name, source in entries.items():
            info = zipfile.ZipInfo(name, (2026, 10, 14, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100755 if name.endswith(".sh") or name.endswith("/crossink-pw1") else 0o100644) << 16
            package.writestr(info, source.read_bytes())
        package.writestr("crossink/build-manifest.json", json.dumps(manifest, indent=2) + "\n")
    print(f"Built {archive}\nStatic ELF dependencies={needed}; minimum Linux ABI={kernel_match[1]}")


if __name__ == "__main__":
    main()
