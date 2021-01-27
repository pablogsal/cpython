/*[clinic input]
preserve
[clinic start generated code]*/

static PyObject *
tokenizeriter_new_impl(PyTypeObject *type, PyObject *readline);

static PyObject *
tokenizeriter_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    PyObject *readline;

    if ((type == clinic_find_state()->TokenizerIter) &&
        !_PyArg_NoKeywords("tokenizeriter", kwargs)) {
        goto exit;
    }
    if (!_PyArg_CheckPositional("tokenizeriter", PyTuple_GET_SIZE(args), 1, 1)) {
        goto exit;
    }
    readline = PyTuple_GET_ITEM(args, 0);
    return_value = tokenizeriter_new_impl(type, readline);

exit:
    return return_value;
}
/*[clinic end generated code: output=d39343777e09f58a input=a9049054013a1b77]*/
