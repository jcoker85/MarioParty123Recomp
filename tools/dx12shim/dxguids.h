/*
 * Minimal local stand-in for <dxguids.h>.
 *
 * The MinGW branch of D3D12MemoryAllocator (src/D3D12MemAlloc.cpp, see its
 * issue #71) does:
 *
 *      #ifndef _MSC_VER
 *          #include <guiddef.h>
 *          #include <dxguids.h>   // "guiddef.h must be included first"
 *      #endif
 *
 * Neither the retcomm MinGW toolchain nor the D3D12 Agility SDK package
 * ships that header. This build does not reference any of the GUID
 * constants it normally defines: all D3D12 interfaces are declared with
 * MIDL_INTERFACE (so __uuidof() supplies the GUIDs), and the only literal
 * GUID constants used (IID_NULL, IID_IUnknown) come from guiddef.h/unknwn.h.
 * The file therefore only needs to exist and be self-consistent.
 */
#ifndef MP3_DXGUIDS_SHIM_H
#define MP3_DXGUIDS_SHIM_H

#include <guiddef.h>  /* GUID / IID / IID_NULL (idempotent via its own guard) */

#endif /* MP3_DXGUIDS_SHIM_H */
