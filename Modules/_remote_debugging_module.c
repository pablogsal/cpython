/******************************************************************************
 * Python Remote Debugging Module
 * 
 * This module provides functionality to debug Python processes remotely by
 * reading their memory and reconstructing stack traces and asyncio task states.
 ******************************************************************************/

#define _GNU_SOURCE

/* ============================================================================
 * HEADERS AND INCLUDES
 * ============================================================================ */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef Py_BUILD_CORE_BUILTIN
#    define Py_BUILD_CORE_MODULE 1
#endif
#include "Python.h"
#include <internal/pycore_debug_offsets.h>  // _Py_DebugOffsets
#include <internal/pycore_frame.h>          // FRAME_SUSPENDED_YIELD_FROM
#include <internal/pycore_interpframe.h>    // FRAME_OWNED_BY_CSTACK
#include <internal/pycore_llist.h>          // struct llist_node
#include <internal/pycore_stackref.h>       // Py_TAG_BITS
#include "../Python/remote_debug.h"

#ifndef HAVE_PROCESS_VM_READV
#    define HAVE_PROCESS_VM_READV 0
#endif

/**/

/* ============================================================================
 * TYPE DEFINITIONS AND STRUCTURES
 * ============================================================================ */

// Copied from Modules/_asynciomodule.c because it's not exported

typedef enum {
    STATE_PENDING,
    STATE_CANCELLED,
    STATE_FINISHED
} fut_state;

#define FutureObj_HEAD(prefix)                                              \
    PyObject_HEAD                                                           \
    PyObject *prefix##_loop;                                                \
    PyObject *prefix##_callback0;                                           \
    PyObject *prefix##_context0;                                            \
    PyObject *prefix##_callbacks;                                           \
    PyObject *prefix##_exception;                                           \
    PyObject *prefix##_exception_tb;                                        \
    PyObject *prefix##_result;                                              \
    PyObject *prefix##_source_tb;                                           \
    PyObject *prefix##_cancel_msg;                                          \
    PyObject *prefix##_cancelled_exc;                                       \
    PyObject *prefix##_awaited_by;                                          \
    fut_state prefix##_state;                                               \
    /* Used by profilers to make traversing the stack from an external      \
       process faster. */                                                   \
    char prefix##_is_task;                                                  \
    char prefix##_awaited_by_is_set;                                        \
    /* These bitfields need to be at the end of the struct                  \
       so that these and bitfields from TaskObj are contiguous.             \
    */                                                                      \
    unsigned prefix##_log_tb: 1;                                            \
    unsigned prefix##_blocking: 1;                                          \

typedef struct {
    FutureObj_HEAD(fut)
} FutureObj;

typedef struct TaskObj {
    FutureObj_HEAD(task)
    unsigned task_must_cancel: 1;
    unsigned task_log_destroy_pending: 1;
    int task_num_cancels_requested;
    PyObject *task_fut_waiter;
    PyObject *task_coro;
    PyObject *task_name;
    PyObject *task_context;
    struct llist_node task_node;
#ifdef Py_GIL_DISABLED
    // thread id of the thread where this task was created
    uintptr_t task_tid;
#endif
} TaskObj;

struct _Py_AsyncioModuleDebugOffsets {
    struct _asyncio_task_object {
        uint64_t size;
        uint64_t task_name;
        uint64_t task_awaited_by;
        uint64_t task_is_task;
        uint64_t task_awaited_by_is_set;
        uint64_t task_coro;
        uint64_t task_node;
    } asyncio_task_object;
    struct _asyncio_interpreter_state {
        uint64_t size;
        uint64_t asyncio_tasks_head;
    } asyncio_interpreter_state;
    struct _asyncio_thread_state {
        uint64_t size;
        uint64_t asyncio_running_loop;
        uint64_t asyncio_running_task;
        uint64_t asyncio_tasks_head;
    } asyncio_thread_state;
};

typedef struct {
    PyObject_HEAD
    proc_handle_t handle;
    uintptr_t runtime_start_address;
    struct _Py_DebugOffsets debug_offsets;
    int async_debug_offsets_available;
    struct _Py_AsyncioModuleDebugOffsets async_debug_offsets;
    uintptr_t interpreter_addr;
    uintptr_t tstate_addr;
    uint64_t code_object_generation;
    _Py_hashtable_t *code_object_cache;
} RemoteUnwinderObject;

typedef struct {
    PyObject *func_name;
    PyObject *file_name;
    int first_lineno;
    PyObject *linetable;  // bytes
    uintptr_t addr_code_adaptive;
} CachedCodeMetadata;

typedef struct {
    /* Types */
    PyTypeObject *RemoteDebugging_Type;
} RemoteDebuggingState;

typedef struct
{
    int lineno;
    int end_lineno;
    int column;
    int end_column;
} LocationInfo;

typedef struct {
    uintptr_t remote_addr;
    size_t size;
    void *local_copy;
} StackChunkInfo;

typedef struct {
    StackChunkInfo *chunks;
    size_t count;
} StackChunkList;

#include "clinic/_remote_debugging_module.c.h"

/*[clinic input]
module _remote_debugging
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=5f507d5b2e76a7f7]*/

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

static int
parse_tasks_in_set(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t set_addr,
    PyObject *awaited_by,
    int recurse_task,
    _Py_hashtable_t *code_object_cache
);

static int
parse_task(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t task_address,
    PyObject *render_to,
    int recurse_task,
    _Py_hashtable_t *code_object_cache
);
 
static int
parse_coro_chain(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t coro_address,
    PyObject *render_to,
    _Py_hashtable_t *code_object_cache
);
 
/* Forward declarations for task parsing functions */
static int parse_frame_object(
    proc_handle_t *handle,
    PyObject** result,
    const struct _Py_DebugOffsets* offsets,
    uintptr_t address,
    uintptr_t* previous_frame,
    _Py_hashtable_t *code_object_cache
);

/* ============================================================================
 * UTILITY FUNCTIONS AND HELPERS
 * ============================================================================ */

static void
cached_code_metadata_destroy(void *ptr)
{
    CachedCodeMetadata *meta = (CachedCodeMetadata *)ptr;
    Py_DECREF(meta->func_name);
    Py_DECREF(meta->file_name);
    Py_DECREF(meta->linetable);
    PyMem_RawFree(meta);
}

static inline RemoteDebuggingState *
RemoteDebugging_GetState(PyObject *module)
{
    void *state = _PyModule_GetState(module);
    assert(state != NULL);
    return (RemoteDebuggingState *)state;
}

static inline int
RemoteDebugging_InitState(RemoteDebuggingState *st)
{
    return 0;
}

// Helper to chain exceptions and avoid repetitions
static void
chain_exceptions(PyObject *exception, const char *string)
{
    PyObject *exc = PyErr_GetRaisedException();
    PyErr_SetString(exception, string);
    _PyErr_ChainExceptions1(exc);
}

/* ============================================================================
 * MEMORY READING FUNCTIONS
 * ============================================================================ */

static inline int
read_ptr(proc_handle_t *handle, uintptr_t address, uintptr_t *ptr_addr)
{
    int result = _Py_RemoteDebug_PagedReadRemoteMemory(handle, address, sizeof(void*), ptr_addr);
    if (result < 0) {
        return -1;
    }
    return 0;
}

static inline int
read_Py_ssize_t(proc_handle_t *handle, uintptr_t address, Py_ssize_t *size)
{
    int result = _Py_RemoteDebug_PagedReadRemoteMemory(handle, address, sizeof(Py_ssize_t), size);
    if (result < 0) {
        return -1;
    }
    return 0;
}

static int
read_py_ptr(proc_handle_t *handle, uintptr_t address, uintptr_t *ptr_addr)
{
    if (read_ptr(handle, address, ptr_addr)) {
        return -1;
    }
    *ptr_addr &= ~Py_TAG_BITS;
    return 0;
}

static int
read_char(proc_handle_t *handle, uintptr_t address, char *result)
{
    int res = _Py_RemoteDebug_PagedReadRemoteMemory(handle, address, sizeof(char), result);
    if (res < 0) {
        return -1;
    }
    return 0;
}

static int
read_remote_pointer(proc_handle_t *handle, uintptr_t address, uintptr_t *out_ptr, const char *error_message)
{
    int bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(handle, address, sizeof(void *), out_ptr);
    if (bytes_read < 0) {
        return -1;
    }

    if ((void *)(*out_ptr) == NULL) {
        PyErr_SetString(PyExc_RuntimeError, error_message);
        return -1;
    }

    return 0;
}

static int
read_instruction_ptr(proc_handle_t *handle, const struct _Py_DebugOffsets *offsets,
                     uintptr_t current_frame, uintptr_t *instruction_ptr)
{
    return read_remote_pointer(
        handle,
        current_frame + offsets->interpreter_frame.instr_ptr,
        instruction_ptr,
        "No instruction ptr found"
    );
}

/* ============================================================================
 * PYTHON OBJECT READING FUNCTIONS
 * ============================================================================ */

