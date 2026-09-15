# PW1 Diagnostics

This is the first executable artifact for the Kindle port. It collects a **read-only** environment report from a jailbroken first-generation Kindle Paperwhite (PW1).

The script does not modify Kindle settings, start a framebuffer update, capture touchscreen events, list books, read book contents, access Wi-Fi credentials, or reboot/suspend the device. Its only writes are the report files inside the output directory you choose.

## What it collects

- Kindle firmware, kernel, CPU, memory, executable format, libc, and C++ runtime;
- mounted filesystems, free space, and only the presence of known Kindle/KUAL/KOReader directories;
- framebuffer node names and read-only geometry/format metadata;
- touchscreen/input device metadata and, when available, `getevent -pl` capability listings;
- read-only battery, charging, front-light, power, and Kindle LIPC properties;
- available launch, logging, SSH, and relevant service-process indicators.

It intentionally does **not** read `/dev/fb*` or `/dev/input/event*`. That makes the first diagnostic safe even before we know the Kindle's panel and input ownership rules.

## Before running

1. Leave the Kindle at its normal home screen or in KOReader; no special mode is needed.
2. Use the established shell access on your jailbroken device (for example, SSH or KTerm).
3. Copy [`collect-diagnostics.sh`](../scripts/kindle/collect-diagnostics.sh) to the Kindle's user storage. Copying it to `/mnt/us/documents/` is convenient, but it is not required.
4. Do not run it as a firmware update, KUAL installer, or root-level system script.

## Run

From the Kindle shell, run the script through `sh`. Replace the path if you copied it somewhere else:

```sh
sh /mnt/us/documents/collect-diagnostics.sh /mnt/us/documents
```

The second path is only the parent folder for the output. The script creates a new folder named similar to:

```text
/mnt/us/documents/crossink-pw1-diagnostics-YYYYMMDD-HHMMSS/
```

The main file is `report.txt`. Copy that full output directory to your computer and attach or paste `report.txt` here. Do not send the Kindle serial number, Amazon account details, Wi-Fi credentials, or raw full-device dumps.

## Privacy

The script masks serial-like fields, UUID-like fields, and MAC addresses in the files it creates. This is deliberate but not a substitute for a quick review: open `report.txt` before sharing it and redact anything personal that remains.

## Expected result

A successful run ends with:

```text
Diagnostic report created: .../report.txt
```

Some entries may say “not available” or have a non-zero exit code. That is useful information and does not mean the script failed. Send the complete report as generated.

## Recovery

No recovery procedure is necessary: the script changes no Kindle setting and does not hold the display or touch devices. If it is interrupted, delete only its own `crossink-pw1-diagnostics-*` output directory from user storage.
