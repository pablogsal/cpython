
#define _PY_INTERPRETER

#include "Python.h"
#include "pycore_compile.h"       // _PyCompile_GetUnaryIntrinsicName
#include "pycore_function.h"      // _Py_set_function_type_params()
#include "pycore_genobject.h"     // _PyAsyncGenValueWrapperNew
#include "pycore_interpframe.h"   // _PyFrame_GetLocals()
#include "pycore_intrinsics.h"    // INTRINSIC_PRINT
#include "pycore_list.h"          // _PyList_AsTupleAndClear()
#include "pycore_object.h"        // _PyObject_IsUniquelyReferenced()
#include "pycore_setobject.h"     // _PySet_Freeze()
#include "pycore_pyerrors.h"      // _PyErr_SetString()
#include "pycore_runtime.h"       // _Py_ID()
#include "pycore_typevarobject.h" // _Py_make_typevar()
#include "pycore_unicodeobject.h" // _PyUnicode_FromASCII()


/******** Unary functions ********/

static PyObject *
no_intrinsic1(PyThreadState* tstate, PyObject *unused)
{
    _PyErr_SetString(tstate, PyExc_SystemError, "invalid intrinsic function");
    return NULL;
}

static PyObject *
print_expr(PyThreadState* Py_UNUSED(ignored), PyObject *value)
{
    PyObject *hook = PySys_GetAttr(&_Py_ID(displayhook));
    if (hook == NULL) {
        return NULL;
    }
    PyObject *res = PyObject_CallOneArg(hook, value);
    Py_DECREF(hook);
    return res;
}

static int
import_all_from(PyThreadState *tstate, PyObject *locals, PyObject *v)
{
    PyObject *all, *dict, *name, *value;
    int skip_leading_underscores = 0;
    int pos, err;

    if (PyObject_GetOptionalAttr(v, &_Py_ID(__all__), &all) < 0) {
        return -1; /* Unexpected error */
    }
    if (all == NULL) {
        if (PyObject_GetOptionalAttr(v, &_Py_ID(__dict__), &dict) < 0) {
            return -1;
        }
        if (dict == NULL) {
            _PyErr_SetString(tstate, PyExc_ImportError,
                    "from-import-* object has no __dict__ and no __all__");
            return -1;
        }
        all = PyMapping_Keys(dict);
        Py_DECREF(dict);
        if (all == NULL)
            return -1;
        skip_leading_underscores = 1;
    }

    for (pos = 0, err = 0; ; pos++) {
        name = PySequence_GetItem(all, pos);
        if (name == NULL) {
            if (!_PyErr_ExceptionMatches(tstate, PyExc_IndexError)) {
                err = -1;
            }
            else {
                _PyErr_Clear(tstate);
            }
            break;
        }
        if (!PyUnicode_Check(name)) {
            PyObject *modname = PyObject_GetAttr(v, &_Py_ID(__name__));
            if (modname == NULL) {
                Py_DECREF(name);
                err = -1;
                break;
            }
            if (!PyUnicode_Check(modname)) {
                _PyErr_Format(tstate, PyExc_TypeError,
                              "module __name__ must be a string, not %.100s",
                              Py_TYPE(modname)->tp_name);
            }
            else {
                _PyErr_Format(tstate, PyExc_TypeError,
                              "%s in %U.%s must be str, not %.100s",
                              skip_leading_underscores ? "Key" : "Item",
                              modname,
                              skip_leading_underscores ? "__dict__" : "__all__",
                              Py_TYPE(name)->tp_name);
            }
            Py_DECREF(modname);
            Py_DECREF(name);
            err = -1;
            break;
        }
        if (skip_leading_underscores) {
            if (PyUnicode_READ_CHAR(name, 0) == '_') {
                Py_DECREF(name);
                continue;
            }
        }
        value = PyObject_GetAttr(v, name);
        if (value == NULL)
            err = -1;
        else if (PyDict_CheckExact(locals))
            err = PyDict_SetItem(locals, name, value);
        else
            err = PyObject_SetItem(locals, name, value);
        Py_DECREF(name);
        Py_XDECREF(value);
        if (err < 0)
            break;
    }
    Py_DECREF(all);
    return err;
}

