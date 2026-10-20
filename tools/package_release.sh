#!/usr/bin/env bash
# Stage a ready-to-run release from a finished build and zip it.
#
# The result is what a player downloads: the menu, the three game programs and the files they
# read from their folder. It holds no ROMs, saves, cover art, settings from this machine, sources
# or generated code. The player runs the menu, which downloads the cover art, and the first Play
# on each game opens its launcher to choose their own ROM.
#
# The game programs are copied with their debug info stripped (llvm-strip); the build directory
# keeps the unstripped ones, so crash RVAs from a release can be symbolized against that build.
#
# Usage (from the repository root, after `cmake --build build`, with the llvm-mingw bin on PATH):
#   tools/package_release.sh [--build-dir build] [--version 0.1.0]
# Output: dist/MarioParty123Recomp-<version>/ and the same name + .zip
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
BUILD=build
VERSION=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir) BUILD="${2:?}"; shift 2 ;;
    --version) VERSION="${2:?}"; shift 2 ;;
    *) echo "error: unknown argument $1" >&2; exit 1 ;;
  esac
done
[[ -n "$VERSION" ]] || VERSION="$(git describe --tags --always 2>/dev/null || echo dev)"
NAME="MarioParty123Recomp-$VERSION"
STAGE="dist/$NAME"
[[ -f "$BUILD/MarioParty123Recomp.exe" && -f "$BUILD/games.txt" ]] ||
  { echo "error: $BUILD has no built menu; run cmake --build $BUILD first" >&2; exit 1; }
command -v llvm-strip >/dev/null || { echo "error: llvm-strip not on PATH (it ships with llvm-mingw)" >&2; exit 1; }

rm -rf "$STAGE" "dist/$NAME.zip"
mkdir -p "$STAGE/licenses"
cp "$BUILD/MarioParty123Recomp.exe" "$BUILD/games.txt" "$BUILD/recompcontrollerdb.txt" "$STAGE/"
cp -r "$BUILD/assets" "$STAGE/assets"
# The shipped defaults, never this machine's own settings.
cp -r common/default-settings "$STAGE/default-settings"
for dll in SDL2.dll dxcompiler.dll dxil.dll libc++.dll libunwind.dll libwinpthread-1.dll; do
  [[ -f "$BUILD/$dll" ]] || { echo "error: $BUILD/$dll is missing" >&2; exit 1; }
  cp "$BUILD/$dll" "$STAGE/"
done

count=0
while IFS='|' read -r title _subtitle exe _cover _id; do
  [[ -z "${exe:-}" || "${title:0:1}" == "#" ]] && continue
  [[ -f "$BUILD/$exe" ]] || { echo "error: $BUILD/$exe is missing" >&2; exit 1; }
  llvm-strip --strip-debug -o "$STAGE/$exe" "$BUILD/$exe"
  count=$((count + 1))
done < "$BUILD/games.txt"
[[ $count -gt 0 ]] || { echo "error: $BUILD/games.txt lists no games" >&2; exit 1; }

