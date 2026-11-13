#!/usr/bin/env bash
# Runs mca-hw on a phone over adb, in several core configurations, and keeps every result.
#   hw/run_phone.sh [--smoke] [iterations-per-test]   (default 1000000; needs hw/build.sh first)
#   SYNC=barrier hw/run_phone.sh                      the looser per-instance barrier (default: time)
#
# Configurations, worked out from the phone's own CPU list:
#   unpinned          the scheduler decides
#   cluster-<MHz>     every test thread inside one cluster (cores of the same max clock)
#   cross             threads alternate between the fastest and slowest clusters
set -euo pipefail
cd "$(dirname "$0")/.."
ADB=${ADB:-adb}
command -v "$ADB" >/dev/null || ADB=/mnt/c/Users/loh/AppData/Local/Android/Sdk/platform-tools/adb.exe
SMOKE=0
if [ "${1:-}" = "--smoke" ]; then SMOKE=1; shift; fi
ITER=${1:-1000000}
SYNC=${SYNC:-time}
N=1000
R=$((ITER / N))

[ "$("$ADB" get-state 2>/dev/null | tr -d '\r')" = "device" ] || { echo "no phone: connect one with USB debugging on"; exit 1; }
MODEL=$("$ADB" shell getprop ro.product.model | tr -d '\r' | tr ' ' '_')
SOC=$("$ADB" shell getprop ro.soc.model | tr -d '\r')
OUT=results/hw/$MODEL
mkdir -p "$OUT"

"$ADB" push hw/build/mca-hw /data/local/tmp/mca-hw >/dev/null
"$ADB" shell chmod 755 /data/local/tmp/mca-hw
"$ADB" shell /data/local/tmp/mca-hw --info | tr -d '\r' > "$OUT/info.json"

# --smoke: the controls alone, quickly, before trusting a long run. SB's relaxed state should
# appear; CoRR's and SB+dmb.sys's must not.
if [ $SMOKE = 1 ]; then
  cat "$OUT/info.json"
  "$ADB" shell /data/local/tmp/mca-hw -n 1000 -r 100 SB CoRR SB+dmb.sys < /dev/null | tr -d '\r' > "$OUT/smoke.out"
  python3 -c '
import json, sys
for l in open(sys.argv[1]):
    r = json.loads(l)
    print("%-11s model %-9s relaxed state seen %6d / %d   forbidden states %d" % (
        r["test"], r.get("model", "?"), r.get("condition_seen", 0), r.get("iterations", 0), r.get("forbidden_seen", 0)))
' "$OUT/smoke.out"
  exit 0
fi
"$ADB" shell dumpsys thermalservice 2>/dev/null | tr -d '\r' | grep -m1 "Thermal Status" > "$OUT/thermal_before.txt" || true
echo "$MODEL ($SOC): $(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(len(d["cpus"]), "CPUs, LSE", d["lse"], ", LRCPC", d["lrcpc"])' "$OUT/info.json")"

python3 hw/configs.py "$OUT/info.json" | while read -r name cpus; do
  echo "== $SYNC $name ${cpus:-}"
  pin=""; [ -n "${cpus:-}" ] && pin="-a $cpus"
  "$ADB" shell /data/local/tmp/mca-hw -s $SYNC -n $N -r $R $pin < /dev/null | tr -d '\r' > "$OUT/${SYNC}__$name.jsonl"
done
"$ADB" shell dumpsys thermalservice 2>/dev/null | tr -d '\r' | grep -m1 "Thermal Status" > "$OUT/thermal_after.txt" || true
python3 hw/compare.py results/hw