static PyObject *
import_star(PyThreadState* tstate, PyObject *from)
{
    _PyInterpreterFrame *frame = tstate->current_frame;

    PyObject *locals = _PyFrame_GetLocals(frame);
    if (locals == NULL) {
        _PyErr_SetString(tstate, PyExc_SystemError,
                            "no locals found during 'import *'");
        return NULL;
    }
    int err = import_all_from(tstate, locals, from);
    Py_DECREF(locals);
    if (err < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
stopiteration_error(PyThreadState* tstate, PyObject *exc)
{
    _PyInterpreterFrame *frame = tstate->current_frame;
    assert(frame->owner == FRAME_OWNED_BY_GENERATOR);
    assert(PyExceptionInstance_Check(exc));
    const char *msg = NULL;
    if (PyErr_GivenExceptionMatches(exc, PyExc_StopIteration)) {
        msg = "generator raised StopIteration";
        if (_PyFrame_GetCode(frame)->co_flags & CO_ASYNC_GENERATOR) {
            msg = "async generator raised StopIteration";
        }
        else if (_PyFrame_GetCode(frame)->co_flags & CO_COROUTINE) {
            msg = "coroutine raised StopIteration";
        }
    }
    else if ((_PyFrame_GetCode(frame)->co_flags & CO_ASYNC_GENERATOR) &&
            PyErr_GivenExceptionMatches(exc, PyExc_StopAsyncIteration))
    {
        /* code in `gen` raised a StopAsyncIteration error:
        raise a RuntimeError.
        */
        msg = "async generator raised StopAsyncIteration";
    }
    if (msg != NULL) {
        PyObject *message = _PyUnicode_FromASCII(msg, strlen(msg));
        if (message == NULL) {
            return NULL;
        }
        PyObject *error = PyObject_CallOneArg(PyExc_RuntimeError, message);
        if (error == NULL) {
            Py_DECREF(message);
            return NULL;
        }
        assert(PyExceptionInstance_Check(error));
        PyException_SetCause(error, Py_NewRef(exc));
        // Steal exc reference, rather than Py_NewRef+Py_DECREF
        PyException_SetContext(error, Py_NewRef(exc));
        Py_DECREF(message);
        return error;
    }
    return Py_NewRef(exc);
}

static PyObject *
unary_pos(PyThreadState* unused, PyObject *value)
{
    return PyNumber_Positive(value);
}

static PyObject *
list_to_tuple(PyThreadState* unused, PyObject *v)
{
    /* INTRINSIC_LIST_TO_TUPLE is only emitted by the compiler for a
       freshly-built, uniquely-referenced temporary list, so steal its items
       into the tuple instead of copying them. */
    assert(PyList_CheckExact(v));
    assert(_PyObject_IsUniquelyReferenced(v));
    return _PyList_AsTupleAndClear((PyListObject *)v);
}

static PyObject *
make_typevar(PyThreadState* Py_UNUSED(ignored), PyObject *v)
{
    assert(PyUnicode_Check(v));
    return _Py_make_typevar(v, NULL, NULL);
}

static PyObject *
make_frozenset(PyThreadState* Py_UNUSED(ignored), PyObject *set)
{
    assert(PySet_CheckExact(set));
    assert(_PyObject_IsUniquelyReferenced(set));
    return _PySet_Freeze(set);
}


#define INTRINSIC_FUNC_ENTRY(N, F) \
    [N] = {F, #N},

const intrinsic_func1_info
_PyIntrinsics_UnaryFunctions[] = {
    INTRINSIC_FUNC_ENTRY(INTRINSIC_1_INVALID, no_intrinsic1)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_PRINT, print_expr)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_IMPORT_STAR, import_star)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_STOPITERATION_ERROR, stopiteration_error)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_ASYNC_GEN_WRAP, _PyAsyncGenValueWrapperNew)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_UNARY_POSITIVE, unary_pos)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_LIST_TO_TUPLE, list_to_tuple)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_TYPEVAR, make_typevar)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_PARAMSPEC, _Py_make_paramspec)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_TYPEVARTUPLE, _Py_make_typevartuple)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_SUBSCRIPT_GENERIC, _Py_subscript_generic)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_TYPEALIAS, _Py_make_typealias)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_BUILD_FROZENSET, make_frozenset)
};


/******** Binary functions ********/

