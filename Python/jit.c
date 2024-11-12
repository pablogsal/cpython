#ifdef _Py_JIT

#include "Python.h"

#include "pycore_abstract.h"
#include "pycore_bitutils.h"
#include "pycore_call.h"
#include "pycore_ceval.h"
#include "pycore_critical_section.h"
#include "pycore_dict.h"
#include "pycore_intrinsics.h"
#include "pycore_long.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_optimizer.h"
#include "pycore_pyerrors.h"
#include "pycore_setobject.h"
#include "pycore_sliceobject.h"
#include "pycore_jit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define X86_64_RBP 6    // Frame pointer
#define X86_64_RSP 7    // Stack pointer
#define X86_64_RIP 16   // Instruction pointer

 //

struct table_entry {
    int32_t start_ip_offset;
    int32_t fde_offset;
};

struct unw_debug_frame_list {
    unw_word_t start;             // Start of region (inclusive)
    unw_word_t end;               // End of region (exclusive)
    unw_word_t load_offset;       // ELF load offset
    char* debug_frame;            // The debug frame data
    size_t debug_frame_size;      // Size of debug frame data
    struct table_entry* index;    // Binary search index
    size_t index_size;            // Size of index array
    struct unw_debug_frame_list* next;  // Next in chain
};

typedef struct {
    unw_dyn_info_t* di;
    struct unw_debug_frame_list* debug_frame_list;
} unwind_registration_t;

// Write LEB128 encoded value, returns number of bytes written
static int write_uleb128(uint8_t* p, uint64_t value) {
    uint8_t* start = p;
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        *p++ = byte;
    } while (value != 0);
    return p - start;
}

static int write_sleb128(uint8_t* p, int64_t value) {
    uint8_t* start = p;
    int more;
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        more = !((value == 0 && (byte & 0x40) == 0) ||
                 (value == -1 && (byte & 0x40) != 0));
        if (more)
            byte |= 0x80;
        *p++ = byte;
    } while (more);
    return p - start;
}

static char* create_debug_frame(uintptr_t code_start, size_t code_size, size_t* size, off_t *fde_offset) {
    *size = 64;  // Initial size, will adjust based on actual content
    char* data = malloc(*size);
    if (!data) return NULL;

    unsigned char* p = (unsigned char*)data;
    unsigned char* cie_start = p;

    // Leave space for CIE length
    p += 4;

    // CIE ID
    *(uint32_t*)p = 0xffffffff;
    p += 4;

    // Version: should be 1
    *p++ = 1;

    // Augmentation string: empty string
    *p++ = 0;

    // Code alignment factor (encoded as ULEB128)
    p += write_uleb128(p, 1);

    // Data alignment factor (encoded as SLEB128)
    p += write_sleb128(p, -8);

    // Return address register
    p += write_uleb128(p, 16);  // RAX on x86_64

    // Initial instructions
    *p++ = 0x0c;  // DW_CFA_def_cfa
    p += write_uleb128(p, X86_64_RSP);   // RSP
    p += write_uleb128(p, 16);   // Offset


    // Calculate and write CIE length (not including length field itself)
    *(uint32_t*)cie_start = p - cie_start - 4;

    // Write FDE
    *fde_offset = p - (unsigned char*)data;
    unsigned char* fde_start = p;

    // Leave space for FDE length
    p += 4;

    // CIE pointer
    *(uint32_t*)p = 0;
    p += 4;

    // Initial location
    *(uint64_t*)p = 0;
    p += 8;

    // Address range
    *(uint64_t*)p = code_size;
    p += 8;

    // Write FDE length (not including length field itself)
    *(uint32_t*)fde_start = p - fde_start - 4;

    *size = p - (unsigned char*)data;
    return data;
}

