/*[clinic input]
preserve
[clinic start generated code]*/

static PyObject *
tokenizeriter_new_impl(PyTypeObject *type, PyObject *readline,
                       const char *encoding);

static PyObject *
tokenizeriter_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    PyObject *readline;
    const char *encoding;

    if ((type == clinic_find_state()->TokenizerIter) &&
        !_PyArg_NoKeywords("tokenizeriter", kwargs)) {
        goto exit;
    }
    if (!_PyArg_CheckPositional("tokenizeriter", PyTuple_GET_SIZE(args), 2, 2)) {
        goto exit;
    }
    readline = PyTuple_GET_ITEM(args, 0);
    if (!PyUnicode_Check(PyTuple_GET_ITEM(args, 1))) {
        _PyArg_BadArgument("tokenizeriter", "argument 2", "str", PyTuple_GET_ITEM(args, 1));
        goto exit;
    }
    Py_ssize_t encoding_length;
    encoding = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(args, 1), &encoding_length);
    if (encoding == NULL) {
        goto exit;
    }
    if (strlen(encoding) != (size_t)encoding_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    return_value = tokenizeriter_new_impl(type, readline, encoding);

exit:
    return return_value;
}
/*[clinic end generated code: output=514be973d9f91db1 input=a9049054013a1b77]*/