static PyObject *
read_py_str(
    proc_handle_t *handle,
    const _Py_DebugOffsets* debug_offsets,
    uintptr_t address,
    Py_ssize_t max_len
) {
    PyObject *result = NULL;
    char *buf = NULL;

    // Read the entire PyUnicodeObject at once
    PyUnicodeObject unicode_obj;
    int res = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        address,
        debug_offsets->unicode_object.size,
        &unicode_obj
    );
    if (res < 0) {
        goto err;
    }

    Py_ssize_t len = unicode_obj._base._base.length;
    if (len < 0 || len > max_len) {
        PyErr_Format(PyExc_RuntimeError,
                     "Invalid string length (%zd) at 0x%lx", len, address);
        return NULL;
    }

    buf = (char *)PyMem_RawMalloc(len+1);
    if (buf == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    size_t offset = debug_offsets->unicode_object.asciiobject_size;
    res = _Py_RemoteDebug_PagedReadRemoteMemory(handle, address + offset, len, buf);
    if (res < 0) {
        goto err;
    }
    buf[len] = '\0';

    result = PyUnicode_FromStringAndSize(buf, len);
    if (result == NULL) {
        goto err;
    }

    PyMem_RawFree(buf);
    assert(result != NULL);
    return result;

err:
    if (buf != NULL) {
        PyMem_RawFree(buf);
    }
    return NULL;
}

static PyObject *
read_py_bytes(
    proc_handle_t *handle,
    const _Py_DebugOffsets* debug_offsets,
    uintptr_t address,
    Py_ssize_t max_len
) {
    PyObject *result = NULL;
    char *buf = NULL;

    // Read the entire PyBytesObject at once
    PyBytesObject bytes_obj;
    int res = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        address,
        debug_offsets->bytes_object.size,
        &bytes_obj
    );
    if (res < 0) {
        goto err;
    }

    Py_ssize_t len = bytes_obj.ob_base.ob_size;
    if (len < 0 || len > max_len) {
        PyErr_Format(PyExc_RuntimeError,
                     "Invalid string length (%zd) at 0x%lx", len, address);
        return NULL;
    }

    buf = (char *)PyMem_RawMalloc(len+1);
    if (buf == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    size_t offset = debug_offsets->bytes_object.ob_sval;
    res = _Py_RemoteDebug_PagedReadRemoteMemory(handle, address + offset, len, buf);
    if (res < 0) {
        goto err;
    }
    buf[len] = '\0';

    result = PyBytes_FromStringAndSize(buf, len);
    if (result == NULL) {
        goto err;
    }

    PyMem_RawFree(buf);
    assert(result != NULL);
    return result;

err:
    if (buf != NULL) {
        PyMem_RawFree(buf);
    }
    return NULL;
}

static long
read_py_long(proc_handle_t *handle, const _Py_DebugOffsets* offsets, uintptr_t address)
{
    unsigned int shift = PYLONG_BITS_IN_DIGIT;

    // Read the entire PyLongObject at once
    PyLongObject long_obj;
    int bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle, 
        address, 
        offsets->long_object.size,
        &long_obj);
    if (bytes_read < 0) {
        return -1;
    }

    uintptr_t lv_tag = long_obj.long_value.lv_tag;
    int negative = (lv_tag & 3) == 2;
    Py_ssize_t size = lv_tag >> 3;

    if (size == 0) {
        return 0;
    }

    // If the long object has inline digits, use them directly
    digit *digits;
    if (size <= _PY_NSMALLNEGINTS + _PY_NSMALLPOSINTS) {
        // For small integers, digits are inline in the long_value.ob_digit array
        digits = (digit *)PyMem_RawMalloc(size * sizeof(digit));
        if (!digits) {
            PyErr_NoMemory();
            return -1;
        }
        memcpy(digits, long_obj.long_value.ob_digit, size * sizeof(digit));
    } else {
        // For larger integers, we need to read the digits separately
        digits = (digit *)PyMem_RawMalloc(size * sizeof(digit));
        if (!digits) {
            PyErr_NoMemory();
            return -1;
        }

        bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
            handle,
            address + offsets->long_object.ob_digit,
            sizeof(digit) * size,
            digits
        );
        if (bytes_read < 0) {
            goto error;
        }
    }

    long long value = 0;

    // In theory this can overflow, but because of llvm/llvm-project#16778
    // we can't use __builtin_mul_overflow because it fails to link with
    // __muloti4 on aarch64. In practice this is fine because all we're
    // testing here are task numbers that would fit in a single byte.
    for (Py_ssize_t i = 0; i < size; ++i) {
        long long factor = digits[i] * (1UL << (Py_ssize_t)(shift * i));
        value += factor;
    }
    PyMem_RawFree(digits);
    if (negative) {
        value *= -1;
    }
    return (long)value;
error:
    PyMem_RawFree(digits);
    return -1;
}

/* ============================================================================
 * ASYNCIO DEBUG FUNCTIONS
 * ============================================================================ */

// Get the PyAsyncioDebug section address for any platform
static uintptr_t
_Py_RemoteDebug_GetAsyncioDebugAddress(proc_handle_t* handle)
{
    uintptr_t address;

#ifdef MS_WINDOWS
    // On Windows, search for asyncio debug in executable or DLL
    address = search_windows_map_for_section(handle, "AsyncioD", L"_asyncio");
    if (address == 0) {
        // Error out: 'python' substring covers both executable and DLL
        PyObject *exc = PyErr_GetRaisedException();
        PyErr_SetString(PyExc_RuntimeError, "Failed to find the AsyncioDebug section in the process.");
        _PyErr_ChainExceptions1(exc);
    }
#elif defined(__linux__)
    // On Linux, search for asyncio debug in executable or DLL
    address = search_linux_map_for_section(handle, "AsyncioDebug", "_asyncio.cpython");
    if (address == 0) {
        // Error out: 'python' substring covers both executable and DLL
        PyObject *exc = PyErr_GetRaisedException();
        PyErr_SetString(PyExc_RuntimeError, "Failed to find the AsyncioDebug section in the process.");
        _PyErr_ChainExceptions1(exc);
    }
#elif defined(__APPLE__) && TARGET_OS_OSX
    // On macOS, try libpython first, then fall back to python
    address = search_map_for_section(handle, "AsyncioDebug", "_asyncio.cpython");
    if (address == 0) {
        PyErr_Clear();
        address = search_map_for_section(handle, "AsyncioDebug", "_asyncio.cpython");
    }
    if (address == 0) {
        // Error out: 'python' substring covers both executable and DLL
        PyObject *exc = PyErr_GetRaisedException();
        PyErr_SetString(PyExc_RuntimeError, "Failed to find the AsyncioDebug section in the process.");
        _PyErr_ChainExceptions1(exc);
    }
#else
    Py_UNREACHABLE();
#endif

    return address;
}

static int
read_async_debug(
    proc_handle_t *handle,
    struct _Py_AsyncioModuleDebugOffsets* async_debug
) {
    uintptr_t async_debug_addr = _Py_RemoteDebug_GetAsyncioDebugAddress(handle);
    if (!async_debug_addr) {
        return -1;
    }

    size_t size = sizeof(struct _Py_AsyncioModuleDebugOffsets);
    int result = _Py_RemoteDebug_PagedReadRemoteMemory(handle, async_debug_addr, size, async_debug);
    return result;
}

/* ============================================================================
 * ASYNCIO TASK PARSING FUNCTIONS
 * ============================================================================ */

static PyObject *
parse_task_name(
    proc_handle_t *handle,
    const _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t task_address
) {
    // Read the entire TaskObj at once
    TaskObj task_obj;
    int err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        task_address,
        async_offsets->asyncio_task_object.size,
        &task_obj);
    if (err < 0) {
        return NULL;
    }

    uintptr_t task_name_addr = (uintptr_t)task_obj.task_name;
    task_name_addr &= ~Py_TAG_BITS;

    // The task name can be a long or a string so we need to check the type
    PyObject task_name_obj;
    err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        task_name_addr,
        offsets->pyobject.size,
        &task_name_obj);
    if (err < 0) {
        return NULL;
    }

    // Now read the type object to get the flags
    PyTypeObject type_obj;
    err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        (uintptr_t)task_name_obj.ob_type,
        offsets->type_object.size,
        &type_obj);
    if (err < 0) {
        return NULL;
    }

    if ((type_obj.tp_flags & Py_TPFLAGS_LONG_SUBCLASS)) {
        long res = read_py_long(handle, offsets, task_name_addr);
        if (res == -1) {
            chain_exceptions(PyExc_RuntimeError, "Failed to get task name");
            return NULL;
        }
        return PyUnicode_FromFormat("Task-%d", res);
    }

    if(!(type_obj.tp_flags & Py_TPFLAGS_UNICODE_SUBCLASS)) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid task name object");
        return NULL;
    }

    return read_py_str(
        handle,
        offsets,
        task_name_addr,
        255
    );
}

static int parse_task_awaited_by(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t task_address,
    PyObject *awaited_by,
    int recurse_task,
    _Py_hashtable_t *code_object_cache
) {
    // Read the entire TaskObj at once
    TaskObj task_obj;
    int err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        task_address,
        async_offsets->asyncio_task_object.size,
        &task_obj);
    if (err < 0) {
        return -1;
    }

    uintptr_t task_ab_addr = (uintptr_t)task_obj.task_awaited_by;
    task_ab_addr &= ~Py_TAG_BITS;

    if ((void*)task_ab_addr == NULL) {
        return 0;
    }

    char awaited_by_is_a_set = task_obj.task_awaited_by_is_set;

    if (awaited_by_is_a_set) {
        if (parse_tasks_in_set(
            handle,
            offsets,
            async_offsets,
            task_ab_addr,
            awaited_by,
            recurse_task,
            code_object_cache
        )
         ) {
            return -1;
        }
    } else {
        if (parse_task(
            handle,
            offsets,
            async_offsets,
            task_ab_addr,
            awaited_by,
            recurse_task,
            code_object_cache
        )
        ) {
            return -1;
        }
    }

    return 0;
}

