/*
 * dxc_uuids_shim.c
 *
 * Provides the __mingw_uuidof<T>() symbols that clang emits for __uuidof(T)
 * on the DXC interfaces (dxcapi.h declares them via
 * `struct __declspec(uuid(...)) interface;`, but this clang build ignores the
 * uuid attribute: "unknown attribute 'uuid' ignored").
 *
 * The toolchain's guiddef.h declares the primary template
 *   template <typename T> const GUID &__mingw_uuidof();
 * but no definition exists for these types, and an explicit specialization in
 * a separate TU is never emitted (clang constant-folds the constexpr call),
 * so the consumer TU (rt64_shader_compiler.cpp) is left with an undefined
 * reference. Defining the exact mangled symbol as plain C guarantees a strong
 * definition in this object.
 *
 * The symbol names were taken verbatim from the undefined references in
 * rt64_shader_compiler.cpp.obj (llvm-nm).
 *
 * GUID values are copied from the CROSS_PLATFORM_UUIDOF declarations in
 * dxcapi.h (interface, spec).
 */

#include <guiddef.h>

/* IDxcCompiler: 8c210bf3-011f-4422-8d70-6f9acb8db617 */
static const GUID dxc_iid_IDxcCompiler = {
    0x8c210bf3u, 0x011fu, 0x4422u,
    { 0x8d, 0x70, 0x6f, 0x9a, 0xcb, 0x8d, 0xb6, 0x17 }
};

/* IDxcUtils: 4605C4CB-2019-492A-ADA4-65F20BB7D67F */
static const GUID dxc_iid_IDxcUtils = {
    0x4605c4cbu, 0x2019u, 0x492au,
    { 0xad, 0xa4, 0x65, 0xf2, 0x0b, 0xb7, 0xd6, 0x7f }
};

/* IDxcLinker: F1B5BE2A-62DD-4327-A1C2-42AC1E1E78E6 */
static const GUID dxc_iid_IDxcLinker = {
    0xf1b5be2au, 0x62ddu, 0x4327u,
    { 0xa1, 0xc2, 0x42, 0xac, 0x1e, 0x1e, 0x78, 0xe6 }
};

/* C linkage preserves the names verbatim, producing exactly the symbols the
 * C++ __uuidof(T) lowering references (const GUID & return == const GUID *
 * in rax/ecx per the x64 ABI). */

const GUID * _Z14__mingw_uuidofI12IDxcCompilerERK5_GUIDv(void) {
    return (const GUID *)&dxc_iid_IDxcCompiler;
}

const GUID * _Z14__mingw_uuidofI9IDxcUtilsERK5_GUIDv(void) {
    return (const GUID *)&dxc_iid_IDxcUtils;
}

const GUID * _Z14__mingw_uuidofI10IDxcLinkerERK5_GUIDv(void) {
    return (const GUID *)&dxc_iid_IDxcLinker;
}
