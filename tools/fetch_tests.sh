#!/usr/bin/env bash
# Downloads the published tests and model results into third_party/ (not committed):
#   - the litmus tests and the ARMv8-axiomatic results published with Pulte et al. (POPL 2018)
#     https://www.cl.cam.ac.uk/~pes20/armv8-mca/
#   - herdtools7's AArch64 base catalogue (tests and kinds.txt, CeCILL-B)
#     https://github.com/herd/herdtools7/tree/master/catalogue/aarch64/tests
set -euo pipefail
cd "$(dirname "$0")/.."
TP=third_party
mkdir -p "$TP/paper-tests" "$TP/herd-catalogue" "$TP/paper-results"

PAPER=https://www.cl.cam.ac.uk/~pes20/armv8-mca
for t in IRIW+addrs IRIW+poaas+LL IRIW+poaps+LL IRIW LB+data+data-wsi LB 'MP+dmb.sy+addr-%5bws-rf%5d-addr' \
         PPOCA WRC+addrs WW+RR+WW+RR+wsilp+poaa+wsilp+poaa; do
  curl -sSL --fail -o "$TP/paper-tests/$(python3 -c 'import sys,urllib.parse;print(urllib.parse.unquote(sys.argv[1]))' "$t").litmus" \
       "$PAPER/litmus-tests/$t.litmus"
done
curl -sSL --fail -o "$TP/ARMv8-axiomatic-non-mixed-size.tgz" "$PAPER/ARMv8-axiomatic-non-mixed-size.tgz"
tar -xzf "$TP/ARMv8-axiomatic-non-mixed-size.tgz" -C "$TP/paper-results"

HERD=https://raw.githubusercontent.com/herd/herdtools7/master/catalogue/aarch64/tests
for f in $(curl -sSL https://api.github.com/repos/herd/herdtools7/contents/catalogue/aarch64/tests \
           | python3 -c 'import sys,json;[print(e["name"]) for e in json.load(sys.stdin) if e["name"].endswith((".litmus","kinds.txt"))]'); do
  curl -sSL --fail -o "$TP/herd-catalogue/$f" "$HERD/$f"
done
echo "paper tests: $(ls "$TP/paper-tests" | wc -l), catalogue: $(ls "$TP/herd-catalogue" | wc -l), model logs: $(find "$TP/paper-results" -name '*.model_log' | wc -l)"