static int
handle_yield_from_frame(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t gi_iframe_addr,
    uintptr_t gen_type_addr,
    PyObject *render_to,
    _Py_hashtable_t *code_object_cache
) {
    // Read the entire interpreter frame at once
    _PyInterpreterFrame iframe;
    int err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        gi_iframe_addr,
        offsets->interpreter_frame.size,
        &iframe);
    if (err < 0) {
        return -1;
    }
    
    if (iframe.owner != FRAME_OWNED_BY_GENERATOR) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "generator doesn't own its frame \\_o_/");
        return -1;
    }

    uintptr_t stackpointer_addr = (uintptr_t)iframe.stackpointer;
    stackpointer_addr &= ~Py_TAG_BITS;

    if ((void*)stackpointer_addr != NULL) {
        uintptr_t gi_await_addr;
        err = read_py_ptr(
            handle,
            stackpointer_addr - sizeof(void*),
            &gi_await_addr);
        if (err) {
            return -1;
        }

        if ((void*)gi_await_addr != NULL) {
            uintptr_t gi_await_addr_type_addr;
            err = read_ptr(
                handle,
                gi_await_addr + offsets->pyobject.ob_type,
                &gi_await_addr_type_addr);
            if (err) {
                return -1;
            }

            if (gen_type_addr == gi_await_addr_type_addr) {
                /* This needs an explanation. We always start with parsing
                   native coroutine / generator frames. Ultimately they
                   are awaiting on something. That something can be
                   a native coroutine frame or... an iterator.
                   If it's the latter -- we can't continue building
                   our chain. So the condition to bail out of this is
                   to do that when the type of the current coroutine
                   doesn't match the type of whatever it points to
                   in its cr_await.
                */
                err = parse_coro_chain(
                    handle,
                    offsets,
                    async_offsets,
                    gi_await_addr,
                    render_to,
                    code_object_cache
                );
                if (err) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int
parse_coro_chain(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t coro_address,
    PyObject *render_to,
    _Py_hashtable_t *code_object_cache
) {
    assert((void*)coro_address != NULL);

    // Read the entire generator object at once
    PyGenObject gen_object;
    int err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        coro_address,
        offsets->gen_object.size,
        &gen_object);
    if (err < 0) {
        return -1;
    }

    uintptr_t gen_type_addr = (uintptr_t)gen_object.ob_base.ob_type;

    PyObject* name = NULL;

    // Parse the previous frame using the gi_iframe from local copy
    uintptr_t prev_frame;
    uintptr_t gi_iframe_addr = coro_address + offsets->gen_object.gi_iframe;
    if (parse_frame_object(
                handle,
                &name,
                offsets,
                gi_iframe_addr,
                &prev_frame, code_object_cache)
        < 0)
    {
        return -1;
    }

    if (PyList_Append(render_to, name)) {
        Py_DECREF(name);
        return -1;
    }
    Py_DECREF(name);

    if (gen_object.gi_frame_state == FRAME_SUSPENDED_YIELD_FROM) {
        return handle_yield_from_frame(
            handle, offsets, async_offsets, gi_iframe_addr, 
            gen_type_addr, render_to, code_object_cache);
    }

    return 0;
}

static PyObject *
create_task_result(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t task_address,
    int recurse_task,
    _Py_hashtable_t *code_object_cache
) {
    PyObject* result = PyList_New(0);
    if (result == NULL) {
        return NULL;
    }

    PyObject *call_stack = PyList_New(0);
    if (call_stack == NULL) {
        Py_DECREF(result);
        return NULL;
    }
    if (PyList_Append(result, call_stack)) {
        Py_DECREF(call_stack);
        Py_DECREF(result);
        return NULL;
    }
    /* we can operate on a borrowed one to simplify cleanup */
    Py_DECREF(call_stack);

    PyObject *tn = NULL;
    if (recurse_task) {
        tn = parse_task_name(
            handle, offsets, async_offsets, task_address);
    } else {
        tn = PyLong_FromUnsignedLongLong(task_address);
    }
    if (tn == NULL) {
        Py_DECREF(result);
        return NULL;
    }
    if (PyList_Append(result, tn)) {
        Py_DECREF(tn);
        Py_DECREF(result);
        return NULL;
    }
    Py_DECREF(tn);

    // Parse coroutine chain
    TaskObj task_obj;
    int err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        task_address,
        async_offsets->asyncio_task_object.size,
        &task_obj);
    if (err < 0) {
        Py_DECREF(result);
        return NULL;
    }

    uintptr_t coro_addr = (uintptr_t)task_obj.task_coro;
    coro_addr &= ~Py_TAG_BITS;

    if ((void*)coro_addr != NULL) {
        err = parse_coro_chain(
            handle,
            offsets,
            async_offsets,
            coro_addr,
            call_stack,
            code_object_cache
        );
        if (err) {
            Py_DECREF(result);
            return NULL;
        }

        if (PyList_Reverse(call_stack)) {
            Py_DECREF(result);
            return NULL;
        }
    }

    return result;
}

static int
parse_task(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t task_address,
    PyObject *render_to,
    int recurse_task,
    _Py_hashtable_t *code_object_cache
) {
    char is_task;
    int err = read_char(
        handle,
        task_address + async_offsets->asyncio_task_object.task_is_task,
        &is_task);
    if (err) {
        return -1;
    }

    PyObject* result = NULL;
    if (is_task) {
        result = create_task_result(handle, offsets, async_offsets, 
                                   task_address, recurse_task, code_object_cache);
        if (!result) {
            return -1;
        }
    } else {
        result = PyList_New(0);
        if (result == NULL) {
            return -1;
        }
    }

    if (PyList_Append(render_to, result)) {
        Py_DECREF(result);
        return -1;
    }

    if (recurse_task) {
        PyObject *awaited_by = PyList_New(0);
        if (awaited_by == NULL) {
            Py_DECREF(result);
            return -1;
        }
        if (PyList_Append(result, awaited_by)) {
            Py_DECREF(awaited_by);
            Py_DECREF(result);
            return -1;
        }
        /* we can operate on a borrowed one to simplify cleanup */
        Py_DECREF(awaited_by);

        if (parse_task_awaited_by(handle, offsets, async_offsets,
                                task_address, awaited_by, 1, code_object_cache)
        ) {
            Py_DECREF(result);
            return -1;
        }
    }
    Py_DECREF(result);

    return 0;
}

static int
process_set_entry(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t table_ptr,
    PyObject *awaited_by,
    int recurse_task,
    _Py_hashtable_t *code_object_cache
) {
    uintptr_t key_addr;
    if (read_py_ptr(handle, table_ptr, &key_addr)) {
        return -1;
    }

    if ((void*)key_addr != NULL) {
        Py_ssize_t ref_cnt;
        if (read_Py_ssize_t(handle, table_ptr, &ref_cnt)) {
            return -1;
        }

        if (ref_cnt) {
            // if 'ref_cnt=0' it's a set dummy marker
            if (parse_task(
                handle,
                offsets,
                async_offsets,
                key_addr,
                awaited_by,
                recurse_task,
                code_object_cache
            )) {
                return -1;
            }
            return 1; // Successfully processed a valid entry
        }
    }
    return 0; // Entry was NULL or dummy marker
}

static int
parse_tasks_in_set(
    proc_handle_t *handle,
    const struct _Py_DebugOffsets* offsets,
    const struct _Py_AsyncioModuleDebugOffsets* async_offsets,
    uintptr_t set_addr,
    PyObject *awaited_by,
    int recurse_task,
    _Py_hashtable_t *code_object_cache
) {

    PySetObject set_object;
    int err = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        set_addr,
        offsets->set_object.size,
        &set_object);
    if (err < 0) {
        return -1;
    }

    Py_ssize_t num_els = set_object.used;
    Py_ssize_t set_len = set_object.mask + 1; // The set contains the `mask+1` element slots.
    uintptr_t table_ptr = (uintptr_t)set_object.table;

    Py_ssize_t i = 0;
    Py_ssize_t els = 0;
    while (i < set_len && els < num_els) {
        int result = process_set_entry(
            handle, offsets, async_offsets, table_ptr,
            awaited_by, recurse_task, code_object_cache);
        
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            els++;
        }

        table_ptr += sizeof(void*) * 2;
        i++;
    }
    return 0;
}

/* ============================================================================
 * LINE TABLE PARSING FUNCTIONS
 * ============================================================================ */

static int
scan_varint(const uint8_t **ptr)
{
    unsigned int read = **ptr;
    *ptr = *ptr + 1;
    unsigned int val = read & 63;
    unsigned int shift = 0;
    while (read & 64) {
        read = **ptr;
        *ptr = *ptr + 1;
        shift += 6;
        val |= (read & 63) << shift;
    }
    return val;
}

static int
scan_signed_varint(const uint8_t **ptr)
{
    unsigned int uval = scan_varint(ptr);
    if (uval & 1) {
        return -(int)(uval >> 1);
    }
    else {
        return uval >> 1;
    }
}

