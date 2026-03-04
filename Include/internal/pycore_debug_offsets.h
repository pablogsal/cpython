#ifndef Py_INTERNAL_DEBUG_OFFSETS_H
#define Py_INTERNAL_DEBUG_OFFSETS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include <stdint.h>

#define _Py_Debug_Cookie "xdebugpy"

typedef struct _Py_DebugOffsets {
    char cookie[9];
    uint64_t version;
    uint64_t free_threaded;  // Always 0 for 3.12 (no free-threading support)

    // Runtime state offset
    struct {
        uint64_t size;
        uint64_t finalizing;
        uint64_t interpreters_head;
    } runtime_state;

    // Interpreter state offset
    struct {
        uint64_t size;
        uint64_t id;
        uint64_t next;
        uint64_t threads_head;
        uint64_t gc;
        uint64_t sysdict;
        uint64_t builtins;
        uint64_t ceval_gil;
        uint64_t gil_runtime_state;
        uint64_t gil_runtime_state_locked;
        uint64_t gil_runtime_state_holder;
    } interpreter_state;

    // Thread state offset
    struct {
        uint64_t size;
        uint64_t prev;
        uint64_t next;
        uint64_t interp;
        uint64_t current_frame;
        uint64_t base_frame;
        uint64_t last_profiled_frame;
        uint64_t thread_id;
        uint64_t native_thread_id;
        uint64_t datastack_chunk;
        uint64_t status;
        uint64_t current_exception;
        uint64_t exc_state;
        uint64_t holds_gil;
        uint64_t gil_requested;
    } thread_state;

    // Exception stack item offset
    struct {
        uint64_t exc_value;
    } err_stackitem;

    // Interpreter frame offset
    struct {
        uint64_t size;
        uint64_t previous;
        uint64_t executable;    // f_code in 3.12
        uint64_t instr_ptr;     // prev_instr in 3.12
        uint64_t localsplus;
        uint64_t owner;
    } interpreter_frame;

    // Code object offset
    struct {
        uint64_t size;
        uint64_t filename;
        uint64_t name;
        uint64_t qualname;
        uint64_t linetable;
        uint64_t firstlineno;
        uint64_t argcount;
        uint64_t localsplusnames;
        uint64_t localspluskinds;
        uint64_t co_code_adaptive;
    } code_object;

    // PyObject offset
    struct {
        uint64_t size;
        uint64_t ob_type;
    } pyobject;

    // PyTypeObject offset
    struct {
        uint64_t size;
        uint64_t tp_name;
        uint64_t tp_repr;
        uint64_t tp_flags;
    } type_object;

    // Tuple object offset
    struct {
        uint64_t size;
        uint64_t ob_item;
        uint64_t ob_size;
    } tuple_object;

    // List object offset
    struct {
        uint64_t size;
        uint64_t ob_item;
        uint64_t ob_size;
    } list_object;

    // Dict object offset
    struct {
        uint64_t size;
        uint64_t ma_keys;
        uint64_t ma_values;
    } dict_object;

    // Float object offset
    struct {
        uint64_t size;
        uint64_t ob_fval;
    } float_object;

    // Long object offset
    struct {
        uint64_t size;
        uint64_t lv_tag;
        uint64_t ob_digit;
    } long_object;

    // Bytes object offset
    struct {
        uint64_t size;
        uint64_t ob_size;
        uint64_t ob_sval;
    } bytes_object;

    // Unicode object offset
    struct {
        uint64_t size;
        uint64_t state;
        uint64_t length;
        uint64_t asciiobject_size;
    } unicode_object;

    // GC runtime state offset
    struct {
        uint64_t size;
        uint64_t collecting;
        uint64_t frame;
    } gc;

    // Generator object offset
    struct {
        uint64_t size;
        uint64_t gi_name;
        uint64_t gi_iframe;
        uint64_t gi_frame_state;
    } gen_object;

    // Debugger support offsets (PEP 768)
    struct {
        uint64_t eval_breaker;
        uint64_t remote_debugger_support;
        uint64_t remote_debugging_enabled;
        uint64_t debugger_pending_call;
        uint64_t debugger_script_path;
        uint64_t debugger_script_path_size;
    } debugger_support;

} _Py_DebugOffsets;


