# Relinking the PW1 binaries

These application object files allow relinking the statically linked preview
and diagnostic against a compatible modified C/C++ runtime. Copy this directory
to a Linux build machine; it is not needed to launch the app on the Kindle.

```sh
python3 relink.py --toolchain /path/arm-kindle5-linux-gnueabi --profile ui --output crossink-ui
python3 relink.py --toolchain /path/arm-kindle5-linux-gnueabi --profile diagnostic --output crossink-pw1
```

The toolchain is koxtoolchain kindle5 release 2026.08:
https://github.com/koreader/koxtoolchain/releases/tag/2026.08
It uses GCC 14.4.0 and glibc 2.12.2, targeting Linux 2.6.31 and ARMv7 Cortex-A8
with the base ARM calling convention. Replace the sysroot/runtime libraries with
compatible modified ones before relinking; preserve the target ABI and kernel
requirements. Audit the replacement with readelf before installing.

The application source commit, SDK commit, source hashes, flags and compiler
are recorded in `ui-build.json` and `diagnostic-build.json`. App source:
https://github.com/thiagodluz/CrossInk-Kindle/tree/kindle-pw1
SDK source: https://github.com/Free-Ink/freeink-sdk
The only copied simulator code is WString.h, identified and licensed in the
compatibility directory. Font and runtime license texts are in `../licenses`.
glibc source: https://ftp.gnu.org/gnu/glibc/glibc-2.12.2.tar.bz2
GCC source: https://ftp.gnu.org/gnu/gcc/gcc-14.4.0/gcc-14.4.0.tar.xz

The GCC Runtime Library Exception permits distributing the linked runtime under
its terms. glibc is supplied under its LGPL terms; the supplied objects are for
relinking the application with a modified library. No KOReader, FBInk or ESP32
firmware runtime is linked into these binaries.
