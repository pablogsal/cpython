/*
 * _parsebench: C extension module that benchmarks CPython's internal PEG parser.
 *
 * Calls _PyParser_ASTFromString() which produces mod_ty (C struct),
 * WITHOUT creating Python AST objects. This measures the raw C-level
 * parsing cost only.
 */

#define Py_BUILD_CORE

#include "Python.h"
#include "pycore_parser.h"
#include "pycore_pyarena.h"

#include <time.h>

/*
 * parse_raw(source: str) -> (time_us: int, has_error: bool)
 *
 * Parse source as a module, returning (elapsed_microseconds, had_error).
 * Only measures the C-level parse (mod_ty creation), not Python AST objects.
 */
static PyObject *
parsebench_parse_raw(PyObject *self, PyObject *args)
{
    const char *source;
    Py_ssize_t source_len;

    if (!PyArg_ParseTuple(args, "s#", &source, &source_len))
        return NULL;

    PyObject *filename = PyUnicode_FromString("<bench>");
    if (filename == NULL)
        return NULL;

    PyCompilerFlags cf = _PyCompilerFlags_INIT;
    cf.cf_flags = PyCF_SOURCE_IS_UTF8 | PyCF_IGNORE_COOKIE;

    PyArena *arena = _PyArena_New();
    if (arena == NULL) {
        Py_DECREF(filename);
        return NULL;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    mod_ty mod = _PyParser_ASTFromString(source, filename, Py_file_input,
                                          &cf, arena);

    clock_gettime(CLOCK_MONOTONIC, &end);

    int has_error = (mod == NULL);
    if (has_error) {
        /* Clear the exception — we just want to record the error */
        PyErr_Clear();
    }

    _PyArena_Free(arena);
    Py_DECREF(filename);

    long long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL
                         + (end.tv_nsec - start.tv_nsec);
    long long elapsed_us = elapsed_ns / 1000;

    return Py_BuildValue("(Li)", elapsed_us, has_error);
}

static PyMethodDef parsebench_methods[] = {
    {"parse_raw", parsebench_parse_raw, METH_VARARGS,
     "Parse source at C level (mod_ty only), return (time_us, has_error)."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef parsebench_module = {
    PyModuleDef_HEAD_INIT,
    "_parsebench",
    "Benchmark CPython's internal PEG parser at the C struct level.",
    -1,
    parsebench_methods
};

PyMODINIT_FUNC
PyInit__parsebench(void)
{
    return PyModule_Create(&parsebench_module);
}