static bool
parse_linetable(const uintptr_t addrq, const char* linetable, int firstlineno, LocationInfo* info)
{
    const uint8_t* ptr = (const uint8_t*)(linetable);
    uint64_t addr = 0;
    info->lineno = firstlineno;

    while (*ptr != '\0') {
        // See InternalDocs/code_objects.md for where these magic numbers are from
        // and for the decoding algorithm.
        uint8_t first_byte = *(ptr++);
        uint8_t code = (first_byte >> 3) & 15;
        size_t length = (first_byte & 7) + 1;
        uintptr_t end_addr = addr + length;
        switch (code) {
            case PY_CODE_LOCATION_INFO_NONE: {
                break;
            }
            case PY_CODE_LOCATION_INFO_LONG: {
                int line_delta = scan_signed_varint(&ptr);
                info->lineno += line_delta;
                info->end_lineno = info->lineno + scan_varint(&ptr);
                info->column = scan_varint(&ptr) - 1;
                info->end_column = scan_varint(&ptr) - 1;
                break;
            }
            case PY_CODE_LOCATION_INFO_NO_COLUMNS: {
                int line_delta = scan_signed_varint(&ptr);
                info->lineno += line_delta;
                info->column = info->end_column = -1;
                break;
            }
            case PY_CODE_LOCATION_INFO_ONE_LINE0:
            case PY_CODE_LOCATION_INFO_ONE_LINE1:
            case PY_CODE_LOCATION_INFO_ONE_LINE2: {
                int line_delta = code - 10;
                info->lineno += line_delta;
                info->end_lineno = info->lineno;
                info->column = *(ptr++);
                info->end_column = *(ptr++);
                break;
            }
            default: {
                uint8_t second_byte = *(ptr++);
                if ((second_byte & 128) != 0) {
                    return false;
                }
                info->column = code << 3 | (second_byte >> 4);
                info->end_column = info->column + (second_byte & 15);
                break;
            }
        }
        if (addr <= addrq && end_addr > addrq) {
            return true;
        }
        addr = end_addr;
    }
    return false;
}

/* ============================================================================
 * CODE OBJECT AND FRAME PARSING FUNCTIONS
 * ============================================================================ */

static int
parse_code_object(proc_handle_t *handle,
                  PyObject **result,
                  const struct _Py_DebugOffsets *offsets,
                  uintptr_t address,
                  uintptr_t current_frame,
                  uintptr_t *previous_frame,
                  _Py_hashtable_t *code_object_cache)
{
    void *key = (void *)address;
    CachedCodeMetadata *meta = NULL;

    if (code_object_cache != NULL) {
        meta = _Py_hashtable_get(code_object_cache, key);
    }

    if (meta == NULL) {
        PyCodeObject code_object;
        if (_Py_RemoteDebug_PagedReadRemoteMemory(
                handle, address, offsets->code_object.size, &code_object) < 0)
        {
            return -1;
        }

        PyObject *func = read_py_str(handle, offsets, (uintptr_t)code_object.co_qualname, 1024);
        if (!func) {
            return -1;
        }

        PyObject *file = read_py_str(handle, offsets, (uintptr_t)code_object.co_filename, 1024);
        if (!file) {
            Py_DECREF(func);
            return -1;
        }

        PyObject *linetable = read_py_bytes(handle, offsets, (uintptr_t)code_object.co_linetable, 4096);
        if (!linetable) {
            Py_DECREF(func);
            Py_DECREF(file);
            return -1;
        }

        meta = PyMem_RawMalloc(sizeof(CachedCodeMetadata));
        if (!meta) {
            Py_DECREF(func);
            Py_DECREF(file);
            Py_DECREF(linetable);
            return -1;
        }

        meta->func_name = func;
        meta->file_name = file;
        meta->linetable = linetable;
        meta->first_lineno = code_object.co_firstlineno;
        meta->addr_code_adaptive = address + offsets->code_object.co_code_adaptive;

        if (code_object_cache && _Py_hashtable_set(code_object_cache, key, meta) < 0)
        {
            cached_code_metadata_destroy(meta);
            return -1;
        }
    } 

    // Instruction pointer for the current frame
    uintptr_t ip = 0;
    if (read_instruction_ptr(handle, offsets, current_frame, &ip) < 0) {
        return -1;
    }

    ptrdiff_t addrq = (uint16_t *)ip - (uint16_t *)meta->addr_code_adaptive;

    LocationInfo info = {0};
    bool ok = parse_linetable(addrq, PyBytes_AS_STRING(meta->linetable),
                              meta->first_lineno, &info);
    if (!ok) {
        info.lineno = -1;
    }

    PyObject *lineno = PyLong_FromLong(info.lineno);
    if (!lineno) {
        return -1;
    }

    PyObject *tuple = PyTuple_New(3);
    if (!tuple) {
        Py_DECREF(lineno);
        return -1;
    }

    Py_INCREF(meta->func_name);
    Py_INCREF(meta->file_name);
    PyTuple_SET_ITEM(tuple, 0, meta->func_name);
    PyTuple_SET_ITEM(tuple, 1, meta->file_name);
    PyTuple_SET_ITEM(tuple, 2, lineno);

    *result = tuple;
    return 0;
}

/* ============================================================================
 * STACK CHUNK MANAGEMENT FUNCTIONS
 * ============================================================================ */

static int
copy_stack_chunks(proc_handle_t *handle,
                  uintptr_t tstate_addr,
                  const _Py_DebugOffsets *offsets,
                  StackChunkList *out_chunks)
{
    uintptr_t chunk_addr;
    if (read_ptr(handle,
                 tstate_addr + offsets->thread_state.datastack_chunk,
                 &chunk_addr)) {
        return -1;
    }

    size_t max_chunks = 16;
    StackChunkInfo *chunks = PyMem_RawMalloc(max_chunks * sizeof(StackChunkInfo));
    if (!chunks) {
        PyErr_NoMemory();
        return -1;
    }

    size_t count = 0;
    while (chunk_addr != 0) {

        // Asume the chunk has the default size
        size_t current_size = _PY_DATA_STACK_CHUNK_SIZE;

        _PyStackChunk* this_chunk = PyMem_RawMalloc(current_size);
        if (!this_chunk) {
            PyErr_NoMemory();
            goto error;
        }

        if (_Py_RemoteDebug_PagedReadRemoteMemory(handle, chunk_addr, current_size, this_chunk) < 0) {
            PyMem_RawFree(this_chunk);
            goto error;
        }

        // Check the actual size of the chunk
        size_t size = this_chunk->size;

        if (size != current_size) {
            current_size = size;
            this_chunk = PyMem_RawRealloc(this_chunk, current_size);
            if (!this_chunk) {
                PyMem_RawFree(this_chunk);
                PyErr_NoMemory();
                goto error;
            }
            if (_Py_RemoteDebug_PagedReadRemoteMemory(handle, chunk_addr, current_size, this_chunk) < 0) {
                PyMem_RawFree(this_chunk);
                goto error;
            }
        }

       if (count >= max_chunks) {
            max_chunks *= 2;
            StackChunkInfo *new_chunks = PyMem_RawRealloc(chunks, max_chunks * sizeof(StackChunkInfo));
            if (!new_chunks) {
                PyErr_NoMemory();
                goto error;
            }
            chunks = new_chunks;
        }

        chunks[count++] = (StackChunkInfo){chunk_addr, size, this_chunk};
        chunk_addr = (uintptr_t)(this_chunk->previous);
    }

    out_chunks->chunks = chunks;
    out_chunks->count = count;
    return 0;

error:
    for (size_t i = 0; i < count; ++i) {
        PyMem_RawFree(chunks[i].local_copy);
    }
    PyMem_RawFree(chunks);
    return -1;
}

static void *
find_frame_in_chunks(StackChunkList *chunks, uintptr_t remote_ptr)
{
    for (size_t i = 0; i < chunks->count; ++i) {
        uintptr_t base = chunks->chunks[i].remote_addr + offsetof(_PyStackChunk, data);
        size_t payload = chunks->chunks[i].size - offsetof(_PyStackChunk, data);

        if (remote_ptr >= base && remote_ptr < base + payload) {
            return (char *)chunks->chunks[i].local_copy + (remote_ptr - chunks->chunks[i].remote_addr);
        }
    }
    return NULL;
}

static int
parse_frame_from_chunks(
    proc_handle_t *handle,
    PyObject **result,
    const struct _Py_DebugOffsets *offsets,
    uintptr_t address,
    uintptr_t *previous_frame,
    StackChunkList *chunks,
    _Py_hashtable_t *code_object_cache
) {
    void *frame_ptr = find_frame_in_chunks(chunks, address);
    if (!frame_ptr) {
        return -1;
    }

    _PyInterpreterFrame *frame = (_PyInterpreterFrame *)frame_ptr;
    *previous_frame = (uintptr_t)frame->previous;

    if (frame->owner >= FRAME_OWNED_BY_INTERPRETER || !frame->f_executable.bits) {
        return 0;
    }

    return parse_code_object(
        handle, result, offsets, frame->f_executable.bits, address, previous_frame, code_object_cache);
}

/* ============================================================================
 * FRAME PARSING FUNCTIONS
 * ============================================================================ */

static int
parse_frame_object(
    proc_handle_t *handle,
    PyObject** result,
    const struct _Py_DebugOffsets* offsets,
    uintptr_t address,
    uintptr_t* previous_frame,
    _Py_hashtable_t *code_object_cache
) {
    _PyInterpreterFrame frame;

    Py_ssize_t bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        address,
        offsets->interpreter_frame.size,
        &frame
    );
    if (bytes_read < 0) {
        return -1;
    }

    *previous_frame = (uintptr_t)frame.previous;

    if (frame.owner >= FRAME_OWNED_BY_INTERPRETER) {
        return 0;
    }

    if ((void*)frame.f_executable.bits == NULL) {
        return 0;
    }

    return parse_code_object(
        handle, result, offsets, frame.f_executable.bits, address, previous_frame, code_object_cache);
}

