#!/usr/bin/env bash
# Builds lib/<id>-decomp inside Docker and copies the ELF/map/ROM to games/<id>/decomp-build/.
# Needs games/<id>/<id>.us.z64. Usage (from any shell with docker): bash tools/build_decomp.sh <mp1|mp2|mp3>
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
id="${1:-}"
case "$id" in
    mp1) name=marioparty;  kmc_gcc=no ;;
    mp2) name=marioparty2; kmc_gcc=no ;;
    mp3) name=marioparty3; kmc_gcc=yes ;;  # MP3 builds with KMC GCC 2.7.2, fetched into its image
    *) echo "usage: tools/build_decomp.sh <mp1|mp2|mp3>" >&2; exit 1 ;;
esac

IMAGE=${id}recomp-decomp
OUT="$REPO_ROOT/games/$id/decomp-build"

docker build -q -t "$IMAGE" -f "$REPO_ROOT/docker/$id.Dockerfile" "$REPO_ROOT" >/dev/null
mkdir -p "$OUT"

# Build from a fresh clone inside the container: avoids CRLF checkouts and the
# slow Windows bind mount. Only the outputs are copied back.
MSYS_NO_PATHCONV=1 docker run --rm \
    -v "$REPO_ROOT:/src:ro" \
    -v "$OUT:/out" \
    -e ID="$id" -e NAME="$name" -e KMC_GCC="$kmc_gcc" \
    "$IMAGE" bash -c '
        set -euo pipefail
        git clone -q /src/lib/$ID-decomp /work
        cd /work
        git checkout -q "$(git -C /src/lib/$ID-decomp rev-parse HEAD)"
        if [[ "$KMC_GCC" == yes ]]; then cp -r /opt/mp3tools/gcc_2.7.2 tools/; fi
        cp /src/games/$ID/$ID.us.z64 baserom.us.z64
        export PATH=/opt/venv/bin:$PATH
        PY=/opt/venv/bin/python3
        make PYTHON=$PY setup >/out/setup.log 2>&1 || { tail -40 /out/setup.log; exit 1; }
        make PYTHON=$PY -j"$(nproc)" >/out/build.log 2>&1 || { tail -40 /out/build.log; exit 1; }
        tail -3 /out/build.log
        cp build/$NAME.elf build/$NAME.map build/$NAME.z64 $NAME.ld /out/
    '
echo "decomp outputs in games/$id/decomp-build/"
