/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

PyDoc_STRVAR(activate_debugger_interface__doc__,
"activate_debugger_interface($module, pid, /, tid=0,\n"
"                            debugger_script_path=<unrepresentable>)\n"
"--\n"
"\n"
"Return number of occurrences of v in the array.");

#define ACTIVATE_DEBUGGER_INTERFACE_METHODDEF    \
    {"activate_debugger_interface", _PyCFunction_CAST(activate_debugger_interface), METH_FASTCALL|METH_KEYWORDS, activate_debugger_interface__doc__},

static PyObject *
activate_debugger_interface_impl(PyObject *module, int pid, int tid,
                                 const char *debugger_script_path);

static PyObject *
activate_debugger_interface(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 2
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(tid), &_Py_ID(debugger_script_path), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"", "tid", "debugger_script_path", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "activate_debugger_interface",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[3];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    int pid;
    int tid = 0;
    const char *debugger_script_path = NULL;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 3, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    pid = PyLong_AsInt(args[0]);
    if (pid == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[1]) {
        tid = PyLong_AsInt(args[1]);
        if (tid == -1 && PyErr_Occurred()) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    if (!PyUnicode_Check(args[2])) {
        _PyArg_BadArgument("activate_debugger_interface", "argument 'debugger_script_path'", "str", args[2]);
        goto exit;
    }
    Py_ssize_t debugger_script_path_length;
    debugger_script_path = PyUnicode_AsUTF8AndSize(args[2], &debugger_script_path_length);
    if (debugger_script_path == NULL) {
        goto exit;
    }
    if (strlen(debugger_script_path) != (size_t)debugger_script_path_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
skip_optional_pos:
    return_value = activate_debugger_interface_impl(module, pid, tid, debugger_script_path);

exit:
    return return_value;
}
/*[clinic end generated code: output=73fc0b8703b2f35d input=a9049054013a1b77]*/