static int
parse_async_frame_object(
    proc_handle_t *handle,
    PyObject** result,
    const struct _Py_DebugOffsets* offsets,
    uintptr_t address,
    uintptr_t* previous_frame,
    uintptr_t* code_object,
    _Py_hashtable_t *code_object_cache
) {
    _PyInterpreterFrame frame;

    Py_ssize_t bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        address,
        offsets->interpreter_frame.size,
        &frame
    );
    if (bytes_read < 0) {
        return -1;
    }

    *previous_frame = (uintptr_t)frame.previous;

    if (frame.owner == FRAME_OWNED_BY_CSTACK || frame.owner == FRAME_OWNED_BY_INTERPRETER) {
        return 0;  // C frame
    }

    if (frame.owner != FRAME_OWNED_BY_GENERATOR
        && frame.owner != FRAME_OWNED_BY_THREAD) {
        PyErr_Format(PyExc_RuntimeError, "Unhandled frame owner %d.\n", frame.owner);
        return -1;
    }

    *code_object = frame.f_executable.bits;

    assert(code_object != NULL);
    if ((void*)*code_object == NULL) {
        return 0;
    }

    if (parse_code_object(
        handle, result, offsets, *code_object, address, previous_frame, code_object_cache)) {
        return -1;
    }

    return 1;
}

/* ============================================================================
 * INTERPRETER STATE AND THREAD DISCOVERY FUNCTIONS
 * ============================================================================ */

static int
populate_initial_state_data(
    int all_threads,
    proc_handle_t *handle,
    uintptr_t runtime_start_address,
    const _Py_DebugOffsets* local_debug_offsets,
    uintptr_t *interpreter_state,
    uintptr_t *tstate
) {
    uint64_t interpreter_state_list_head =
        local_debug_offsets->runtime_state.interpreters_head;

    uintptr_t address_of_interpreter_state;
    int bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
            handle,
            runtime_start_address + interpreter_state_list_head,
            sizeof(void*),
            &address_of_interpreter_state);
    if (bytes_read < 0) {
        return -1;
    }

    if (address_of_interpreter_state == 0) {
        PyErr_SetString(PyExc_RuntimeError, "No interpreter state found");
        return -1;
    }

    *interpreter_state = address_of_interpreter_state;

    if (all_threads) {
        *tstate = 0;
        return 0;
    }

    uintptr_t address_of_thread = address_of_interpreter_state +
                    local_debug_offsets->interpreter_state.threads_main;

    if (_Py_RemoteDebug_PagedReadRemoteMemory(
            handle,
            address_of_thread,
            sizeof(void*),
            tstate) < 0) {
        return -1;
    }

    return 0;
}

static int
find_running_frame(
    proc_handle_t *handle,
    uintptr_t runtime_start_address,
    const _Py_DebugOffsets* local_debug_offsets,
    uintptr_t *frame
) {
    uint64_t interpreter_state_list_head =
        local_debug_offsets->runtime_state.interpreters_head;

    uintptr_t address_of_interpreter_state;
    int bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
            handle,
            runtime_start_address + interpreter_state_list_head,
            sizeof(void*),
            &address_of_interpreter_state);
    if (bytes_read < 0) {
        return -1;
    }

    if (address_of_interpreter_state == 0) {
        PyErr_SetString(PyExc_RuntimeError, "No interpreter state found");
        return -1;
    }

    uintptr_t address_of_thread;
    bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
            handle,
            address_of_interpreter_state +
                local_debug_offsets->interpreter_state.threads_main,
            sizeof(void*),
            &address_of_thread);
    if (bytes_read < 0) {
        return -1;
    }

    // No Python frames are available for us (can happen at tear-down).
    if ((void*)address_of_thread != NULL) {
        int err = read_ptr(
            handle,
            address_of_thread + local_debug_offsets->thread_state.current_frame,
            frame);
        if (err) {
            return -1;
        }
        return 0;
    }

    *frame = (uintptr_t)NULL;
    return 0;
}

static int
find_running_task(
    proc_handle_t *handle,
    uintptr_t runtime_start_address,
    const _Py_DebugOffsets *local_debug_offsets,
    struct _Py_AsyncioModuleDebugOffsets *async_offsets,
    uintptr_t *running_task_addr
) {
    *running_task_addr = (uintptr_t)NULL;

    uint64_t interpreter_state_list_head =
        local_debug_offsets->runtime_state.interpreters_head;

    uintptr_t address_of_interpreter_state;
    int bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
            handle,
            runtime_start_address + interpreter_state_list_head,
            sizeof(void*),
            &address_of_interpreter_state);
    if (bytes_read < 0) {
        return -1;
    }

    if (address_of_interpreter_state == 0) {
        PyErr_SetString(PyExc_RuntimeError, "No interpreter state found");
        return -1;
    }

    uintptr_t address_of_thread;
    bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
            handle,
            address_of_interpreter_state +
                local_debug_offsets->interpreter_state.threads_head,
            sizeof(void*),
            &address_of_thread);
    if (bytes_read < 0) {
        return -1;
    }

    uintptr_t address_of_running_loop;
    // No Python frames are available for us (can happen at tear-down).
    if ((void*)address_of_thread == NULL) {
        return 0;
    }

    bytes_read = read_py_ptr(
        handle,
        address_of_thread
        + async_offsets->asyncio_thread_state.asyncio_running_loop,
        &address_of_running_loop);
    if (bytes_read == -1) {
        return -1;
    }

    // no asyncio loop is now running
    if ((void*)address_of_running_loop == NULL) {
        return 0;
    }

    int err = read_ptr(
        handle,
        address_of_thread
        + async_offsets->asyncio_thread_state.asyncio_running_task,
        running_task_addr);
    if (err) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * AWAITED BY PARSING FUNCTIONS
 * ============================================================================ */

static int
append_awaited_by_for_thread(
    proc_handle_t *handle,
    uintptr_t head_addr,
    const struct _Py_DebugOffsets *debug_offsets,
    const struct _Py_AsyncioModuleDebugOffsets *async_offsets,
    PyObject *result,
    _Py_hashtable_t *code_object_cache
) {
    struct llist_node task_node;

    if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                handle,
                head_addr,
                sizeof(task_node),
                &task_node))
    {
        return -1;
    }

    size_t iteration_count = 0;
    const size_t MAX_ITERATIONS = 2 << 15;  // A reasonable upper bound
    while ((uintptr_t)task_node.next != head_addr) {
        if (++iteration_count > MAX_ITERATIONS) {
            PyErr_SetString(PyExc_RuntimeError, "Task list appears corrupted");
            return -1;
        }

        if (task_node.next == NULL) {
            PyErr_SetString(
                PyExc_RuntimeError,
                "Invalid linked list structure reading remote memory");
            return -1;
        }

        uintptr_t task_addr = (uintptr_t)task_node.next
            - async_offsets->asyncio_task_object.task_node;

        PyObject *tn = parse_task_name(
            handle,
            debug_offsets,
            async_offsets,
            task_addr);
        if (tn == NULL) {
            return -1;
        }

        PyObject *current_awaited_by = PyList_New(0);
        if (current_awaited_by == NULL) {
            Py_DECREF(tn);
            return -1;
        }

        PyObject* task_id = PyLong_FromUnsignedLongLong(task_addr);
        if (task_id == NULL) {
            Py_DECREF(tn);
            Py_DECREF(current_awaited_by);
            return -1;
        }

        PyObject *result_item = PyTuple_New(3);
        if (result_item == NULL) {
            Py_DECREF(tn);
            Py_DECREF(current_awaited_by);
            Py_DECREF(task_id);
            return -1;
        }

        PyTuple_SET_ITEM(result_item, 0, task_id);  // steals ref
        PyTuple_SET_ITEM(result_item, 1, tn);  // steals ref
        PyTuple_SET_ITEM(result_item, 2, current_awaited_by);  // steals ref
        if (PyList_Append(result, result_item)) {
            Py_DECREF(result_item);
            return -1;
        }
        Py_DECREF(result_item);

        if (parse_task_awaited_by(handle, debug_offsets, async_offsets,
                                  task_addr, current_awaited_by, 0, code_object_cache))
        {
            return -1;
        }

        // onto the next one...
        if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                    handle,
                    (uintptr_t)task_node.next,
                    sizeof(task_node),
                    &task_node))
        {
            return -1;
        }
    }

    return 0;
}

