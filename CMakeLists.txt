cmake_minimum_required(VERSION 3.20)

# Mario Party 1, 2 and 3 as three executables built against one copy of the runtime
# (N64ModernRuntime, rt64, RecompFrontend). All three land in the build directory together.
project(MarioParty123Recomp LANGUAGES C CXX)
if (WIN32)
    enable_language(RC)     # exe icons (icons/*.rc)
endif()
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (WIN32)
    # Opt out of constexpr mutex constructor on windows to prevent vcredist issues
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
    add_compile_definitions(NOMINMAX)
endif()

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(RT64_DIR ${CMAKE_SOURCE_DIR}/lib/rt64)
set(NMR_DIR ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)

# MinGW (llvm-mingw) builds: the toolchain's WIDL d3d12.h lacks D3D12_HEAP_TYPE_GPU_UPLOAD and
# there is no <dxguids.h>; tools/dx12shim provides both. MSVC/clang-cl builds don't need it.
if (MINGW)
    include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/tools/dx12shim")
    # ultramodern/ultra64.h defines NULL as an N64 pointer type when NULL isn't defined yet.
    # MSVC headers always define it first; llvm-mingw's may not, so make the standard one win.
    string(APPEND CMAKE_CXX_FLAGS " -include stddef.h")
    # Target Windows 10 so headers declare APIs such as SetProcessDpiAwareness.
    add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000000)
endif()

set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
add_compile_definitions(HLSL_CPU)
add_subdirectory(${RT64_DIR} ${CMAKE_BINARY_DIR}/rt64)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${NMR_DIR})

target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

if (MINGW)
    # rt64 uses the MSVC-only _declspec spelling, and clang ignores __declspec(uuid) on the DXC
    # interfaces, so the __mingw_uuidof<T>() definitions come from a shim.
    # Also run tools/apply_submodule_patches.sh once after checking out submodules.
    target_compile_definitions(rt64 PRIVATE _declspec=__declspec)
    target_sources(rt64 PRIVATE ${CMAKE_SOURCE_DIR}/common/mingw/dxc_uuids_shim.c)
endif()

