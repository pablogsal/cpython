/* Assertion hook support.
 *
 * The Python compiler emits, on assert failure, a call to the
 * INTRINSIC_FORMAT_ASSERT intrinsic which delegates to
 * `_PyAssertion_FormatFromTuple` below.  The intrinsic receives a single
 * tuple `(source_strs, values, msg, expr_source)` where:
 *
 *   - source_strs : tuple of unparsed source representations of each
 *                   captured sub-expression of the assert test
 *                   (built at compile time by ast_preprocess.c).
 *   - values      : tuple of the runtime values of each captured
 *                   sub-expression.  Codegen captures these *inline*
 *                   while evaluating the test (single-pass): they are
 *                   the same evaluations the test itself ran, kept on
 *                   the operand stack for the failure path.  Sub-
 *                   expressions are never re-executed.
 *   - msg         : the user-provided `assert expr, msg` value, or None.
 *   - expr_source : the unparsed source of the whole test expression
 *                   (a string), so the hook can render `assert <expr>`.
 *
 * The function looks up `sys.__assertion_hook__`.  If the slot is set to a
 * callable, the hook is invoked with `(source_strs, values, msg,
 * expr_source)` and is expected to return a string used as the message of
 * the `AssertionError` instance that is raised next.  If the slot is None
 * (the default), the original user-provided `msg` (or an empty string) is
 * returned, preserving the standard `assert` semantics.
 */

#include "Python.h"
#include "pycore_pyerrors.h"      // _PyErr_SetString
#include "pycore_runtime.h"       // _Py_ID
#include "pycore_sysmodule.h"     // PySys_GetAttr (via sysmodule.h)


/* The hook lives in sys.__assertion_hook__.  We look it up by name on
 * each call -- failed asserts are not on the fast path so the dict lookup
 * cost is acceptable, and this avoids the need for per-interpreter
 * threading of the hook slot. */
static PyObject *
get_assertion_hook(void)
{
    PyObject *name = PyUnicode_InternFromString("__assertion_hook__");
    if (name == NULL) {
        return NULL;
    }
    PyObject *hook = PySys_GetAttr(name);
    Py_DECREF(name);
    return hook;  /* new ref, or NULL with exception set */
}


PyObject *
_PyAssertion_FormatFromTuple(PyThreadState *tstate, PyObject *args)
{
    if (!PyTuple_Check(args) || PyTuple_GET_SIZE(args) != 4) {
        _PyErr_SetString(tstate, PyExc_SystemError,
                         "INTRINSIC_FORMAT_ASSERT expects a 4-tuple");
        return NULL;
    }

    PyObject *source_strs = PyTuple_GET_ITEM(args, 0);
    PyObject *values      = PyTuple_GET_ITEM(args, 1);
    PyObject *msg         = PyTuple_GET_ITEM(args, 2);
    PyObject *expr_source = PyTuple_GET_ITEM(args, 3);

    PyObject *hook = get_assertion_hook();
    if (hook == NULL) {
        return NULL;
    }

    /* If the hook is unset (sys.__assertion_hook__ is None), preserve the
     * default assert semantics: just propagate the user message. */
    if (hook == Py_None) {
        Py_DECREF(hook);
        if (msg == Py_None) {
            Py_RETURN_NONE;
        }
        return Py_NewRef(msg);
    }

    PyObject *hook_args[4] = {source_strs, values, msg, expr_source};
    PyObject *result = PyObject_Vectorcall(hook, hook_args, 4, NULL);
    Py_DECREF(hook);
    if (result == NULL) {
        /* The hook itself raised.  Chain it onto an AssertionError so the
         * user sees both the hook failure and the original assert. */
        PyObject *exc = _PyErr_GetRaisedException(tstate);
        _PyErr_SetString(tstate, PyExc_AssertionError,
                         "assertion hook raised");
        _PyErr_ChainExceptions1(exc);
        return NULL;
    }
    if (result != Py_None && !PyUnicode_Check(result)) {
        Py_DECREF(result);
        _PyErr_Format(tstate, PyExc_TypeError,
                      "sys.__assertion_hook__ must return str or None, "
                      "got %T",
                      result);
        return NULL;
    }
    return result;
}