static int
append_awaited_by(
    proc_handle_t *handle,
    unsigned long tid,
    uintptr_t head_addr,
    const struct _Py_DebugOffsets *debug_offsets,
    const struct _Py_AsyncioModuleDebugOffsets *async_offsets,
    _Py_hashtable_t *code_object_cache,
    PyObject *result)
{
    PyObject *tid_py = PyLong_FromUnsignedLong(tid);
    if (tid_py == NULL) {
        return -1;
    }

    PyObject *result_item = PyTuple_New(2);
    if (result_item == NULL) {
        Py_DECREF(tid_py);
        return -1;
    }

    PyObject* awaited_by_for_thread = PyList_New(0);
    if (awaited_by_for_thread == NULL) {
        Py_DECREF(tid_py);
        Py_DECREF(result_item);
        return -1;
    }

    PyTuple_SET_ITEM(result_item, 0, tid_py);  // steals ref
    PyTuple_SET_ITEM(result_item, 1, awaited_by_for_thread);  // steals ref
    if (PyList_Append(result, result_item)) {
        Py_DECREF(result_item);
        return -1;
    }
    Py_DECREF(result_item);

    if (append_awaited_by_for_thread(
            handle,
            head_addr,
            debug_offsets,
            async_offsets,
            awaited_by_for_thread,
            code_object_cache))
    {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * STACK UNWINDING FUNCTIONS
 * ============================================================================ */

static PyObject*
unwind_stack_for_thread(
    proc_handle_t *handle,
    uintptr_t *current_tstate,
    const _Py_DebugOffsets *offsets,
    _Py_hashtable_t *code_object_cache
) {
    PyThreadState ts;
    int bytes_read = _Py_RemoteDebug_PagedReadRemoteMemory(
        handle,
        *current_tstate,
        offsets->thread_state.size,
        &ts);
    if (bytes_read < 0) {
        return NULL;
    }

    uintptr_t frame_addr = (uintptr_t)ts.current_frame;
    uintptr_t prev_frame_addr = 0;

    PyObject* frame_info = PyList_New(0);
    if (!frame_info) {
        return NULL;
    }

    StackChunkList chunks = {0};
    if (copy_stack_chunks(handle, *current_tstate, offsets, &chunks) < 0)
    {
        Py_DECREF(frame_info);
        return NULL;
    }

    const size_t MAX_FRAMES = 1024;
    size_t frame_count = 0;

    while ((void*)frame_addr != NULL) {
        PyObject *frame = NULL;
        uintptr_t next_frame_addr = 0;

        if (++frame_count > MAX_FRAMES) {
            PyErr_SetString(PyExc_RuntimeError, "Too many stack frames (possible infinite loop)");
            goto error;
        }

        if (parse_frame_from_chunks(handle, &frame,
                                    offsets, frame_addr, &next_frame_addr,
                                    &chunks, code_object_cache) < 0)
        {
            PyErr_Clear();
            if (parse_frame_object(handle, &frame,
                                offsets, frame_addr, &next_frame_addr,
                                code_object_cache) < 0)
            {
                goto error;
            }
        }

        if (!frame) {
            break;
        }

        if (prev_frame_addr && frame_addr != prev_frame_addr) {
            PyErr_Format(PyExc_RuntimeError,
                        "Broken frame chain: expected frame at 0x%lx, got 0x%lx",
                        prev_frame_addr, frame_addr);
            Py_DECREF(frame);
            goto error;
        }

        if (PyList_Append(frame_info, frame) == 1) {
            Py_DECREF(frame);
            goto error;
        }
        Py_DECREF(frame);

        prev_frame_addr = next_frame_addr;
        frame_addr = next_frame_addr;
    }

    for (size_t i = 0; i < chunks.count; ++i) {
        PyMem_RawFree(chunks.chunks[i].local_copy);
    }
    PyMem_RawFree(chunks.chunks);

    *current_tstate = (uintptr_t)ts.next;

    PyObject* thread_id = PyLong_FromLongLong(ts.native_thread_id);
    if (thread_id == NULL) {
        Py_DECREF(frame_info);
    }

    PyObject* result = PyTuple_New(2);
    if (result == NULL) {
        Py_DECREF(frame_info);
        Py_DECREF(thread_id);
        return NULL;
    }

    PyTuple_SET_ITEM(result, 0, thread_id);
    PyTuple_SET_ITEM(result, 1, frame_info);

    return result;
error:
    Py_XDECREF(frame_info);
    for (size_t i = 0; i < chunks.count; ++i) {
        PyMem_RawFree(chunks.chunks[i].local_copy);
    }
    PyMem_RawFree(chunks.chunks);
    return NULL;
}

/* ============================================================================
 * PUBLIC API FUNCTIONS
 * ============================================================================ */

/*[clinic input]
_remote_debugging.get_all_awaited_by
    pid: int
Get all tasks and their awaited_by from a given pid
[clinic start generated code]*/

static PyObject *
_remote_debugging_get_all_awaited_by_impl(PyObject *module, int pid)
/*[clinic end generated code: output=327d67d6f5492c97 input=3ab97592cfcbf599]*/
{
#if (!defined(__linux__) && !defined(__APPLE__))  && !defined(MS_WINDOWS) || \
    (defined(__linux__) && !HAVE_PROCESS_VM_READV)
    PyErr_SetString(
        PyExc_RuntimeError,
        "get_all_awaited_by is not implemented on this platform");
    return NULL;
#endif

    proc_handle_t the_handle;
    proc_handle_t *handle = &the_handle;
    if (_Py_RemoteDebug_InitProcHandle(handle, pid) < 0) {
        return 0;
    }

    _Py_hashtable_t *code_object_cache = _Py_hashtable_new_full(
        _Py_hashtable_hash_ptr,
        _Py_hashtable_compare_direct,
        NULL,  // keys are stable pointers, don't destroy
        cached_code_metadata_destroy,
        NULL
    );
    if (code_object_cache == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    PyObject *result = NULL;

    uintptr_t runtime_start_addr = _Py_RemoteDebug_GetPyRuntimeAddress(handle);
    if (runtime_start_addr == 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(
                PyExc_RuntimeError, "Failed to get .PyRuntime address");
        }
        goto result_err;
    }
    struct _Py_DebugOffsets local_debug_offsets;

    if (_Py_RemoteDebug_ReadDebugOffsets(handle, &runtime_start_addr, &local_debug_offsets)) {
        chain_exceptions(PyExc_RuntimeError, "Failed to read debug offsets");
        goto result_err;
    }

    struct _Py_AsyncioModuleDebugOffsets local_async_debug;
    if (read_async_debug(handle, &local_async_debug)) {
        chain_exceptions(PyExc_RuntimeError, "Failed to read asyncio debug offsets");
        goto result_err;
    }

    result = PyList_New(0);
    if (result == NULL) {
        goto result_err;
    }

    uint64_t interpreter_state_list_head =
        local_debug_offsets.runtime_state.interpreters_head;

    uintptr_t interpreter_state_addr;
    if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                handle,
                runtime_start_addr + interpreter_state_list_head,
                sizeof(void*),
                &interpreter_state_addr))
    {
        goto result_err;
    }

    uintptr_t thread_state_addr;
    unsigned long tid = 0;
    if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                handle,
                interpreter_state_addr
                + local_debug_offsets.interpreter_state.threads_main,
                sizeof(void*),
                &thread_state_addr))
    {
        goto result_err;
    }

    uintptr_t head_addr;
    while (thread_state_addr != 0) {
        if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                    handle,
                    thread_state_addr
                    + local_debug_offsets.thread_state.native_thread_id,
                    sizeof(tid),
                    &tid))
        {
            goto result_err;
        }

        head_addr = thread_state_addr
            + local_async_debug.asyncio_thread_state.asyncio_tasks_head;

        if (append_awaited_by(handle, tid, head_addr, &local_debug_offsets,
                              &local_async_debug, code_object_cache, result))
        {
            goto result_err;
        }

        if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                    handle,
                    thread_state_addr + local_debug_offsets.thread_state.next,
                    sizeof(void*),
                    &thread_state_addr))
        {
            goto result_err;
        }
    }

    head_addr = interpreter_state_addr
        + local_async_debug.asyncio_interpreter_state.asyncio_tasks_head;

    // On top of a per-thread task lists used by default by asyncio to avoid
    // contention, there is also a fallback per-interpreter list of tasks;
    // any tasks still pending when a thread is destroyed will be moved to the
    // per-interpreter task list.  It's unlikely we'll find anything here, but
    // interesting for debugging.
    if (append_awaited_by(handle, 0, head_addr, &local_debug_offsets,
                        &local_async_debug, code_object_cache, result))
    {
        goto result_err;
    }

    _Py_hashtable_destroy(code_object_cache);
    _Py_RemoteDebug_CleanupProcHandle(handle);
    return result;

result_err:
    _Py_hashtable_destroy(code_object_cache);
    Py_XDECREF(result);
    _Py_RemoteDebug_CleanupProcHandle(handle);
    return NULL;
}

/*[clinic input]
_remote_debugging.get_async_stack_trace
    pid: int
Get the asyncio stack from a given pid
[clinic start generated code]*/

