#!/usr/bin/env bash
# Applies tools/submodule-patches/<submodule>-*.patch to the matching lib/<submodule>.
# Idempotent: patches that are already applied are skipped. Needed for llvm-mingw builds only.
# Usage: bash tools/apply_submodule_patches.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

for patch in "$REPO_ROOT"/tools/submodule-patches/*.patch; do
    name="$(basename "$patch")"
    sub="${name%%-*}"
    dir="$REPO_ROOT/lib/$sub"
    if git -C "$dir" apply --reverse --check "$patch" 2>/dev/null; then
        echo "already applied: $name"
    else
        git -C "$dir" apply "$patch"
        echo "applied: $name"
    fi
done
