/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_critical_section.h"// Py_BEGIN_CRITICAL_SECTION()
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

PyDoc_STRVAR(_remote_debugging_get_all_awaited_by__doc__,
"get_all_awaited_by($module, /, pid)\n"
"--\n"
"\n"
"Get all tasks and their awaited_by from a given pid");

#define _REMOTE_DEBUGGING_GET_ALL_AWAITED_BY_METHODDEF    \
    {"get_all_awaited_by", _PyCFunction_CAST(_remote_debugging_get_all_awaited_by), METH_FASTCALL|METH_KEYWORDS, _remote_debugging_get_all_awaited_by__doc__},

static PyObject *
_remote_debugging_get_all_awaited_by_impl(PyObject *module, int pid);

static PyObject *
_remote_debugging_get_all_awaited_by(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(pid), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"pid", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "get_all_awaited_by",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    int pid;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    pid = PyLong_AsInt(args[0]);
    if (pid == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = _remote_debugging_get_all_awaited_by_impl(module, pid);

exit:
    return return_value;
}

PyDoc_STRVAR(_remote_debugging_get_async_stack_trace__doc__,
"get_async_stack_trace($module, /, pid)\n"
"--\n"
"\n"
"Get the asyncio stack from a given pid");

#define _REMOTE_DEBUGGING_GET_ASYNC_STACK_TRACE_METHODDEF    \
    {"get_async_stack_trace", _PyCFunction_CAST(_remote_debugging_get_async_stack_trace), METH_FASTCALL|METH_KEYWORDS, _remote_debugging_get_async_stack_trace__doc__},

static PyObject *
_remote_debugging_get_async_stack_trace_impl(PyObject *module, int pid);

static PyObject *
_remote_debugging_get_async_stack_trace(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(pid), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"pid", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "get_async_stack_trace",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    int pid;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    pid = PyLong_AsInt(args[0]);
    if (pid == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = _remote_debugging_get_async_stack_trace_impl(module, pid);

exit:
    return return_value;
}

PyDoc_STRVAR(_remote_debugging_RemoteUnwinder___init____doc__,
"RemoteUnwinder(pid, *, all_threads=False)\n"
"--\n"
"\n"
"Something");

static int
_remote_debugging_RemoteUnwinder___init___impl(RemoteUnwinderObject *self,
                                               int pid, int all_threads);

static int
_remote_debugging_RemoteUnwinder___init__(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int return_value = -1;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 2
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(pid), &_Py_ID(all_threads), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"pid", "all_threads", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "RemoteUnwinder",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[2];
    PyObject * const *fastargs;
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    Py_ssize_t noptargs = nargs + (kwargs ? PyDict_GET_SIZE(kwargs) : 0) - 1;
    int pid;
    int all_threads = 0;

    fastargs = _PyArg_UnpackKeywords(_PyTuple_CAST(args)->ob_item, nargs, kwargs, NULL, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!fastargs) {
        goto exit;
    }
    pid = PyLong_AsInt(fastargs[0]);
    if (pid == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    all_threads = PyObject_IsTrue(fastargs[1]);
    if (all_threads < 0) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = _remote_debugging_RemoteUnwinder___init___impl((RemoteUnwinderObject *)self, pid, all_threads);

exit:
    return return_value;
}

PyDoc_STRVAR(_remote_debugging_RemoteUnwinder_get_stack_trace__doc__,
"get_stack_trace($self, /)\n"
"--\n"
"\n"
"Blah blah blah");

#define _REMOTE_DEBUGGING_REMOTEUNWINDER_GET_STACK_TRACE_METHODDEF    \
    {"get_stack_trace", (PyCFunction)_remote_debugging_RemoteUnwinder_get_stack_trace, METH_NOARGS, _remote_debugging_RemoteUnwinder_get_stack_trace__doc__},

static PyObject *
_remote_debugging_RemoteUnwinder_get_stack_trace_impl(RemoteUnwinderObject *self);

static PyObject *
_remote_debugging_RemoteUnwinder_get_stack_trace(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;

    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _remote_debugging_RemoteUnwinder_get_stack_trace_impl((RemoteUnwinderObject *)self);
    Py_END_CRITICAL_SECTION();

    return return_value;
}

PyDoc_STRVAR(_remote_debugging_RemoteUnwinder_get_all_awaited_by__doc__,
"get_all_awaited_by($self, /)\n"
"--\n"
"\n"
"Get all tasks and their awaited_by from the remote process");

#define _REMOTE_DEBUGGING_REMOTEUNWINDER_GET_ALL_AWAITED_BY_METHODDEF    \
    {"get_all_awaited_by", (PyCFunction)_remote_debugging_RemoteUnwinder_get_all_awaited_by, METH_NOARGS, _remote_debugging_RemoteUnwinder_get_all_awaited_by__doc__},

static PyObject *
_remote_debugging_RemoteUnwinder_get_all_awaited_by_impl(RemoteUnwinderObject *self);

static PyObject *
_remote_debugging_RemoteUnwinder_get_all_awaited_by(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;

    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _remote_debugging_RemoteUnwinder_get_all_awaited_by_impl((RemoteUnwinderObject *)self);
    Py_END_CRITICAL_SECTION();

    return return_value;
}

PyDoc_STRVAR(_remote_debugging_RemoteUnwinder_get_async_stack_trace__doc__,
"get_async_stack_trace($self, /)\n"
"--\n"
"\n"
"Get the asyncio stack from the remote process");

#define _REMOTE_DEBUGGING_REMOTEUNWINDER_GET_ASYNC_STACK_TRACE_METHODDEF    \
    {"get_async_stack_trace", (PyCFunction)_remote_debugging_RemoteUnwinder_get_async_stack_trace, METH_NOARGS, _remote_debugging_RemoteUnwinder_get_async_stack_trace__doc__},

static PyObject *
_remote_debugging_RemoteUnwinder_get_async_stack_trace_impl(RemoteUnwinderObject *self);

static PyObject *
_remote_debugging_RemoteUnwinder_get_async_stack_trace(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;

    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _remote_debugging_RemoteUnwinder_get_async_stack_trace_impl((RemoteUnwinderObject *)self);
    Py_END_CRITICAL_SECTION();

    return return_value;
}

PyDoc_STRVAR(_remote_debugging_BinaryStackDumper___init____doc__,
"BinaryStackDumper(filename, pid, *, all_threads=False)\n"
"--\n"
"\n"
"Initialize binary stack dumper with filename and PID");

static int
_remote_debugging_BinaryStackDumper___init___impl(BinaryStackDumperObject *self,
                                                  const char *filename,
                                                  int pid, int all_threads);

static int
_remote_debugging_BinaryStackDumper___init__(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int return_value = -1;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 3
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(filename), &_Py_ID(pid), &_Py_ID(all_threads), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"filename", "pid", "all_threads", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "BinaryStackDumper",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[3];
    PyObject * const *fastargs;
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    Py_ssize_t noptargs = nargs + (kwargs ? PyDict_GET_SIZE(kwargs) : 0) - 2;
    const char *filename;
    int pid;
    int all_threads = 0;

    fastargs = _PyArg_UnpackKeywords(_PyTuple_CAST(args)->ob_item, nargs, kwargs, NULL, &_parser,
            /*minpos*/ 2, /*maxpos*/ 2, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!fastargs) {
        goto exit;
    }
    if (!PyUnicode_Check(fastargs[0])) {
        _PyArg_BadArgument("BinaryStackDumper", "argument 'filename'", "str", fastargs[0]);
        goto exit;
    }
    Py_ssize_t filename_length;
    filename = PyUnicode_AsUTF8AndSize(fastargs[0], &filename_length);
    if (filename == NULL) {
        goto exit;
    }
    if (strlen(filename) != (size_t)filename_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    pid = PyLong_AsInt(fastargs[1]);
    if (pid == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    all_threads = PyObject_IsTrue(fastargs[2]);
    if (all_threads < 0) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = _remote_debugging_BinaryStackDumper___init___impl((BinaryStackDumperObject *)self, filename, pid, all_threads);

exit:
    return return_value;
}

PyDoc_STRVAR(_remote_debugging_BinaryStackDumper_dump_thread__doc__,
"dump_thread($self, /)\n"
"--\n"
"\n"
"Dump current stack trace state from the initialized PID");

#define _REMOTE_DEBUGGING_BINARYSTACKDUMPER_DUMP_THREAD_METHODDEF    \
    {"dump_thread", (PyCFunction)_remote_debugging_BinaryStackDumper_dump_thread, METH_NOARGS, _remote_debugging_BinaryStackDumper_dump_thread__doc__},

static PyObject *
_remote_debugging_BinaryStackDumper_dump_thread_impl(BinaryStackDumperObject *self);

static PyObject *
_remote_debugging_BinaryStackDumper_dump_thread(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;

    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _remote_debugging_BinaryStackDumper_dump_thread_impl((BinaryStackDumperObject *)self);
    Py_END_CRITICAL_SECTION();

    return return_value;
}

PyDoc_STRVAR(_remote_debugging_BinaryStackDumper_close__doc__,
"close($self, /)\n"
"--\n"
"\n"
"Close the dumper and finalize the file");

#define _REMOTE_DEBUGGING_BINARYSTACKDUMPER_CLOSE_METHODDEF    \
    {"close", (PyCFunction)_remote_debugging_BinaryStackDumper_close, METH_NOARGS, _remote_debugging_BinaryStackDumper_close__doc__},

static PyObject *
_remote_debugging_BinaryStackDumper_close_impl(BinaryStackDumperObject *self);

static PyObject *
_remote_debugging_BinaryStackDumper_close(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;

    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _remote_debugging_BinaryStackDumper_close_impl((BinaryStackDumperObject *)self);
    Py_END_CRITICAL_SECTION();

    return return_value;
}

PyDoc_STRVAR(_remote_debugging_load_stack_trace_binary__doc__,
"load_stack_trace_binary($module, /, filename)\n"
"--\n"
"\n"
"Load stack trace from binary format");

#define _REMOTE_DEBUGGING_LOAD_STACK_TRACE_BINARY_METHODDEF    \
    {"load_stack_trace_binary", _PyCFunction_CAST(_remote_debugging_load_stack_trace_binary), METH_FASTCALL|METH_KEYWORDS, _remote_debugging_load_stack_trace_binary__doc__},

static PyObject *
_remote_debugging_load_stack_trace_binary_impl(PyObject *module,
                                               const char *filename);

static PyObject *
_remote_debugging_load_stack_trace_binary(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(filename), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"filename", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "load_stack_trace_binary",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    const char *filename;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("load_stack_trace_binary", "argument 'filename'", "str", args[0]);
        goto exit;
    }
    Py_ssize_t filename_length;
    filename = PyUnicode_AsUTF8AndSize(args[0], &filename_length);
    if (filename == NULL) {
        goto exit;
    }
    if (strlen(filename) != (size_t)filename_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    return_value = _remote_debugging_load_stack_trace_binary_impl(module, filename);

exit:
    return return_value;
}
/*[clinic end generated code: output=282e1afcd910a3b9 input=a9049054013a1b77]*/