static PyObject *
_remote_debugging_get_async_stack_trace_impl(PyObject *module, int pid)
/*[clinic end generated code: output=81f4b0012d347aca input=7684fc95166dbb02]*/
{
#if (!defined(__linux__) && !defined(__APPLE__))  && !defined(MS_WINDOWS) || \
    (defined(__linux__) && !HAVE_PROCESS_VM_READV)
    PyErr_SetString(
        PyExc_RuntimeError,
        "get_stack_trace is not supported on this platform");
    return NULL;
#endif

    proc_handle_t the_handle;
    proc_handle_t *handle = &the_handle;
    if (_Py_RemoteDebug_InitProcHandle(handle, pid) < 0) {
        return 0;
    }

    PyObject *result = NULL;
    _Py_hashtable_t *code_object_cache = _Py_hashtable_new_full(
        _Py_hashtable_hash_ptr,
        _Py_hashtable_compare_direct,
        NULL,  // keys are stable pointers, don't destroy
        cached_code_metadata_destroy,
        NULL
    );
 
    if (code_object_cache == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
 
    uintptr_t runtime_start_address = _Py_RemoteDebug_GetPyRuntimeAddress(handle);
    if (runtime_start_address == 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(
                PyExc_RuntimeError, "Failed to get .PyRuntime address");
        }
        goto result_err;
    }
    struct _Py_DebugOffsets local_debug_offsets;

    if (_Py_RemoteDebug_ReadDebugOffsets(handle, &runtime_start_address, &local_debug_offsets)) {
        chain_exceptions(PyExc_RuntimeError, "Failed to read debug offsets");
        goto result_err;
    }

    struct _Py_AsyncioModuleDebugOffsets local_async_debug;
    if (read_async_debug(handle, &local_async_debug)) {
        chain_exceptions(PyExc_RuntimeError, "Failed to read asyncio debug offsets");
        goto result_err;
    }

    result = PyList_New(1);
    if (result == NULL) {
        goto result_err;
    }
    PyObject* calls = PyList_New(0);
    if (calls == NULL) {
        goto result_err;
    }
    if (PyList_SetItem(result, 0, calls)) { /* steals ref to 'calls' */
        Py_DECREF(calls);
        goto result_err;
    }

    uintptr_t running_task_addr = (uintptr_t)NULL;
    if (find_running_task(
        handle, runtime_start_address, &local_debug_offsets, &local_async_debug,
        &running_task_addr)
    ) {
        chain_exceptions(PyExc_RuntimeError, "Failed to find running task");
        goto result_err;
    }

    if ((void*)running_task_addr == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "No running task found");
        goto result_err;
    }

    uintptr_t running_coro_addr;
    if (read_py_ptr(
        handle,
        running_task_addr + local_async_debug.asyncio_task_object.task_coro,
        &running_coro_addr
    )) {
        chain_exceptions(PyExc_RuntimeError, "Failed to read running task coro");
        goto result_err;
    }

    if ((void*)running_coro_addr == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Running task coro is NULL");
        goto result_err;
    }

    // note: genobject's gi_iframe is an embedded struct so the address to
    // the offset leads directly to its first field: f_executable
    uintptr_t address_of_running_task_code_obj;
    if (read_py_ptr(
        handle,
        running_coro_addr + local_debug_offsets.gen_object.gi_iframe,
        &address_of_running_task_code_obj
    )) {
        goto result_err;
    }

    if ((void*)address_of_running_task_code_obj == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Running task code object is NULL");
        goto result_err;
    }

    uintptr_t address_of_current_frame;
    if (find_running_frame(
        handle, runtime_start_address, &local_debug_offsets,
        &address_of_current_frame)
    ) {
        chain_exceptions(PyExc_RuntimeError, "Failed to find running frame");
        goto result_err;
    }

    uintptr_t address_of_code_object;
    while ((void*)address_of_current_frame != NULL) {
        PyObject* frame_info = NULL;
        int res = parse_async_frame_object(
            handle,
            &frame_info,
            &local_debug_offsets,
            address_of_current_frame,
            &address_of_current_frame,
            &address_of_code_object,
            code_object_cache
        );

        if (res < 0) {
            chain_exceptions(PyExc_RuntimeError, "Failed to parse async frame object");
            goto result_err;
        }

        if (!frame_info) {
            continue;
        }

        if (PyList_Append(calls, frame_info) == -1) {
            Py_DECREF(frame_info);
            goto result_err;
        }

        Py_DECREF(frame_info);
        frame_info = NULL;

        if (address_of_code_object == address_of_running_task_code_obj) {
            break;
        }
    }

    PyObject *tn = parse_task_name(
        handle, &local_debug_offsets, &local_async_debug, running_task_addr);
    if (tn == NULL) {
        goto result_err;
    }
    if (PyList_Append(result, tn)) {
        Py_DECREF(tn);
        goto result_err;
    }
    Py_DECREF(tn);

    PyObject* awaited_by = PyList_New(0);
    if (awaited_by == NULL) {
        goto result_err;
    }
    if (PyList_Append(result, awaited_by)) {
        Py_DECREF(awaited_by);
        goto result_err;
    }
    Py_DECREF(awaited_by);

    if (parse_task_awaited_by(
        handle, &local_debug_offsets, &local_async_debug,
        running_task_addr, awaited_by, 1, code_object_cache)
    ) {
        goto result_err;
    }

    _Py_hashtable_destroy(code_object_cache);
    _Py_RemoteDebug_CleanupProcHandle(handle);
    return result;

result_err:
    _Py_hashtable_destroy(code_object_cache);
    _Py_RemoteDebug_CleanupProcHandle(handle);
    Py_XDECREF(result);
    return NULL;
}

/* ============================================================================
 * REMOTEUNWINDER CLASS IMPLEMENTATION
 * ============================================================================ */

/*[clinic input]
class _remote_debugging.RemoteUnwinder "RemoteUnwinderObject *" "&RemoteUnwinder_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=55f164d8803318be]*/

/*[clinic input]
_remote_debugging.RemoteUnwinder.__init__
    pid: int
    *
    all_threads: bool = False

Something
[clinic start generated code]*/

static int
_remote_debugging_RemoteUnwinder___init___impl(RemoteUnwinderObject *self,
                                               int pid, int all_threads)
/*[clinic end generated code: output=b8027cb247092081 input=1076d886433b1988]*/
{
    if (_Py_RemoteDebug_InitProcHandle(&self->handle, pid) < 0) {
        return -1;
    }

    self->runtime_start_address = _Py_RemoteDebug_GetPyRuntimeAddress(&self->handle);
    if (self->runtime_start_address == 0) {
        return -1;
    }

    if (_Py_RemoteDebug_ReadDebugOffsets(&self->handle,
                                         &self->runtime_start_address,
                                         &self->debug_offsets) < 0)
    {
        return -1;
    }

    // Try to read async debug offsets, but don't fail if they're not available
    self->async_debug_offsets_available = 1;
    if (read_async_debug(&self->handle, &self->async_debug_offsets) < 0) {
        PyErr_Clear();
        memset(&self->async_debug_offsets, 0, sizeof(self->async_debug_offsets));
        self->async_debug_offsets_available = 0;
    }

    if (populate_initial_state_data(all_threads, &self->handle, self->runtime_start_address,
                    &self->debug_offsets, &self->interpreter_addr ,&self->tstate_addr) < 0)
    {
        return -1;
    }

    self->code_object_cache = _Py_hashtable_new_full(
        _Py_hashtable_hash_ptr,
        _Py_hashtable_compare_direct,
        NULL,  // keys are stable pointers, don't destroy
        cached_code_metadata_destroy,
        NULL
    );
    if (self->code_object_cache == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    return 0;
}

/*[clinic input]
@critical_section
_remote_debugging.RemoteUnwinder.get_stack_trace
Blah blah blah
[clinic start generated code]*/

static PyObject *
_remote_debugging_RemoteUnwinder_get_stack_trace_impl(RemoteUnwinderObject *self)
/*[clinic end generated code: output=666192b90c69d567 input=aa504416483c9467]*/
{
    PyObject* result = NULL;

    // Check the code object generatiom
    uint64_t code_object_generation = 0;
    if (_Py_RemoteDebug_PagedReadRemoteMemory(
        &self->handle, 
        self->interpreter_addr+self->debug_offsets.interpreter_state.code_object_generation, 
        sizeof(uint64_t), &code_object_generation) < 0) {
        goto exit;
    }

    if (code_object_generation != self->code_object_generation) {
        self->code_object_generation = code_object_generation;
        _Py_hashtable_clear(self->code_object_cache);
    }

    uintptr_t current_tstate;
    if (self->tstate_addr == 0) {
        if (_Py_RemoteDebug_PagedReadRemoteMemory(
                &self->handle,
                self->interpreter_addr +
                    self->debug_offsets.interpreter_state.threads_head,
                sizeof(void*),
                &current_tstate) < 0) {
            goto exit;
        }
    } else {
        current_tstate = self->tstate_addr;
    }

    result = PyList_New(0);
    if (!result) {
        goto exit;
    }

    while (current_tstate != 0) {
        PyObject* frame_info = unwind_stack_for_thread(&self->handle, 
                                                       &current_tstate, &self->debug_offsets,
                                                       self->code_object_cache);
        if (!frame_info) {
            Py_CLEAR(result);
            goto exit;
        }

        if (PyList_Append(result, frame_info) == -1) {
            Py_DECREF(frame_info);
            Py_CLEAR(result);
            goto exit;
        }
        Py_DECREF(frame_info);

        // We are targeting a single tstate, break here
        if (self->tstate_addr) {
            break;
        }
    }

exit:
   _Py_RemoteDebug_ClearCache(&self->handle);
    return result;
}

/*[clinic input]
@critical_section
_remote_debugging.RemoteUnwinder.get_all_awaited_by
Get all tasks and their awaited_by from the remote process
[clinic start generated code]*/

static PyObject *
_remote_debugging_RemoteUnwinder_get_all_awaited_by_impl(RemoteUnwinderObject *self)
/*[clinic end generated code: output=6a49cd345e8aec53 input=40a62dc4725b295e]*/
{
    if (!self->async_debug_offsets_available) {
        PyErr_SetString(PyExc_RuntimeError, "AsyncioDebug section not available");
        return NULL;
    }

    PyObject *result = PyList_New(0);
    if (result == NULL) {
        goto result_err;
    }

    uintptr_t thread_state_addr;
    unsigned long tid = 0;
    if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                &self->handle,
                self->interpreter_addr
                + self->debug_offsets.interpreter_state.threads_main,
                sizeof(void*),
                &thread_state_addr))
    {
        goto result_err;
    }

    uintptr_t head_addr;
    while (thread_state_addr != 0) {
        if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                    &self->handle,
                    thread_state_addr
                    + self->debug_offsets.thread_state.native_thread_id,
                    sizeof(tid),
                    &tid))
        {
            goto result_err;
        }

        head_addr = thread_state_addr
            + self->async_debug_offsets.asyncio_thread_state.asyncio_tasks_head;

        if (append_awaited_by(&self->handle, tid, head_addr, &self->debug_offsets,
                              &self->async_debug_offsets, self->code_object_cache, result))
        {
            goto result_err;
        }

        if (0 > _Py_RemoteDebug_PagedReadRemoteMemory(
                    &self->handle,
                    thread_state_addr + self->debug_offsets.thread_state.next,
                    sizeof(void*),
                    &thread_state_addr))
        {
            goto result_err;
        }
    }

    head_addr = self->interpreter_addr
        + self->async_debug_offsets.asyncio_interpreter_state.asyncio_tasks_head;

    // On top of a per-thread task lists used by default by asyncio to avoid
    // contention, there is also a fallback per-interpreter list of tasks;
    // any tasks still pending when a thread is destroyed will be moved to the
    // per-interpreter task list.  It's unlikely we'll find anything here, but
    // interesting for debugging.
    if (append_awaited_by(&self->handle, 0, head_addr, &self->debug_offsets,
                        &self->async_debug_offsets, self->code_object_cache, result))
    {
        goto result_err;
    }

    _Py_RemoteDebug_ClearCache(&self->handle);
    return result;

