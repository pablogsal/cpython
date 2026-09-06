#include "parts.h"

#include "../../Parser/tokenizer/source.h"

static int
check(int condition, const char *message)
{
    if (condition) {
        return 0;
    }
    PyErr_SetString(PyExc_AssertionError, message);
    return -1;
}

static int
check_system_error(int failed, const char *message)
{
    if (!failed || !PyErr_ExceptionMatches(PyExc_SystemError)) {
        PyErr_SetString(PyExc_AssertionError, message);
        return -1;
    }
    PyErr_Clear();
    return 0;
}

static int
check_line_view(const _PyTok_SourceText *source, Py_ssize_t lineno,
                const char *expected)
{
    Py_ssize_t len;
    const char *line = _PyTok_SourceLineView(source, lineno, &len);
    return check(len == (Py_ssize_t)strlen(expected) &&
                 memcmp(line, expected, len) == 0,
                 "wrong source line view");
}

static PyObject *
test_tokenizer_source(PyObject *Py_UNUSED(module),
                      PyObject *Py_UNUSED(args))
{
    _PyTok_SourceText source;
    _PyTok_SourceInit(&source);

    if (check_line_view(&source, 1, "") < 0) {
        goto error;
    }

    _PyTok_Loc loc;
    _PyTok_Line line;
    if (check(_PyTok_SourceLocation(
                  &source, 0, _PYTOK_AFFINITY_RIGHT, &loc) == 0,
              "cannot locate empty source") < 0 ||
            check(loc.lineno == 1 && loc.byte_col == 0,
                  "wrong empty source location") < 0 ||
            check(_PyTok_SourceLine(&source, 1, &line) == 0,
                  "cannot find empty source line") < 0 ||
            check(line.start == 0 && line.end == 0,
                  "wrong empty source line") < 0 ||
            check_system_error(
                _PyTok_SourceAppendLine(&source, "", 0, 0) < 0,
                "accepted empty source line") < 0 ||
            check_system_error(
                _PyTok_SourceAppendLine(&source, "a\nb\n", 4, 0) < 0,
                "accepted multiple source lines") < 0 ||
            check_system_error(
                _PyTok_SourceAppendLine(&source, "a", 1, 1) < 0,
                "accepted missing implicit newline") < 0) {
        goto error;
    }

    if (check(_PyTok_SourceAppendLine(&source, "alpha\n", 6, 0) == 0,
              "wrong first source offset") < 0 ||
            check(_PyTok_SourceAppendLine(
                      &source, "\xce\xb2\n", 3, 1) == 6,
                  "wrong second source offset") < 0) {
        goto error;
    }

    if (check_line_view(&source, PY_SSIZE_T_MIN, "alpha") < 0 ||
            check_line_view(&source, 1, "alpha") < 0 ||
            check_line_view(&source, 2, "\xce\xb2") < 0 ||
            check_line_view(&source, 3, "") < 0 ||
            check_line_view(&source, PY_SSIZE_T_MAX, "") < 0) {
        goto error;
    }

    if (check(_PyTok_SourceAppendLine(&source, "nul\0x\n", 6, 0) == 9,
              "wrong third source offset") < 0) {
        goto error;
    }

    int marker_line = 257;
    int final_line = 300;
    _PyTok_Off marker_start = -1;
    for (int lineno = 4; lineno <= final_line; lineno++) {
        const char *text = lineno == marker_line ? "marker\n" : "x\n";
        Py_ssize_t len = (Py_ssize_t)strlen(text);
        _PyTok_Off start = _PyTok_SourceAppendLine(
            &source, text, len, lineno == final_line);
        if (start < 0) {
            goto error;
        }
        if (lineno == marker_line) {
            marker_start = start;
        }
    }

    if (check(source.nlines == final_line, "wrong source line count") < 0 ||
            check(_PyTok_SourceLine(&source, marker_line, &line) == 0,
                  "cannot find late source line") < 0 ||
            check(line.start == marker_start &&
                      line.end == marker_start + 7,
                  "wrong late source line") < 0 ||
            check(!line.implicit_newline && !line.contains_nul,
                  "wrong late source flags") < 0 ||
            check(_PyTok_SourceLine(&source, 2, &line) == 0,
                  "cannot find second source line") < 0 ||
            check(line.start == 6 && line.end == 9 &&
                      line.implicit_newline && !line.contains_nul,
                  "wrong second source line") < 0 ||
            check(!_PyTok_SourceLineIsImplicit(&source, 1) &&
                      _PyTok_SourceLineIsImplicit(&source, 2),
                  "wrong early implicit newline flags") < 0 ||
            check(_PyTok_SourceLine(&source, 3, &line) == 0,
                  "cannot find third source line") < 0 ||
            check(line.contains_nul, "missing null byte flag") < 0 ||
            check(_PyTok_SourceLine(&source, final_line, &line) == 0,
                  "cannot find final source line") < 0 ||
            check(line.implicit_newline &&
                      _PyTok_SourceLineIsImplicit(&source, final_line),
                  "missing late implicit newline flag") < 0) {
        goto error;
    }

    Py_ssize_t view_len;
    const char *view = _PyTok_SourceSpanView(
        &source, _PyTok_SpanFromBounds(6, 8), &view_len);
    if (check(view != NULL && view_len == 2 &&
                  memcmp(view, "\xce\xb2", 2) == 0,
              "wrong source span view") < 0 ||
            check(_PyTok_SourceLocation(
                      &source, marker_start,
                      _PYTOK_AFFINITY_LEFT, &loc) == 0,
                  "cannot locate left line boundary") < 0 ||
            check(loc.lineno == marker_line - 1 && loc.byte_col == 2,
                  "wrong left boundary location") < 0 ||
            check(_PyTok_SourceLocation(
                      &source, marker_start,
                      _PYTOK_AFFINITY_RIGHT, &loc) == 0,
                  "cannot locate right line boundary") < 0 ||
            check(loc.lineno == marker_line && loc.byte_col == 0,
                  "wrong right boundary location") < 0 ||
            check(_PyTok_SourceLocation(
                      &source, marker_start + 1,
                      _PYTOK_AFFINITY_RIGHT, &loc) == 0,
                  "cannot locate late source byte") < 0 ||
            check(loc.lineno == marker_line && loc.byte_col == 1,
                  "wrong late source location") < 0) {
        goto error;
    }

    if (check(_PyTok_SourceLocation(
                  &source, source.len, _PYTOK_AFFINITY_LEFT, &loc) == 0,
              "cannot locate left EOF") < 0 ||
            check(loc.lineno == final_line && loc.byte_col == 2,
                  "wrong left EOF location") < 0 ||
            check(_PyTok_SourceLocation(
                      &source, source.len,
                      _PYTOK_AFFINITY_RIGHT, &loc) == 0,
                  "cannot locate right EOF") < 0 ||
            check(loc.lineno == final_line + 1 && loc.byte_col == 0,
                  "wrong right EOF location") < 0 ||
            check(_PyTok_SourceLine(&source, final_line + 1, &line) == 0,
                  "cannot find virtual EOF line") < 0 ||
            check(line.start == source.len && line.end == source.len,
                  "wrong virtual EOF line") < 0 ||
            check(!_PyTok_SourceLineIsImplicit(&source, 0) &&
                      !_PyTok_SourceLineIsImplicit(
                          &source, final_line + 1),
                  "virtual or invalid line is implicit") < 0) {
        goto error;
    }

    view = _PyTok_SourceSpanView(
        &source, _PyTok_SpanFromBounds(0, source.len + 1), &view_len);
    if (check_system_error(view == NULL, "accepted invalid source span") < 0 ||
            check_system_error(
                _PyTok_SourceLocation(
                    &source, source.len + 1,
                    _PYTOK_AFFINITY_RIGHT, &loc) < 0,
                "accepted invalid source offset") < 0 ||
            check_system_error(
                _PyTok_SourceLine(&source, final_line + 2, &line) < 0,
                "accepted invalid source line") < 0) {
        goto error;
    }

    _PyTok_SourceClear(&source);
    _PyTok_SourceInit(&source);
    if (_PyTok_SourceAppendLine(&source, "tail", 4, 0) < 0 ||
            check_system_error(
                _PyTok_SourceAppendLine(&source, "x\n", 2, 0) < 0,
                "appended after unterminated source line") < 0 ||
            check(_PyTok_SourceLocation(
                      &source, source.len,
                      _PYTOK_AFFINITY_RIGHT, &loc) == 0,
                  "cannot locate unterminated EOF") < 0 ||
            check(loc.lineno == 1 && loc.byte_col == 4,
                  "wrong unterminated EOF location") < 0) {
        goto error;
    }

    if (check_line_view(&source, 1, "tail") < 0 ||
            check_line_view(&source, PY_SSIZE_T_MAX, "tail") < 0) {
        goto error;
    }

    _PyTok_SourceDiscard(&source);
    if (check(_PyTok_SourceAppendLine(&source, "a\n", 2, 0) == 4,
              "wrong retained source offset") < 0 ||
            _PyTok_SourceLine(&source, 1, &line) < 0 ||
            check(line.start == 4 && line.end == 6,
                  "wrong retained source line") < 0 ||
            _PyTok_SourceLocation(
                &source, 4, _PYTOK_AFFINITY_LEFT, &loc) < 0 ||
            check(loc.lineno == 1 && loc.byte_col == 0,
                  "wrong retained source location") < 0) {
        goto error;
    }
    view = _PyTok_SourceSpanView(
        &source, _PyTok_SpanFromBounds(4, 5), &view_len);
    if (check(view != NULL && view_len == 1 && view[0] == 'a',
              "wrong retained source span") < 0 ||
            check_system_error(_PyTok_SourceSpanView(
                &source, _PyTok_SpanFromBounds(0, 1), &view_len) == NULL,
                "accepted discarded source span") < 0) {
        goto error;
    }

    _PyTok_SourceClear(&source);
    Py_RETURN_NONE;

error:
    _PyTok_SourceClear(&source);
    return NULL;
}

