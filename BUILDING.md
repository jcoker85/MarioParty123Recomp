# Building MarioParty123Recomp

## Requirements

- US ROMs, copied to `games/mp1/mp1.us.z64`, `games/mp2/mp2.us.z64` and `games/mp3/mp3.us.z64`
  (sha1s in README.md). You can build a subset of the games; see step 4.
- Docker (builds the decomps, which only build on Linux/macOS).
- Windows: an llvm-mingw toolchain with CMake and Ninja (developed with the `cmake-clang-v1` pack:
  llvm-mingw 20260616, CMake 3.31, Ninja 1.13). Put its `bin` on `PATH`.
- Git Bash (or any bash) for the scripts in `tools/`.

## Steps

```bash
git clone --recurse-submodules <this repo>
# or, in an existing clone:
git submodule update --init --recursive
bash tools/apply_submodule_patches.sh
```

Clone to a short path (such as `C:\src\MarioParty123Recomp`) or enable long paths first
(`git config --global core.longpaths true`). The nested submodules' git data sits deep under
`.git/modules/`, and past Windows' 260-character limit their clones fail with "Filename too long".
That leaves submodules missing or at the wrong commit, and the build later fails with missing
`CMakeLists.txt` files. `git submodule status --recursive` should show no lines starting with `-`
or `+`.

1. Build each decomp ELF (runs in Docker, outputs to `games/<id>/decomp-build/`, and checks the
   build matches the ROM). The MP3 image downloads the decomp's KMC GCC 2.7.2 toolchain from GitHub:

   ```bash
   bash tools/build_decomp.sh mp1
   bash tools/build_decomp.sh mp2
   bash tools/build_decomp.sh mp3
   ```

2. Build N64Recomp and RSPRecomp from the commit the runtime pins, into `tools/bin/`. N64Recomp
   needs a large stack for these ELFs (it overflows the default 1MB and crashes silently), hence the
   linker flag:

   ```bash
   cmake -S lib/N64ModernRuntime/N64Recomp -B build-tools/n64recomp -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ "-DCMAKE_EXE_LINKER_FLAGS=-Wl,--stack,67108864"
   cmake --build build-tools/n64recomp --target N64RecompCLI RSPRecomp
   mkdir -p tools/bin && cp build-tools/n64recomp/N64Recomp.exe build-tools/n64recomp/RSPRecomp.exe tools/bin/
   ```

3. Generate the recompiled code (`games/<id>/RecompiledFuncs/`, `games/<id>/rsp/`). Set
   `TOOLCHAIN_BIN` to the toolchain's `bin` if the tools can't find its libc++ DLLs:

   ```bash
   bash tools/generate.sh all
   ```

4. Build. The runtime libraries are built once and linked into all three executables, which land
   together in `build/` with the shared DLLs and launcher assets. Add `-DMP_GAMES=mp3` (or
   `"mp1;mp2"`) to build only some of the games.

   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
   cmake --build build
   ```

5. Run `build/MarioParty123Recomp.exe` (the hub), or run a game executable with `build/` as the working directory (the launcher reads `assets/` and
   `default-settings/` from there) and pick the ROM in its launcher. Put an empty `portable.txt`
   in `build/` to keep settings and saves there instead of in `%LOCALAPPDATA%\MarioParty123Recomp`.

Generated code, decomp builds and ROMs are never committed.

## Testing

- `SDL_AUDIODRIVER=dummy` mutes a run.
- `--game <id>` skips the launcher once a ROM has been chosen.
- A test profile is a folder with `portable.txt`, `assets/`, `recompcontrollerdb.txt` and
  `default-settings/`, plus `<id>/<game_id>.z64` for each game it runs; run the exe with that
  folder as the working directory.
- A file named `input.cmd` in the working directory is played back on controller 1
  (`common/src/test_input.cpp`), e.g. `START 3`, `- 60` (wait 60 frames), `A 3`, `STICK x y n`.
- `dldump.request` in the working directory dumps the next 8 display lists to `dldump_<n>.txt`;
  `MP_FRAME_STATS=1` logs graphics tasks and their target framebuffers per displayed frame.
- Crash backtraces print RVAs; symbolize with
  `llvm-symbolizer --obj=build/<exe> 0x140000000+<rva>`.