result_err:
    _Py_RemoteDebug_ClearCache(&self->handle);
    Py_XDECREF(result);
    return NULL;
}

/*[clinic input]
@critical_section
_remote_debugging.RemoteUnwinder.get_async_stack_trace
Get the asyncio stack from the remote process
[clinic start generated code]*/

static PyObject *
_remote_debugging_RemoteUnwinder_get_async_stack_trace_impl(RemoteUnwinderObject *self)
/*[clinic end generated code: output=6433d52b55e87bbe input=a94e61c351cc4eed]*/
{

    if (!self->async_debug_offsets_available) {
        PyErr_SetString(PyExc_RuntimeError, "AsyncioDebug section not available");
        return NULL;
    }

    PyObject *result = PyList_New(1);
    if (result == NULL) {
        goto result_err;
    }
    PyObject* calls = PyList_New(0);
    if (calls == NULL) {
        goto result_err;
    }
    if (PyList_SetItem(result, 0, calls)) { /* steals ref to 'calls' */
        Py_DECREF(calls);
        goto result_err;
    }

    uintptr_t running_task_addr = (uintptr_t)NULL;
    if (find_running_task(
        &self->handle, self->runtime_start_address, &self->debug_offsets, &self->async_debug_offsets,
        &running_task_addr)
    ) {
        chain_exceptions(PyExc_RuntimeError, "Failed to find running task");
        goto result_err;
    }

    if ((void*)running_task_addr == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "No running task found");
        goto result_err;
    }

    uintptr_t running_coro_addr;
    if (read_py_ptr(
        &self->handle,
        running_task_addr + self->async_debug_offsets.asyncio_task_object.task_coro,
        &running_coro_addr
    )) {
        chain_exceptions(PyExc_RuntimeError, "Failed to read running task coro");
        goto result_err;
    }

    if ((void*)running_coro_addr == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Running task coro is NULL");
        goto result_err;
    }

    // note: genobject's gi_iframe is an embedded struct so the address to
    // the offset leads directly to its first field: f_executable
    uintptr_t address_of_running_task_code_obj;
    if (read_py_ptr(
        &self->handle,
        running_coro_addr + self->debug_offsets.gen_object.gi_iframe,
        &address_of_running_task_code_obj
    )) {
        goto result_err;
    }

    if ((void*)address_of_running_task_code_obj == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Running task code object is NULL");
        goto result_err;
    }

    uintptr_t address_of_current_frame;
    if (find_running_frame(
        &self->handle, self->runtime_start_address, &self->debug_offsets,
        &address_of_current_frame)
    ) {
        chain_exceptions(PyExc_RuntimeError, "Failed to find running frame");
        goto result_err;
    }

    uintptr_t address_of_code_object;
    while ((void*)address_of_current_frame != NULL) {
        PyObject* frame_info = NULL;
        int res = parse_async_frame_object(
            &self->handle,
            &frame_info,
            &self->debug_offsets,
            address_of_current_frame,
            &address_of_current_frame,
            &address_of_code_object,
            self->code_object_cache
        );

        if (res < 0) {
            chain_exceptions(PyExc_RuntimeError, "Failed to parse async frame object");
            goto result_err;
        }

        if (!frame_info) {
            continue;
        }

        if (PyList_Append(calls, frame_info) == -1) {
            Py_DECREF(frame_info);
            goto result_err;
        }

        Py_DECREF(frame_info);
        frame_info = NULL;

        if (address_of_code_object == address_of_running_task_code_obj) {
            break;
        }
    }

    PyObject *tn = parse_task_name(
        &self->handle, &self->debug_offsets, &self->async_debug_offsets, running_task_addr);
    if (tn == NULL) {
        goto result_err;
    }
    if (PyList_Append(result, tn)) {
        Py_DECREF(tn);
        goto result_err;
    }
    Py_DECREF(tn);

    PyObject* awaited_by = PyList_New(0);
    if (awaited_by == NULL) {
        goto result_err;
    }
    if (PyList_Append(result, awaited_by)) {
        Py_DECREF(awaited_by);
        goto result_err;
    }
    Py_DECREF(awaited_by);

    if (parse_task_awaited_by(
        &self->handle, &self->debug_offsets, &self->async_debug_offsets,
        running_task_addr, awaited_by, 1, self->code_object_cache)
    ) {
        goto result_err;
    }

    _Py_RemoteDebug_ClearCache(&self->handle);
    return result;

result_err:
    _Py_RemoteDebug_ClearCache(&self->handle);
    Py_XDECREF(result);
    return NULL;
}

static PyMethodDef RemoteUnwinder_methods[] = {
    _REMOTE_DEBUGGING_REMOTEUNWINDER_GET_STACK_TRACE_METHODDEF
    _REMOTE_DEBUGGING_REMOTEUNWINDER_GET_ALL_AWAITED_BY_METHODDEF
    _REMOTE_DEBUGGING_REMOTEUNWINDER_GET_ASYNC_STACK_TRACE_METHODDEF
    {NULL, NULL}
};

static void
RemoteUnwinder_dealloc(RemoteUnwinderObject *self)
{
    PyTypeObject *tp = Py_TYPE(self);
    if (self->code_object_cache) {
        _Py_hashtable_destroy(self->code_object_cache);
    }
    if (self->handle.pid != 0) {
        _Py_RemoteDebug_ClearCache(&self->handle);
        _Py_RemoteDebug_CleanupProcHandle(&self->handle);
    }
    PyObject_Del(self);
    Py_DECREF(tp);
}

static PyType_Slot RemoteUnwinder_slots[] = {
    {Py_tp_doc, (void *)"RemoteUnwinder(pid): Inspect stack of a remote Python process."},
    {Py_tp_methods, RemoteUnwinder_methods},
    {Py_tp_init, _remote_debugging_RemoteUnwinder___init__},
    {Py_tp_dealloc, RemoteUnwinder_dealloc},
    {0, NULL}
};

static PyType_Spec RemoteUnwinder_spec = {
    .name = "_remote_debugging.RemoteUnwinder",
    .basicsize = sizeof(RemoteUnwinderObject),
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = RemoteUnwinder_slots,
};

/* ============================================================================
 * MODULE INITIALIZATION
 * ============================================================================ */

static int
_remote_debugging_exec(PyObject *m)
{
    RemoteDebuggingState *st = RemoteDebugging_GetState(m);
#define CREATE_TYPE(mod, type, spec)                                        \
    do {                                                                    \
        type = (PyTypeObject *)PyType_FromMetaclass(NULL, mod, spec, NULL); \
        if (type == NULL) {                                                 \
            return -1;                                                      \
        }                                                                   \
    } while (0)

    CREATE_TYPE(m, st->RemoteDebugging_Type, &RemoteUnwinder_spec);

   if (PyModule_AddType(m, st->RemoteDebugging_Type) < 0) {
        return -1;
    }
#ifdef Py_GIL_DISABLED
    PyUnstable_Module_SetGIL(mod, Py_MOD_GIL_NOT_USED);
#endif
    int rc = PyModule_AddIntConstant(m, "PROCESS_VM_READV_SUPPORTED", HAVE_PROCESS_VM_READV);
    if (rc < 0) {
        return -1;
    }
    if (RemoteDebugging_InitState(st) < 0) {
        return -1;
    }
    return 0;
}

static int
remote_debugging_traverse(PyObject *mod, visitproc visit, void *arg)
{
    RemoteDebuggingState *state = RemoteDebugging_GetState(mod);
    Py_VISIT(state->RemoteDebugging_Type);
    return 0;
}

static int
remote_debugging_clear(PyObject *mod)
{
    RemoteDebuggingState *state = RemoteDebugging_GetState(mod);
    Py_CLEAR(state->RemoteDebugging_Type);
    return 0;
}

static void
remote_debugging_free(void *mod)
{
    (void)remote_debugging_clear((PyObject *)mod);
}

static PyModuleDef_Slot remote_debugging_slots[] = {
    {Py_mod_exec, _remote_debugging_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL},
};

static PyMethodDef remote_debugging_methods[] = {
    _REMOTE_DEBUGGING_GET_ASYNC_STACK_TRACE_METHODDEF
    _REMOTE_DEBUGGING_GET_ALL_AWAITED_BY_METHODDEF
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef remote_debugging_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_remote_debugging",
    .m_size = sizeof(RemoteDebuggingState),
    .m_methods = remote_debugging_methods,
    .m_slots = remote_debugging_slots,
    .m_traverse = remote_debugging_traverse,
    .m_clear = remote_debugging_clear,
    .m_free = remote_debugging_free,
};

PyMODINIT_FUNC
PyInit__remote_debugging(void)
{
    return PyModuleDef_Init(&remote_debugging_module);
}