static unwind_registration_t* register_jit_unwind(void* code_start, size_t code_size) {
    unwind_registration_t* reg = malloc(sizeof(unwind_registration_t));
    if (!reg) return NULL;
    memset(reg, 0, sizeof(*reg));

    // Create debug frame list
    reg->debug_frame_list = malloc(sizeof(struct unw_debug_frame_list));
    if (!reg->debug_frame_list) {
        free(reg);
        return NULL;
    }
    memset(reg->debug_frame_list, 0, sizeof(struct unw_debug_frame_list));

    // Set up debug frame list
    reg->debug_frame_list->start = (unw_word_t)code_start;
    reg->debug_frame_list->end = (unw_word_t)code_start + code_size;
    off_t fde_offset = 0;
    reg->debug_frame_list->debug_frame = create_debug_frame((uintptr_t)code_start, code_size, &reg->debug_frame_list->debug_frame_size, &fde_offset);
    reg->debug_frame_list->index = malloc(sizeof(struct table_entry));
    if (!reg->debug_frame_list->index) {
        free(reg->debug_frame_list);
        free(reg);
        return NULL;
    }

    // Fill in index entry - CORRECTED VERSION
    reg->debug_frame_list->index[0].start_ip_offset = 0;          // IP offset relative to code_start
    reg->debug_frame_list->index[0].fde_offset = fde_offset;             // Offset from debug_frame start to our FDE
    reg->debug_frame_list->index_size = sizeof(struct table_entry);

    // Set up dynamic info
    reg->di = malloc(sizeof(unw_dyn_info_t));
    if (!reg->di) {
        free(reg->debug_frame_list->index);
        free(reg->debug_frame_list->debug_frame);
        free(reg->debug_frame_list);
        free(reg);
        return NULL;
    }

    memset(reg->di, 0, sizeof(*reg->di));
    reg->di->format = UNW_INFO_FORMAT_TABLE;
    reg->di->start_ip = reg->debug_frame_list->start;
    reg->di->end_ip = reg->debug_frame_list->end;
    reg->di->u.ti.name_ptr = (unw_word_t)"JIT";
    reg->di->u.ti.segbase = (unw_word_t)code_start;  // Important for IP offset calculation
    reg->di->u.ti.table_len = sizeof(struct unw_debug_frame_list) / sizeof(unw_word_t);
    reg->di->u.ti.table_data = (unw_word_t*)reg->debug_frame_list;

    _U_dyn_register(reg->di);

    // printf("Registered JIT unwind from %p to %p\n", code_start, (char*)code_start + code_size);

    return reg;
}


// Memory management stuff: ////////////////////////////////////////////////////

#ifndef MS_WINDOWS
    #include <sys/mman.h>
#endif