static PyObject *
no_intrinsic2(PyThreadState* tstate, PyObject *unused1, PyObject *unused2)
{
    _PyErr_SetString(tstate, PyExc_SystemError, "invalid intrinsic function");
    return NULL;
}

static PyObject *
prep_reraise_star(PyThreadState* unused, PyObject *orig, PyObject *excs)
{
    assert(PyList_Check(excs));
    return _PyExc_PrepReraiseStar(orig, excs);
}

static PyObject *
make_typevar_with_bound(PyThreadState* Py_UNUSED(ignored), PyObject *name,
                        PyObject *evaluate_bound)
{
    assert(PyUnicode_Check(name));
    return _Py_make_typevar(name, evaluate_bound, NULL);
}

static PyObject *
make_typevar_with_constraints(PyThreadState* Py_UNUSED(ignored), PyObject *name,
                              PyObject *evaluate_constraints)
{
    assert(PyUnicode_Check(name));
    return _Py_make_typevar(name, NULL, evaluate_constraints);
}

static PyObject *
add_conditional_annotation(PyThreadState* tstate, PyObject *conditional_annotations,
                           PyObject *index)
{
    // gh-154902: user code can rebind __conditional_annotations__ to any object
    if (!PySet_CheckExact(conditional_annotations)) {
        _PyErr_Format(tstate, PyExc_TypeError,
                      "__conditional_annotations__ must be a set, not %T",
                      conditional_annotations);
        return NULL;
    }
    if (PySet_Add(conditional_annotations, index) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
match_class_isinstance(PyThreadState* tstate, PyObject *subject, PyObject *type)
{
    /* Fast path for class patterns with no sub-patterns, e.g. `case C():`
       Equivalent to the isinstance check performed by _PyEval_MatchClass,
       including the same TypeError when the pattern does not refer to a
       class. */
    if (!PyType_Check(type)) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "class pattern must refer to a class");
        return NULL;
    }
    int res = PyObject_IsInstance(subject, type);
    if (res < 0) {
        return NULL;
    }
    return res ? Py_True : Py_False;
}

/* Look up callbacks only if a consumer imported assertionlib. Failures keep
 * their private payload even when no Python callback is installed. */
static PyObject *
assertion_dispatcher(int passed)
{
    PyObject *module = PyImport_GetModule(&_Py_ID(assertionlib));
    if (module == NULL) {
        return PyErr_Occurred() ? NULL : Py_NewRef(Py_None);
    }
    PyObject *var = NULL, *handlers = NULL, *dispatch = NULL;
    int found = PyObject_GetOptionalAttr(module, &_Py_ID(_handlers), &var);
    if (found == 0) {
        dispatch = Py_NewRef(Py_None);
    }
    else if (found > 0 && PyContextVar_CheckExact(var) &&
             PyContextVar_Get(var, Py_None, &handlers) == 0) {
        if (handlers == Py_None) {
            dispatch = Py_NewRef(Py_None);
        }
        else if (PyTuple_CheckExact(handlers) && PyTuple_GET_SIZE(handlers) == 2) {
            if (PyTuple_GET_ITEM(handlers, passed) == Py_None) {
                dispatch = Py_NewRef(Py_None);
            }
            else {
                dispatch = PyObject_GetAttr(module, &_Py_ID(_dispatch));
            }
        }
    }
    if (dispatch == NULL && !PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "invalid assertion callback state");
    }
    Py_XDECREF(var);
    Py_XDECREF(handlers);
    Py_DECREF(module);
    return dispatch;
}

