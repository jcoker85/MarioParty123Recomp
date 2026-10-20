#!/usr/bin/env bash
# Regenerates games/<id>/RecompiledFuncs/ (N64Recomp) and games/<id>/rsp/<ucode>.cpp (RSPRecomp).
# Requires games/<id>/decomp-build/<elf> (tools/build_decomp.sh <id>) and games/<id>/<id>.us.z64.
# Usage: bash tools/generate.sh <mp1|mp2|mp3|all>
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# The tools are built with llvm-mingw and need its libc++ DLLs; point TOOLCHAIN_BIN at its bin dir.
if [[ -n "${TOOLCHAIN_BIN:-}" ]]; then
    export PATH="$TOOLCHAIN_BIN:$PATH"
fi

generate() {
    local id="$1" elf ucode sha
    case "$id" in
        mp1) elf=marioparty.elf;  ucode=aspMain;   sha=1159bd56... ;;
        mp2) elf=marioparty2.elf; ucode=aspMain;   sha=166eda1c... ;;
        mp3) elf=marioparty3.elf; ucode=n_aspMain; sha=6beb80ff... ;;
        *) echo "unknown game id: $id (expected mp1, mp2, mp3 or all)" >&2; exit 1 ;;
    esac

    # Paths in the tomls are relative to games/<id>.
    cd "$REPO_ROOT/games/$id"
    [[ -f decomp-build/$elf ]] || { echo "missing games/$id/decomp-build/$elf; run tools/build_decomp.sh $id" >&2; exit 1; }
    [[ -f $id.us.z64 ]] || { echo "missing games/$id/$id.us.z64 (US ROM, sha1 $sha)" >&2; exit 1; }

    rm -rf RecompiledFuncs && mkdir RecompiledFuncs
    "$REPO_ROOT/tools/bin/N64Recomp" $id.us.toml > build-codegen.log 2>&1 || { tail -20 build-codegen.log; exit 1; }
    grep -E "Error|Warn" build-codegen.log && echo "(codegen warnings above, see games/$id/build-codegen.log)" || true

    mkdir -p rsp
    "$REPO_ROOT/tools/bin/RSPRecomp" $ucode.toml

    echo "$id: generated $(ls RecompiledFuncs | wc -l) files in RecompiledFuncs/ and rsp/$ucode.cpp"
}

if [[ "${1:-}" == "all" ]]; then
    for id in mp1 mp2 mp3; do generate "$id"; done
elif [[ -n "${1:-}" ]]; then
    generate "$1"
else
    echo "usage: tools/generate.sh <mp1|mp2|mp3|all>" >&2
    exit 1
fi