// Macro to initialize debug offsets - uses 3.12 field names
#define _Py_DebugOffsets_INIT() { \
    .cookie = _Py_Debug_Cookie, \
    .version = PY_VERSION_HEX, \
    .free_threaded = 0, \
    .runtime_state = { \
        .size = sizeof(_PyRuntimeState), \
        .finalizing = offsetof(_PyRuntimeState, _finalizing), \
        .interpreters_head = offsetof(_PyRuntimeState, interpreters.head), \
    }, \
    .interpreter_state = { \
        .size = sizeof(PyInterpreterState), \
        .id = offsetof(PyInterpreterState, id), \
        .next = offsetof(PyInterpreterState, next), \
        .threads_head = offsetof(PyInterpreterState, threads.head), \
        .gc = offsetof(PyInterpreterState, gc), \
        .sysdict = offsetof(PyInterpreterState, sysdict), \
        .builtins = offsetof(PyInterpreterState, builtins), \
        .ceval_gil = offsetof(PyInterpreterState, ceval) + offsetof(struct _ceval_state, gil), \
        .gil_runtime_state = offsetof(PyInterpreterState, _gil), \
        .gil_runtime_state_locked = offsetof(PyInterpreterState, _gil) + offsetof(struct _gil_runtime_state, locked), \
        .gil_runtime_state_holder = offsetof(PyInterpreterState, _gil) + offsetof(struct _gil_runtime_state, last_holder), \
    }, \
    .thread_state = { \
        .size = sizeof(PyThreadState), \
        .prev = offsetof(PyThreadState, prev), \
        .next = offsetof(PyThreadState, next), \
        .interp = offsetof(PyThreadState, interp), \
        .current_frame = offsetof(PyThreadState, current_frame), \
        .base_frame = offsetof(PyThreadState, base_frame), \
        .last_profiled_frame = offsetof(PyThreadState, last_profiled_frame), \
        .thread_id = offsetof(PyThreadState, thread_id), \
        .native_thread_id = offsetof(PyThreadState, native_thread_id), \
        .datastack_chunk = offsetof(PyThreadState, datastack_chunk), \
        .status = offsetof(PyThreadState, _status), \
        .current_exception = offsetof(PyThreadState, current_exception), \
        .exc_state = offsetof(PyThreadState, exc_state), \
        .holds_gil = offsetof(PyThreadState, holds_gil), \
        .gil_requested = offsetof(PyThreadState, gil_requested), \
    }, \
    .err_stackitem = { \
        .exc_value = offsetof(_PyErr_StackItem, exc_value), \
    }, \
    .interpreter_frame = { \
        .size = sizeof(_PyInterpreterFrame), \
        .previous = offsetof(_PyInterpreterFrame, previous), \
        .executable = offsetof(_PyInterpreterFrame, f_code), \
        .instr_ptr = offsetof(_PyInterpreterFrame, prev_instr), \
        .localsplus = offsetof(_PyInterpreterFrame, localsplus), \
        .owner = offsetof(_PyInterpreterFrame, owner), \
    }, \
    .code_object = { \
        .size = sizeof(PyCodeObject), \
        .filename = offsetof(PyCodeObject, co_filename), \
        .name = offsetof(PyCodeObject, co_name), \
        .qualname = offsetof(PyCodeObject, co_qualname), \
        .linetable = offsetof(PyCodeObject, co_linetable), \
        .firstlineno = offsetof(PyCodeObject, co_firstlineno), \
        .argcount = offsetof(PyCodeObject, co_argcount), \
        .localsplusnames = offsetof(PyCodeObject, co_localsplusnames), \
        .localspluskinds = offsetof(PyCodeObject, co_localspluskinds), \
        .co_code_adaptive = offsetof(PyCodeObject, co_code_adaptive), \
    }, \
    .pyobject = { \
        .size = sizeof(PyObject), \
        .ob_type = offsetof(PyObject, ob_type), \
    }, \
    .type_object = { \
        .size = sizeof(PyTypeObject), \
        .tp_name = offsetof(PyTypeObject, tp_name), \
        .tp_repr = offsetof(PyTypeObject, tp_repr), \
        .tp_flags = offsetof(PyTypeObject, tp_flags), \
    }, \
    .tuple_object = { \
        .size = sizeof(PyTupleObject), \
        .ob_item = offsetof(PyTupleObject, ob_item), \
        .ob_size = offsetof(PyVarObject, ob_size), \
    }, \
    .list_object = { \
        .size = sizeof(PyListObject), \
        .ob_item = offsetof(PyListObject, ob_item), \
        .ob_size = offsetof(PyVarObject, ob_size), \
    }, \
    .dict_object = { \
        .size = sizeof(PyDictObject), \
        .ma_keys = offsetof(PyDictObject, ma_keys), \
        .ma_values = offsetof(PyDictObject, ma_values), \
    }, \
    .float_object = { \
        .size = sizeof(PyFloatObject), \
        .ob_fval = offsetof(PyFloatObject, ob_fval), \
    }, \
    .long_object = { \
        .size = sizeof(PyLongObject), \
        .lv_tag = offsetof(PyLongObject, long_value.lv_tag), \
        .ob_digit = offsetof(PyLongObject, long_value.ob_digit), \
    }, \
    .bytes_object = { \
        .size = sizeof(PyBytesObject), \
        .ob_size = offsetof(PyVarObject, ob_size), \
        .ob_sval = offsetof(PyBytesObject, ob_sval), \
    }, \
    .unicode_object = { \
        .size = sizeof(PyUnicodeObject), \
        .state = offsetof(PyUnicodeObject, _base._base.state), \
        .length = offsetof(PyUnicodeObject, _base._base.length), \
        .asciiobject_size = sizeof(PyASCIIObject), \
    }, \
    .gc = { \
        .size = sizeof(struct _gc_runtime_state), \
        .collecting = offsetof(struct _gc_runtime_state, collecting), \
        .frame = offsetof(struct _gc_runtime_state, frame), \
    }, \
    .gen_object = { \
        .size = sizeof(PyGenObject), \
        .gi_name = offsetof(PyGenObject, gi_name), \
        .gi_iframe = offsetof(PyGenObject, gi_iframe), \
        .gi_frame_state = offsetof(PyGenObject, gi_frame_state), \
    }, \
    .debugger_support = { \
        .eval_breaker = offsetof(PyInterpreterState, ceval) + offsetof(struct _ceval_state, pending) + offsetof(struct _pending_calls, calls_to_do), \
        .remote_debugger_support = offsetof(PyThreadState, remote_debugger_support), \
        .remote_debugging_enabled = offsetof(PyInterpreterState, remote_debugging_enabled), \
        .debugger_pending_call = offsetof(_PyRemoteDebuggerSupport, debugger_pending_call), \
        .debugger_script_path = offsetof(_PyRemoteDebuggerSupport, debugger_script_path), \
        .debugger_script_path_size = _Py_MAX_SCRIPT_PATH_SIZE, \
    }, \
}


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_DEBUG_OFFSETS_H */