static PyObject *
assert_details(PyThreadState *tstate, PyObject *exc, PyObject *plan)
{
    int passed = exc == Py_None;
    _PyInterpreterFrame *frame = tstate->current_frame;
    if ((!passed && !PyExceptionInstance_Check(exc)) ||
        !PyTuple_CheckExact(plan) || PyTuple_GET_SIZE(plan) != 3 ||
        !PyTuple_CheckExact(PyTuple_GET_ITEM(plan, 1)) ||
        !PyTuple_CheckExact(PyTuple_GET_ITEM(plan, 2))) {
        PyErr_SetString(PyExc_SystemError, "invalid assertion plan");
        return NULL;
    }
    PyObject *dispatch = assertion_dispatcher(passed);
    if (dispatch == NULL) {
        return NULL;
    }
    if (passed && dispatch == Py_None) {
        return dispatch;
    }
    PyObject *records = PyTuple_GET_ITEM(plan, 1);
    Py_ssize_t n = PyTuple_GET_SIZE(records);
    PyObject *values = PyTuple_New(n);
    PyObject *details = NULL, *result = NULL;
    if (values == NULL) {
        goto done;
    }
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *record = PyTuple_GET_ITEM(records, i);
        if (!PyTuple_CheckExact(record) || PyTuple_GET_SIZE(record) != 6) {
            PyErr_SetString(PyExc_SystemError, "invalid assertion record");
            goto done;
        }
        Py_ssize_t slot = PyLong_AsSsize_t(PyTuple_GET_ITEM(record, 0));
        if (slot < 0 || slot >= _PyFrame_GetCode(frame)->co_nlocals) {
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_SystemError, "invalid assertion slot");
            }
            goto done;
        }
        if (i > 0 && slot == PyLong_AsSsize_t(
                PyTuple_GET_ITEM(PyTuple_GET_ITEM(records, i - 1), 0))) {
            PyTuple_SET_ITEM(values, i, Py_NewRef(PyTuple_GET_ITEM(values, i - 1)));
            continue;
        }
        _PyStackRef ref = frame->localsplus[slot];
        PyObject *value = PyStackRef_IsNull(ref)
            ? PyTuple_New(0)
            : PyTuple_Pack(1, PyStackRef_AsPyObjectBorrow(ref));
        if (value == NULL) {
            goto done;
        }
        PyTuple_SET_ITEM(values, i, value);
    }
    details = PyTuple_Pack(6, PyTuple_GET_ITEM(plan, 0), records, values,
                          PyTuple_GET_ITEM(plan, 2),
                          _PyFrame_GetCode(frame)->co_filename,
                          passed ? Py_True : Py_False);
    if (details == NULL) {
        goto done;
    }
    if (!passed && PyObject_SetAttrString(exc, "_assertion", details) < 0) {
        goto done;
    }
    if (dispatch != Py_None) {
        PyObject *previous = NULL;
        if (!passed) {
            /* Establish ordinary exception chaining before a callback runs. */
            _PyErr_SetObject(tstate, (PyObject *)Py_TYPE(exc), exc);
            PyObject *raised = _PyErr_GetRaisedException(tstate);
            Py_DECREF(raised);
            previous = PyErr_GetHandledException();
            PyErr_SetHandledException(exc);
        }
        PyObject *res = PyObject_CallFunctionObjArgs(dispatch, exc, details, NULL);
        if (!passed) {
            PyErr_SetHandledException(previous);
            Py_XDECREF(previous);
        }
        if (res == NULL) {
            goto done;
        }
        Py_DECREF(res);
    }
    result = Py_NewRef(exc);
 done:
    Py_XDECREF(details);
    Py_XDECREF(values);
    Py_DECREF(dispatch);
    return result;
}

const intrinsic_func2_info
_PyIntrinsics_BinaryFunctions[] = {
    INTRINSIC_FUNC_ENTRY(INTRINSIC_2_INVALID, no_intrinsic2)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_PREP_RERAISE_STAR, prep_reraise_star)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_TYPEVAR_WITH_BOUND, make_typevar_with_bound)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_TYPEVAR_WITH_CONSTRAINTS, make_typevar_with_constraints)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_SET_FUNCTION_TYPE_PARAMS, _Py_set_function_type_params)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_SET_TYPEPARAM_DEFAULT, _Py_set_typeparam_default)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_ADD_CONDITIONAL_ANNOTATION, add_conditional_annotation)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_MATCH_CLASS_ISINSTANCE, match_class_isinstance)
    INTRINSIC_FUNC_ENTRY(INTRINSIC_ASSERT_DETAILS, assert_details)
};

#undef INTRINSIC_FUNC_ENTRY

PyObject*
_PyCompile_GetUnaryIntrinsicName(int index)
{
    if (index < 0 || index > MAX_INTRINSIC_1) {
        return NULL;
    }
    return PyUnicode_FromString(_PyIntrinsics_UnaryFunctions[index].name);
}

PyObject*
_PyCompile_GetBinaryIntrinsicName(int index)
{
    if (index < 0 || index > MAX_INTRINSIC_2) {
        return NULL;
    }
    return PyUnicode_FromString(_PyIntrinsics_BinaryFunctions[index].name);
}
