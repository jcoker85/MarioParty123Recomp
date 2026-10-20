/*
 * MSVC CRT helpers referenced by rt64's prebuilt freetype.lib (compiled with /GS and stack
 * probes). The mingw CRT has no equivalents under these names, so provide them here.
 */

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

uintptr_t __security_cookie = 0x00002B992DDFA232ULL;

void __security_check_cookie(uintptr_t cookie) {
    if (cookie != __security_cookie) {
        abort();
    }
}

void __report_rangecheckfailure(void) {
    abort();
}

EXCEPTION_DISPOSITION __GSHandlerCheck(PEXCEPTION_RECORD record, void* frame, PCONTEXT context, void* dispatcher) {
    (void)record; (void)frame; (void)context; (void)dispatcher;
    return ExceptionContinueSearch;
}

/* MSVC's x64 __chkstk and mingw's ___chkstk_ms share a contract: probe RAX bytes of stack,
 * leave RSP unchanged, clobber only R10/R11. Forward one to the other. */
__asm__(
    ".globl __chkstk\n"
    "__chkstk:\n"
    "    jmp ___chkstk_ms\n"
);