static PyObject *
test_tokenizer_source_discard(PyObject *Py_UNUSED(module),
                             PyObject *Py_UNUSED(args))
{
    _PyTok_SourceText source;
    _PyTok_SourceInit(&source);
    for (int i = 0; i < 260; i++) {
        if (_PyTok_SourceAppendLine(&source, "x\n", 2, 1) < 0) {
            goto error;
        }
    }
    char *bytes = source.bytes;
    _PyTok_Off capacity = source.cap;
    _PyTok_SourceDiscard(&source);
    if (check(source.base_offset == 520 && source.len == 0 &&
                  source.nlines == 0 && source.bytes == bytes &&
                  source.cap == capacity && source.bytes[0] == '\0',
              "discard did not preserve source allocation") < 0) {
        goto error;
    }
    for (int i = 0; i < 260; i++) {
        if (check(_PyTok_SourceAppendLine(&source, "y\n", 2, 0) == 520 + 2 * i,
                  "wrong source offset after discard") < 0 ||
                check(!_PyTok_SourceLineIsImplicit(&source, i + 1),
                      "discard preserved implicit newline flag") < 0) {
            goto error;
        }
    }
    if (check(source.bytes == bytes && source.cap == capacity,
              "discarded allocation was not reused") < 0) {
        goto error;
    }
    _PyTok_SourceDiscard(&source);
    if (check(_PyTok_SourceAppendLine(&source, "tail", 4, 0) == 1040,
              "wrong source offset after repeated discard") < 0) {
        goto error;
    }
    _PyTok_SourceDiscard(&source);
    if (check(_PyTok_SourceAppendLine(&source, "z\n", 2, 0) == 1044,
              "cannot append after discarding unterminated line") < 0) {
        goto error;
    }
    _PyTok_SourceDiscard(&source);
    source.base_offset = PY_SSIZE_T_MAX - 1;
    if (check(_PyTok_SourceAppendLine(&source, "z\n", 2, 0) < 0 &&
                  PyErr_ExceptionMatches(PyExc_MemoryError),
              "accepted overflowing logical source offset") < 0) {
        goto error;
    }
    PyErr_Clear();
    if (check(source.len == 0 && source.nlines == 0 &&
                  source.base_offset == PY_SSIZE_T_MAX - 1,
              "overflow changed retained source") < 0) {
        goto error;
    }
    _PyTok_SourceClear(&source);
    Py_RETURN_NONE;

error:
    _PyTok_SourceClear(&source);
    return NULL;
}

static PyMethodDef test_methods[] = {
    {"test_tokenizer_source", test_tokenizer_source, METH_NOARGS},
    {"test_tokenizer_source_discard", test_tokenizer_source_discard, METH_NOARGS},
    {NULL},
};

int
_PyTestInternalCapi_Init_Tokenizer(PyObject *module)
{
    return PyModule_AddFunctions(module, test_methods);
}
