/*
 * Wraps the toolchain's own <d3d12.h> for llvm-mingw builds. Its WIDL-generated header lacks
 * D3D12_HEAP_TYPE_GPU_UPLOAD (value 5 in the Windows SDK), which D3D12MemoryAllocator (via
 * rt64/plume) uses. CMakeLists.txt puts this directory on the system include path for MinGW only.
 */
#ifndef MP_D3D12_SHIM_H
#define MP_D3D12_SHIM_H

#include_next <d3d12.h>

#ifndef D3D12_HEAP_TYPE_GPU_UPLOAD
#define D3D12_HEAP_TYPE_GPU_UPLOAD ((D3D12_HEAP_TYPE)5)
#endif

#endif /* MP_D3D12_SHIM_H */