static size_t
get_page_size(void)
{
#ifdef MS_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}

static void
jit_error(const char *message)
{
#ifdef MS_WINDOWS
    int hint = GetLastError();
#else
    int hint = errno;
#endif
    PyErr_Format(PyExc_RuntimeWarning, "JIT %s (%d)", message, hint);
}

static unsigned char *
jit_alloc(size_t size)
{
    assert(size);
    assert(size % get_page_size() == 0);
#ifdef MS_WINDOWS
    int flags = MEM_COMMIT | MEM_RESERVE;
    unsigned char *memory = VirtualAlloc(NULL, size, flags, PAGE_READWRITE);
    int failed = memory == NULL;
#else
    int flags = MAP_ANONYMOUS | MAP_PRIVATE;
    unsigned char *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    int failed = memory == MAP_FAILED;
#endif
    if (failed) {
        jit_error("unable to allocate memory");
        return NULL;
    }
    return memory;
}

static int
jit_free(unsigned char *memory, size_t size)
{
    assert(size);
    assert(size % get_page_size() == 0);
#ifdef MS_WINDOWS
    int failed = !VirtualFree(memory, 0, MEM_RELEASE);
#else
    int failed = munmap(memory, size);
#endif
    if (failed) {
        jit_error("unable to free memory");
        return -1;
    }
    return 0;
}

static int
mark_executable(unsigned char *memory, size_t size)
{
    if (size == 0) {
        return 0;
    }
    assert(size % get_page_size() == 0);
    // Do NOT ever leave the memory writable! Also, don't forget to flush the
    // i-cache (I cannot begin to tell you how horrible that is to debug):
#ifdef MS_WINDOWS
    if (!FlushInstructionCache(GetCurrentProcess(), memory, size)) {
        jit_error("unable to flush instruction cache");
        return -1;
    }
    int old;
    int failed = !VirtualProtect(memory, size, PAGE_EXECUTE_READ, &old);
#else
    __builtin___clear_cache((char *)memory, (char *)memory + size);
    int failed = mprotect(memory, size, PROT_EXEC | PROT_READ);
#endif
    if (failed) {
        jit_error("unable to protect executable memory");
        return -1;
    }
    return 0;
}

// JIT compiler stuff: /////////////////////////////////////////////////////////

#define SYMBOL_MASK_WORDS 4

typedef uint32_t symbol_mask[SYMBOL_MASK_WORDS];

typedef struct {
    unsigned char *mem;
    symbol_mask mask;
    size_t size;
} trampoline_state;

typedef struct {
    trampoline_state trampolines;
    uintptr_t instruction_starts[UOP_MAX_TRACE_LENGTH];
} jit_state;

// Warning! AArch64 requires you to get your hands dirty. These are your gloves:

// value[value_start : value_start + len]
static uint32_t
get_bits(uint64_t value, uint8_t value_start, uint8_t width)
{
    assert(width <= 32);
    return (value >> value_start) & ((1ULL << width) - 1);
}

// *loc[loc_start : loc_start + width] = value[value_start : value_start + width]
static void
set_bits(uint32_t *loc, uint8_t loc_start, uint64_t value, uint8_t value_start,
         uint8_t width)
{
    assert(loc_start + width <= 32);
    // Clear the bits we're about to patch:
    *loc &= ~(((1ULL << width) - 1) << loc_start);
    assert(get_bits(*loc, loc_start, width) == 0);
    // Patch the bits:
    *loc |= get_bits(value, value_start, width) << loc_start;
    assert(get_bits(*loc, loc_start, width) == get_bits(value, value_start, width));
}

// See https://developer.arm.com/documentation/ddi0602/2023-09/Base-Instructions
// for instruction encodings:
#define IS_AARCH64_ADD_OR_SUB(I) (((I) & 0x11C00000) == 0x11000000)
#define IS_AARCH64_ADRP(I)       (((I) & 0x9F000000) == 0x90000000)
#define IS_AARCH64_BRANCH(I)     (((I) & 0x7C000000) == 0x14000000)
#define IS_AARCH64_LDR_OR_STR(I) (((I) & 0x3B000000) == 0x39000000)
#define IS_AARCH64_MOV(I)        (((I) & 0x9F800000) == 0x92800000)

// LLD is a great reference for performing relocations... just keep in
// mind that Tools/jit/build.py does filtering and preprocessing for us!
// Here's a good place to start for each platform:
// - aarch64-apple-darwin:
//   - https://github.com/llvm/llvm-project/blob/main/lld/MachO/Arch/ARM64.cpp
//   - https://github.com/llvm/llvm-project/blob/main/lld/MachO/Arch/ARM64Common.cpp
//   - https://github.com/llvm/llvm-project/blob/main/lld/MachO/Arch/ARM64Common.h
// - aarch64-pc-windows-msvc:
//   - https://github.com/llvm/llvm-project/blob/main/lld/COFF/Chunks.cpp
// - aarch64-unknown-linux-gnu:
//   - https://github.com/llvm/llvm-project/blob/main/lld/ELF/Arch/AArch64.cpp
// - i686-pc-windows-msvc:
//   - https://github.com/llvm/llvm-project/blob/main/lld/COFF/Chunks.cpp
// - x86_64-apple-darwin:
//   - https://github.com/llvm/llvm-project/blob/main/lld/MachO/Arch/X86_64.cpp
// - x86_64-pc-windows-msvc:
//   - https://github.com/llvm/llvm-project/blob/main/lld/COFF/Chunks.cpp
// - x86_64-unknown-linux-gnu:
//   - https://github.com/llvm/llvm-project/blob/main/lld/ELF/Arch/X86_64.cpp

// Many of these patches are "relaxing", meaning that they can rewrite the
// code they're patching to be more efficient (like turning a 64-bit memory
// load into a 32-bit immediate load). These patches have an "x" in their name.
// Relative patches have an "r" in their name.

// 32-bit absolute address.
void
patch_32(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    // Check that we're not out of range of 32 unsigned bits:
    assert(value < (1ULL << 32));
    *loc32 = (uint32_t)value;
}

// 32-bit relative address.
void
patch_32r(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    value -= (uintptr_t)location;
    // Check that we're not out of range of 32 signed bits:
    assert((int64_t)value >= -(1LL << 31));
    assert((int64_t)value < (1LL << 31));
    *loc32 = (uint32_t)value;
}

// 64-bit absolute address.
void
patch_64(unsigned char *location, uint64_t value)
{
    uint64_t *loc64 = (uint64_t *)location;
    *loc64 = value;
}

// 12-bit low part of an absolute address. Pairs nicely with patch_aarch64_21r
// (below).
void
patch_aarch64_12(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    assert(IS_AARCH64_LDR_OR_STR(*loc32) || IS_AARCH64_ADD_OR_SUB(*loc32));
    // There might be an implicit shift encoded in the instruction:
    uint8_t shift = 0;
    if (IS_AARCH64_LDR_OR_STR(*loc32)) {
        shift = (uint8_t)get_bits(*loc32, 30, 2);
        // If both of these are set, the shift is supposed to be 4.
        // That's pretty weird, and it's never actually been observed...
        assert(get_bits(*loc32, 23, 1) == 0 || get_bits(*loc32, 26, 1) == 0);
    }
    value = get_bits(value, 0, 12);
    assert(get_bits(value, 0, shift) == 0);
    set_bits(loc32, 10, value, shift, 12);
}

// Relaxable 12-bit low part of an absolute address. Pairs nicely with
// patch_aarch64_21rx (below).
void
patch_aarch64_12x(unsigned char *location, uint64_t value)
{
    // This can *only* be relaxed if it occurs immediately before a matching
    // patch_aarch64_21rx. If that happens, the JIT build step will replace both
    // calls with a single call to patch_aarch64_33rx. Otherwise, we end up
    // here, and the instruction is patched normally:
    patch_aarch64_12(location, value);
}

// 16-bit low part of an absolute address.
void
patch_aarch64_16a(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    assert(IS_AARCH64_MOV(*loc32));
    // Check the implicit shift (this is "part 0 of 3"):
    assert(get_bits(*loc32, 21, 2) == 0);
    set_bits(loc32, 5, value, 0, 16);
}

// 16-bit middle-low part of an absolute address.
void
patch_aarch64_16b(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    assert(IS_AARCH64_MOV(*loc32));
    // Check the implicit shift (this is "part 1 of 3"):
    assert(get_bits(*loc32, 21, 2) == 1);
    set_bits(loc32, 5, value, 16, 16);
}

// 16-bit middle-high part of an absolute address.
void
patch_aarch64_16c(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    assert(IS_AARCH64_MOV(*loc32));
    // Check the implicit shift (this is "part 2 of 3"):
    assert(get_bits(*loc32, 21, 2) == 2);
    set_bits(loc32, 5, value, 32, 16);
}

// 16-bit high part of an absolute address.
void
patch_aarch64_16d(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    assert(IS_AARCH64_MOV(*loc32));
    // Check the implicit shift (this is "part 3 of 3"):
    assert(get_bits(*loc32, 21, 2) == 3);
    set_bits(loc32, 5, value, 48, 16);
}

// 21-bit count of pages between this page and an absolute address's page... I
// know, I know, it's weird. Pairs nicely with patch_aarch64_12 (above).
void
patch_aarch64_21r(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    value = (value >> 12) - ((uintptr_t)location >> 12);
    // Check that we're not out of range of 21 signed bits:
    assert((int64_t)value >= -(1 << 20));
    assert((int64_t)value < (1 << 20));
    // value[0:2] goes in loc[29:31]:
    set_bits(loc32, 29, value, 0, 2);
    // value[2:21] goes in loc[5:26]:
    set_bits(loc32, 5, value, 2, 19);
}

// Relaxable 21-bit count of pages between this page and an absolute address's
// page. Pairs nicely with patch_aarch64_12x (above).
void
patch_aarch64_21rx(unsigned char *location, uint64_t value)
{
    // This can *only* be relaxed if it occurs immediately before a matching
    // patch_aarch64_12x. If that happens, the JIT build step will replace both
    // calls with a single call to patch_aarch64_33rx. Otherwise, we end up
    // here, and the instruction is patched normally:
    patch_aarch64_21r(location, value);
}

// 28-bit relative branch.
void
patch_aarch64_26r(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    assert(IS_AARCH64_BRANCH(*loc32));
    value -= (uintptr_t)location;
    // Check that we're not out of range of 28 signed bits:
    assert((int64_t)value >= -(1 << 27));
    assert((int64_t)value < (1 << 27));
    // Since instructions are 4-byte aligned, only use 26 bits:
    assert(get_bits(value, 0, 2) == 0);
    set_bits(loc32, 0, value, 2, 26);
}

// A pair of patch_aarch64_21rx and patch_aarch64_12x.
void
patch_aarch64_33rx(unsigned char *location, uint64_t value)
{
    uint32_t *loc32 = (uint32_t *)location;
    // Try to relax the pair of GOT loads into an immediate value:
    assert(IS_AARCH64_ADRP(*loc32));
    unsigned char reg = get_bits(loc32[0], 0, 5);
    assert(IS_AARCH64_LDR_OR_STR(loc32[1]));
    // There should be only one register involved:
    assert(reg == get_bits(loc32[1], 0, 5));  // ldr's output register.
    assert(reg == get_bits(loc32[1], 5, 5));  // ldr's input register.
    uint64_t relaxed = *(uint64_t *)value;
    if (relaxed < (1UL << 16)) {
        // adrp reg, AAA; ldr reg, [reg + BBB] -> movz reg, XXX; nop
        loc32[0] = 0xD2800000 | (get_bits(relaxed, 0, 16) << 5) | reg;
        loc32[1] = 0xD503201F;
        return;
    }
    if (relaxed < (1ULL << 32)) {
        // adrp reg, AAA; ldr reg, [reg + BBB] -> movz reg, XXX; movk reg, YYY
        loc32[0] = 0xD2800000 | (get_bits(relaxed,  0, 16) << 5) | reg;
        loc32[1] = 0xF2A00000 | (get_bits(relaxed, 16, 16) << 5) | reg;
        return;
    }
    relaxed = value - (uintptr_t)location;
    if ((relaxed & 0x3) == 0 &&
        (int64_t)relaxed >= -(1L << 19) &&
        (int64_t)relaxed < (1L << 19))
    {
        // adrp reg, AAA; ldr reg, [reg + BBB] -> ldr reg, XXX; nop
        loc32[0] = 0x58000000 | (get_bits(relaxed, 2, 19) << 5) | reg;
        loc32[1] = 0xD503201F;
        return;
    }
    // Couldn't do it. Just patch the two instructions normally:
    patch_aarch64_21rx(location, value);
    patch_aarch64_12x(location + 4, value);
}

// Relaxable 32-bit relative address.
void
patch_x86_64_32rx(unsigned char *location, uint64_t value)
{
    uint8_t *loc8 = (uint8_t *)location;
    // Try to relax the GOT load into an immediate value:
    uint64_t relaxed = *(uint64_t *)(value + 4) - 4;
    if ((int64_t)relaxed - (int64_t)location >= -(1LL << 31) &&
        (int64_t)relaxed - (int64_t)location + 1 < (1LL << 31))
    {
        if (loc8[-2] == 0x8B) {
            // mov reg, dword ptr [rip + AAA] -> lea reg, [rip + XXX]
            loc8[-2] = 0x8D;
            value = relaxed;
        }
        else if (loc8[-2] == 0xFF && loc8[-1] == 0x15) {
            // call qword ptr [rip + AAA] -> nop; call XXX
            loc8[-2] = 0x90;
            loc8[-1] = 0xE8;
            value = relaxed;
        }
        else if (loc8[-2] == 0xFF && loc8[-1] == 0x25) {
            // jmp qword ptr [rip + AAA] -> nop; jmp XXX
            loc8[-2] = 0x90;
            loc8[-1] = 0xE9;
            value = relaxed;
        }
    }
    patch_32r(location, value);
}

void patch_aarch64_trampoline(unsigned char *location, int ordinal, jit_state *state);

#include "jit_stencils.h"

#if defined(__aarch64__) || defined(_M_ARM64)
    #define TRAMPOLINE_SIZE 16
#else
    #define TRAMPOLINE_SIZE 0
#endif

// Generate and patch AArch64 trampolines. The symbols to jump to are stored
// in the jit_stencils.h in the symbols_map.
void
patch_aarch64_trampoline(unsigned char *location, int ordinal, jit_state *state)
{
    // Masking is done modulo 32 as the mask is stored as an array of uint32_t
    const uint32_t symbol_mask = 1 << (ordinal % 32);
    const uint32_t trampoline_mask = state->trampolines.mask[ordinal / 32];
    assert(symbol_mask & trampoline_mask);

    // Count the number of set bits in the trampoline mask lower than ordinal,
    // this gives the index into the array of trampolines.
    int index = _Py_popcount32(trampoline_mask & (symbol_mask - 1));
    for (int i = 0; i < ordinal / 32; i++) {
        index += _Py_popcount32(state->trampolines.mask[i]);
    }

    uint32_t *p = (uint32_t*)(state->trampolines.mem + index * TRAMPOLINE_SIZE);
    assert((size_t)(index + 1) * TRAMPOLINE_SIZE <= state->trampolines.size);

    uint64_t value = (uintptr_t)symbols_map[ordinal];

    /* Generate the trampoline
       0: 58000048      ldr     x8, 8
       4: d61f0100      br      x8
       8: 00000000      // The next two words contain the 64-bit address to jump to.
       c: 00000000
    */
    p[0] = 0x58000048;
    p[1] = 0xD61F0100;
    p[2] = value & 0xffffffff;
    p[3] = value >> 32;

    patch_aarch64_26r(location, (uintptr_t)p);
}

static void
combine_symbol_mask(const symbol_mask src, symbol_mask dest)
{
    // Calculate the union of the trampolines required by each StencilGroup
    for (size_t i = 0; i < SYMBOL_MASK_WORDS; i++) {
        dest[i] |= src[i];
    }
}

// Compiles executor in-place. Don't forget to call _PyJIT_Free later!
int
_PyJIT_Compile(_PyExecutorObject *executor, const _PyUOpInstruction trace[], size_t length)
{
    const StencilGroup *group;
    // Loop once to find the total compiled size:
    size_t code_size = 0;
    size_t data_size = 0;
    jit_state state = {0};
    group = &trampoline;
    code_size += group->code_size;
    data_size += group->data_size;
    for (size_t i = 0; i < length; i++) {
        const _PyUOpInstruction *instruction = &trace[i];
        group = &stencil_groups[instruction->opcode];
        state.instruction_starts[i] = code_size;
        code_size += group->code_size;
        data_size += group->data_size;
        combine_symbol_mask(group->trampoline_mask, state.trampolines.mask);
    }
    group = &stencil_groups[_FATAL_ERROR];
    code_size += group->code_size;
    data_size += group->data_size;
    combine_symbol_mask(group->trampoline_mask, state.trampolines.mask);
    // Calculate the size of the trampolines required by the whole trace
    for (size_t i = 0; i < Py_ARRAY_LENGTH(state.trampolines.mask); i++) {
        state.trampolines.size += _Py_popcount32(state.trampolines.mask[i]) * TRAMPOLINE_SIZE;
    }
    // Round up to the nearest page:
    size_t page_size = get_page_size();
    assert((page_size & (page_size - 1)) == 0);
    size_t padding = page_size - ((code_size + data_size + state.trampolines.size) & (page_size - 1));
    size_t total_size = code_size + data_size + state.trampolines.size + padding;
    unsigned char *memory = jit_alloc(total_size);
    if (memory == NULL) {
        return -1;
    }
    // Update the offsets of each instruction:
    for (size_t i = 0; i < length; i++) {
        state.instruction_starts[i] += (uintptr_t)memory;
        const _PyUOpInstruction *instruction = &trace[i];
        group = &stencil_groups[instruction->opcode];
        // register_jit_unwind((void*)state.instruction_starts[i], group->code_size);
    }
    // Loop again to emit the code:
    unsigned char *code = memory;
    unsigned char *data = memory + code_size;
    state.trampolines.mem = memory + code_size + data_size;
    // Compile the trampoline, which handles converting between the native
    // calling convention and the calling convention used by jitted code
    // (which may be different for efficiency reasons). On platforms where
    // we don't change calling conventions, the trampoline is empty and
    // nothing is emitted here:
    group = &trampoline;
    group->emit(code, data, executor, NULL, &state);
    code += group->code_size;
    data += group->data_size;
    assert(trace[0].opcode == _START_EXECUTOR);
    for (size_t i = 0; i < length; i++) {
        const _PyUOpInstruction *instruction = &trace[i];
        group = &stencil_groups[instruction->opcode];
        group->emit(code, data, executor, instruction, &state);
        code += group->code_size;
        data += group->data_size;
    }
    // Protect against accidental buffer overrun into data:
    group = &stencil_groups[_FATAL_ERROR];
    group->emit(code, data, executor, NULL, &state);
    code += group->code_size;
    data += group->data_size;
    assert(code == memory + code_size);
    assert(data == memory + code_size + data_size);
    if (mark_executable(memory, total_size)) {
        jit_free(memory, total_size);
        return -1;
    }
    executor->jit_code = memory;
    executor->jit_side_entry = memory + trampoline.code_size;
    register_jit_unwind((void*)memory, total_size);
    executor->jit_size = total_size;
    return 0;
}

void
_PyJIT_Free(_PyExecutorObject *executor)
{
    unsigned char *memory = (unsigned char *)executor->jit_code;
    size_t size = executor->jit_size;
    if (memory) {
        executor->jit_code = NULL;
        executor->jit_side_entry = NULL;
        executor->jit_size = 0;
        if (jit_free(memory, size)) {
            PyErr_WriteUnraisable(NULL);
        }
    }
}

#endif  // _Py_JIT
