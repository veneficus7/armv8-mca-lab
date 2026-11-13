#!/usr/bin/env bash
# Builds mca-hw for Android (arm64) from hw/suite.txt. Run in WSL or Linux, from anywhere.
#   hw/build.sh                  the newest NDK in the Android SDK (see hw/sdk.sh)
#   NDK=/path/to/ndk hw/build.sh
set -euo pipefail
cd "$(dirname "$0")/.."
. hw/sdk.sh
NDK=${NDK:-$(ls -d "$(android_sdk)"/ndk/*/ 2>/dev/null | sort -V | tail -1)}
NDK=${NDK%/}
[ -d "$NDK/toolchains" ] || { echo "no Android NDK found: install one, or set NDK=/path/to/ndk"; exit 1; }
BIN="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin"
[ -x "$BIN/clang.exe" ] || BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
EXE=""; [ -x "$BIN/clang.exe" ] && EXE=".exe"

mkdir -p hw/build
make -C model -s
grep -v '^#' hw/suite.txt | grep -v '^$' | xargs ./model/mca-check --describe > hw/build/tests.jsonl
python3 hw/gen.py hw/build/tests.jsonl hw/build/tests_gen.c
# Static, so it runs from /data/local/tmp with nothing else installed. API 29 = Android 10.
"$BIN/clang$EXE" --target=aarch64-linux-android29 -O2 -static -Wall -Wextra -Ihw \
    -o hw/build/mca-hw hw/harness.c hw/build/tests_gen.c
"$BIN/llvm-objdump$EXE" -d --no-show-raw-insn hw/build/mca-hw > hw/build/mca-hw.dis
python3 hw/verify_disasm.py hw/build/tests.jsonl hw/build/mca-hw.dis
ls -la hw/build/mca-hw