# Licenses of this project and of everything linked into or shipped beside the programs.
cp LICENSE "$STAGE/licenses/MarioParty123Recomp-LICENSE.txt"
cp lib/N64ModernRuntime/COPYING "$STAGE/licenses/N64ModernRuntime-COPYING.txt"
cp lib/RecompFrontend/COPYING "$STAGE/licenses/RecompFrontend-COPYING.txt"
cp lib/N64ModernRuntime/N64Recomp/LICENSE "$STAGE/licenses/N64Recomp-LICENSE.txt"
cp lib/rt64/LICENSE "$STAGE/licenses/rt64-LICENSE.txt"
cp lib/rt64/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/COPYING.txt "$STAGE/licenses/SDL2-COPYING.txt"
cp lib/RecompFrontend/recompui/lib/RmlUi/LICENSE.txt "$STAGE/licenses/RmlUi-LICENSE.txt"
cp lib/RecompFrontend/recompui/lib/lunasvg/LICENSE "$STAGE/licenses/lunasvg-LICENSE.txt"
cp lib/RecompFrontend/recompui/lib/lunasvg/plutovg/LICENSE "$STAGE/licenses/plutovg-LICENSE.txt"
cp lib/RecompFrontend/recompui/lib/freetype-windows-binaries/LICENSE.TXT "$STAGE/licenses/freetype-LICENSE.txt"
TOOLCHAIN_ROOT="$(cd "$(dirname "$(command -v llvm-strip)")/.." && pwd)"
[[ -f "$TOOLCHAIN_ROOT/LICENSE.TXT" ]] && cp "$TOOLCHAIN_ROOT/LICENSE.TXT" "$STAGE/licenses/llvm-mingw-LICENSE.txt"
cat > "$STAGE/licenses/DirectXShaderCompiler.txt" <<'EOF'
dxcompiler.dll and dxil.dll are from Microsoft's DirectXShaderCompiler
(https://github.com/microsoft/DirectXShaderCompiler), redistributed as bundled by RT64.
DirectXShaderCompiler is released under the University of Illinois/NCSA Open Source License;
dxil.dll is redistributable under the DirectX Shader Compiler redistribution terms.
EOF

cat > "$STAGE/START_HERE.txt" <<EOF
MarioParty123Recomp $VERSION

Mario Party, Mario Party 2 and Mario Party 3 (N64, US) as native Windows
programs. Unofficial fan project, not affiliated with Nintendo or Hudson Soft.

You need your own ROM of each game you want to play (.z64, US version).
Nothing from the games is included here.

1. Run MarioParty123Recomp.exe. The box art downloads in the background the
   first time.
2. Press PLAY on a game. The first time, its launcher opens: choose your ROM
   there, then start the game. After that PLAY goes straight in.
3. SETTINGS opens a game's launcher at any time: ROM, display, audio,
   controls and mods. Display, audio and control settings are shared by all
   three games.

Settings and saves are kept in %LOCALAPPDATA%\\MarioParty123Recomp. To keep
them in this folder instead, create an empty file named portable.txt here.

The game programs contain a machine-translated build of each game's own code.
This program is free software under the GNU GPL v3 (licenses folder); its
source code is at https://github.com/jcoker85/MarioParty123Recomp
EOF

# Nothing personal or copyrighted may ride along.
bad="$(find "$STAGE" -type f \( -iname '*.z64' -o -iname '*.n64' -o -iname '*.v64' -o -iname '*.bin' -o -iname '*.bak' \
      -o -iname 'portable.txt' -o -iname 'input.cmd' -o -iname '*.log' -o -iname 'dldump*' -o -iname '*.c' -o -iname '*.cpp' \
      -o -path '*/covers/*' -o -path '*/saves/*' -o -path '*/mods/*' \) 2>/dev/null || true)"
[[ -z "$bad" ]] || { echo "error: the package would include files it must not:" >&2; echo "$bad" >&2; exit 1; }
json="$(find "$STAGE" -type f -iname '*.json' ! -path '*/default-settings/*' 2>/dev/null || true)"
[[ -z "$json" ]] || { echo "error: settings files outside default-settings/:" >&2; echo "$json" >&2; exit 1; }

STAGE_WIN="$(cygpath -w "$ROOT/$STAGE" 2>/dev/null || echo "$ROOT/$STAGE")"
ZIP_WIN="$(cygpath -w "$ROOT/dist/$NAME.zip" 2>/dev/null || echo "$ROOT/dist/$NAME.zip")"
powershell -NoProfile -Command "Compress-Archive -Path '$STAGE_WIN' -DestinationPath '$ZIP_WIN' -CompressionLevel Optimal"
echo "dist/$NAME.zip: $(( $(stat -c %s "dist/$NAME.zip") / 1048576 )) MB, $count game(s) staged in $STAGE"