# rt64 sets DXC options with plain set() (directory scope), but RecompFrontend compiles
# shaders from its own directory. Define them here at top level for it to inherit.
set(DXC_COMMON_OPTS "-I${RT64_DIR}/src")
set(DXC_DXIL_OPTS "-Wno-ignored-attributes")
set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set(DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set(DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
set(DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set(DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
if (WIN32)
    set(DXC "${RT64_DIR}/src/contrib/dxc/bin/x64/dxc.exe")
else()
    set(DXC "LD_LIBRARY_PATH=${RT64_DIR}/src/contrib/dxc/lib/x64" "${RT64_DIR}/src/contrib/dxc/bin/x64/dxc-linux")
endif()

# RecompFrontend reads SDL2 headers from ${sdl2_SOURCE_DIR}/include on Windows;
# point it at the SDL2 that rt64 vendors.
if (WIN32)
    set(sdl2_SOURCE_DIR "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3")
endif()

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH ${NMR_DIR})
set(RECOMP_FRONTEND_RT64_PATH ${RT64_DIR})
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/RecompFrontend)

set(MP_SDL2_DIR "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3")

# Shared code compiled once per game (it reads mp::game, which each game defines).
set(MP_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/common/src/main.cpp
    ${CMAKE_SOURCE_DIR}/common/src/config.cpp
    ${CMAKE_SOURCE_DIR}/common/src/dl_dump.cpp
    ${CMAKE_SOURCE_DIR}/common/src/test_input.cpp
)

# add_mp_game(<id> <exe name> <audio ucode function> [extra game sources...])
# Expects games/<id>/RecompiledFuncs/ and games/<id>/rsp/<ucode>.cpp from tools/generate.sh <id>.
function(add_mp_game id exe ucode)
    set(game_dir ${CMAKE_SOURCE_DIR}/games/${id})

    # The N64Recomp output for this game.
    set(funcs RecompiledFuncs_${id})
    add_library(${funcs} STATIC)
    target_compile_options(${funcs} PRIVATE
        -fno-strict-aliasing
        -Wno-unused-variable
        -Wno-implicit-function-declaration
    )
    target_include_directories(${funcs} PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
        ${NMR_DIR}/ultramodern/include
        ${NMR_DIR}/librecomp/include
        ${NMR_DIR}/N64Recomp/include
    )
    file(GLOB func_c_sources ${game_dir}/RecompiledFuncs/*.c)
    file(GLOB func_cxx_sources ${game_dir}/RecompiledFuncs/*.cpp)
    if (NOT func_c_sources)
        message(FATAL_ERROR "games/${id}/RecompiledFuncs/ is empty: run tools/build_decomp.sh ${id} and tools/generate.sh ${id} first")
    endif()
    target_sources(${funcs} PRIVATE ${func_c_sources} ${func_cxx_sources})

    add_executable(${exe})
    target_sources(${exe} PRIVATE
        ${MP_COMMON_SOURCES}
        ${game_dir}/src/game_info.cpp
        ${game_dir}/src/register_overlays.cpp
        ${game_dir}/src/process_scheduler.cpp
        ${game_dir}/src/overlay_loader.cpp
        ${game_dir}/src/unknown_symbols.cpp
        ${ARGN}
        # Generated by tools/bin/RSPRecomp games/<id>/<ucode>.toml
        ${game_dir}/rsp/${ucode}.cpp
    )

    target_include_directories(${exe} PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
        ${NMR_DIR}/N64Recomp/include
        ${NMR_DIR}/thirdparty/sse2neon
        ${RT64_DIR}/src/contrib
        ${RT64_DIR}/src/contrib/hlslpp/include
        ${RT64_DIR}/src/contrib/dxc/inc
        ${RT64_DIR}/src
        ${RT64_DIR}/src/rhi
        ${RT64_DIR}/src/render
        ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompinput/include
        ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/include
        ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/src
        ${RT64_DIR}/src/contrib/nativefiledialog-extended/src/include
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        target_compile_options(${exe} PRIVATE -march=nehalem -fno-strict-aliasing -fms-extensions)
    else()
        target_compile_options(${exe} PRIVATE -fno-strict-aliasing -fms-extensions)
    endif()

    target_link_libraries(${exe} PRIVATE
        ${funcs}
        recompui
        recompinput
        librecomp
        ultramodern
        rt64
        nfd
    )

    if (WIN32)
        # Exe icon; SDL also uses the exe's first icon for the game window.
        target_sources(${exe} PRIVATE ${CMAKE_SOURCE_DIR}/icons/${id}.rc)
        target_include_directories(${exe} PRIVATE ${MP_SDL2_DIR}/include)
        target_link_directories(${exe} PRIVATE ${MP_SDL2_DIR}/lib/x64)
        target_link_libraries(${exe} PRIVATE SDL2 winmm)

        if (MINGW)
            # llvm-mingw links against import libs it ships; name the DirectX/system ones rt64 needs.
            target_link_libraries(${exe} PRIVATE d3d12 dxgi shcore d3dcompiler_47 dwmapi)
            # rt64's prebuilt freetype.lib embeds MSVC CRT /DEFAULTLIB directives; the mingw CRT
            # provides those symbols, so drop the requests.
            target_link_options(${exe} PRIVATE "-Wl,-Xlink=-nodefaultlib:libcmt" "-Wl,-Xlink=-nodefaultlib:oldnames")
            target_sources(${exe} PRIVATE ${CMAKE_SOURCE_DIR}/common/mingw/msvc_crt_shim.c)
        else()
            set_target_properties(${exe} PROPERTIES
                LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
                LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
                LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
            # Disable identical code folding, which breaks mod function patching.
            target_link_options(${exe} PRIVATE /OPT:NOICF)
        endif()

        add_dependencies(${exe} mp_runtime_files)
    endif()

    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_package(SDL2 REQUIRED)
        find_package(Freetype REQUIRED)
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        set(THREADS_PREFER_PTHREAD_FLAG TRUE)
        find_package(Threads REQUIRED)
        target_compile_definitions(${exe} PRIVATE "PLUME_SDL_VULKAN_ENABLED" "RT64_SDL_WINDOW_VULKAN")
        target_include_directories(${exe} PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(${exe} PRIVATE ${FREETYPE_LIBRARIES} SDL2::SDL2 "-latomic -static-libstdc++" ${CMAKE_DL_LIBS} Threads::Threads)
    endif()
endfunction()

# DLLs, launcher assets and the controller database, staged once next to the three exes.
if (WIN32)
    set(MP_STAGE_FILES
        "${MP_SDL2_DIR}/lib/x64/SDL2.dll"
        "${RT64_DIR}/src/contrib/dxc/bin/x64/dxil.dll"
        "${RT64_DIR}/src/contrib/dxc/bin/x64/dxcompiler.dll"
        "${CMAKE_SOURCE_DIR}/recompcontrollerdb.txt"
    )
    if (MINGW)
        # The libc++ runtime DLLs live next to the compiler and must ship next to the exes.
        get_filename_component(MP_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach (dll libc++.dll libunwind.dll libwinpthread-1.dll)
            if (EXISTS "${MP_TOOLCHAIN_BIN}/${dll}")
                list(APPEND MP_STAGE_FILES "${MP_TOOLCHAIN_BIN}/${dll}")
            endif()
        endforeach()
    endif()
    add_custom_target(mp_runtime_files
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MP_STAGE_FILES} ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
        # Copied into the shared settings folder on first run (common/src/config.cpp).
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/common/default-settings ${CMAKE_BINARY_DIR}/default-settings
    )
endif()

# Pass -DMP_GAMES=mp3 (or "mp1;mp2") to build a subset.
set(MP_GAMES "mp1;mp2;mp3" CACHE STRING "Games to build")

if ("mp1" IN_LIST MP_GAMES)
    # widescreen.cpp holds the (currently disabled) hook targets from mp1.us.toml.
    add_mp_game(mp1 MarioPartyRecompiled aspMain ${CMAKE_SOURCE_DIR}/games/mp1/src/widescreen.cpp)
endif()
if ("mp2" IN_LIST MP_GAMES)
    add_mp_game(mp2 MarioParty2Recompiled aspMain)
endif()
if ("mp3" IN_LIST MP_GAMES)
    add_mp_game(mp3 MarioParty3Recompiled n_aspMain)
endif()

# The hub menu (hub/hub.c): lists the games built above from games.txt and starts them with the
# build directory as their working directory. Columns: title|subtitle|exe|libretro box art name|id.
set(MP_TITLE_mp1 "Mario Party")
set(MP_TITLE_mp2 "Mario Party 2")
set(MP_TITLE_mp3 "Mario Party 3")
set(MP_SUB_mp1 "1998  -  Hudson Soft")
set(MP_SUB_mp2 "1999  -  Hudson Soft")
set(MP_SUB_mp3 "2000  -  Hudson Soft")
set(MP_EXE_mp1 MarioPartyRecompiled)
set(MP_EXE_mp2 MarioParty2Recompiled)
set(MP_EXE_mp3 MarioParty3Recompiled)

set(_mp_games_txt "# Title|Subtitle|executable relative to this file|cover name|id\n")
foreach (g IN LISTS MP_GAMES)
    string(APPEND _mp_games_txt "${MP_TITLE_${g}}|${MP_SUB_${g}}|${MP_EXE_${g}}.exe|${MP_TITLE_${g}} (USA)|${g}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/games.txt" "${_mp_games_txt}")

if (WIN32)
    # icons/*.ico are drawn by tools/make_icons.ps1.
    add_executable(MarioParty123Recomp WIN32 ${CMAKE_SOURCE_DIR}/hub/hub.c ${CMAKE_SOURCE_DIR}/icons/hub.rc)
    target_compile_definitions(MarioParty123Recomp PRIVATE UNICODE _UNICODE)
    target_link_options(MarioParty123Recomp PRIVATE -municode -static)
    target_link_libraries(MarioParty123Recomp PRIVATE gdiplus msimg32 urlmon shell32 ole32 uuid)
    # Covers are not committed: the hub downloads missing ones into <build>/covers itself.
endif()
