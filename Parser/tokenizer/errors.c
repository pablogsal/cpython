#include "Python.h"
#include "pycore_tuple.h"

#include "errors.h"
#include "../lexer/state.h"

int
_PyTok_HasError(const struct _PyTokenizer *tok)
{
    return tok->error.kind != _PYTOK_ERR_NONE;
}

void
_PyTok_RecordError(struct _PyTokenizer *tok, _PyTok_ErrKind kind,
                   _PyTok_Loc loc, _PyTok_Loc end_loc,
                   int display_first_lineno, const char *msg)
{
    if (_PyTok_HasError(tok)) {
        return;
    }
    tok->error.kind = kind;
    tok->error.loc = loc;
    tok->error.end_loc = end_loc;
    tok->error.display_first_lineno = display_first_lineno;
    tok->error.detail = _PYTOK_DETAIL_NONE;
    tok->error.invalid_byte = 0;
    tok->error.pending = 0;
    tok->error.columns_are_chars = 0;
    if (msg == NULL) {
        tok->error.msg[0] = '\0';
    }
    else {
        PyOS_snprintf(tok->error.msg, sizeof(tok->error.msg), "%s", msg);
    }
}

void
_PyTok_RecordCurrentError(struct _PyTokenizer *tok, _PyTok_ErrKind kind,
                          const char *msg)
{
    int col = Py_SAFE_DOWNCAST(
        tok->cursor.pos - tok->cursor.line_start, _PyTok_Off, int);
    _PyTok_Loc loc = {tok->lineno, col};
    _PyTok_RecordError(tok, kind, loc, loc, tok->lineno, msg);
}

void
_PyTok_RecordNoMemory(struct _PyTokenizer *tok)
{
    if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
        PyErr_Clear();
    }
    _PyTok_RecordCurrentError(tok, _PYTOK_ERR_NOMEM, NULL);
}

void
_PyTok_RecordPending(struct _PyTokenizer *tok, _PyTok_ErrKind kind)
{
    assert(kind == _PYTOK_ERR_DECODE || kind == _PYTOK_ERR_PROPAGATE);
    assert(PyErr_Occurred());
    if (_PyTok_HasError(tok)) {
        return;
    }
    _PyTok_RecordCurrentError(tok, kind, NULL);
    tok->error.pending = 1;
}

static PyObject *
error_text(PyTokenizer *tok, const _PyTok_Error *error)
{
    if (error->kind == _PYTOK_ERR_LINECONT) {
        Py_ssize_t len;
        const char *view = _PyTok_CurrentRunView(tok, &len);
        if (view == NULL) {
            PyErr_Clear();
            return Py_GetConstant(Py_CONSTANT_EMPTY_STR);
        }
        const char *nul = memchr(view, '\0', len);
        if (nul != NULL) {
            len = nul - view;
        }
        if (len > 0 && view[len - 1] == '\n') {
            len--;
        }
        return PyUnicode_DecodeUTF8(view, len, "replace");
    }
    int first = error->display_first_lineno > 0
        ? error->display_first_lineno
        : error->loc.lineno;
    int last = error->loc.lineno > 0 ? error->loc.lineno : first;
    Py_ssize_t len;
    const char *view = first == last
        ? _PyTok_LineView(tok, last, &len)
        : _PyTok_LineRangeView(tok, first, last, &len);
    if (view == NULL) {
        PyErr_Clear();
        return Py_GetConstant(Py_CONSTANT_EMPTY_STR);
    }
    const char *nul = memchr(view, '\0', len);
    if (nul != NULL) {
        len = nul - view;
    }
    if (len > 0 && view[len - 1] == '\n') {
        len--;
    }
    return PyUnicode_DecodeUTF8(view, len, "replace");
}

static Py_ssize_t
character_column(PyTokenizer *tok, int lineno, int byte_column)
{
    if (byte_column < 0) {
        return byte_column;
    }
    _PyTok_Line line;
    if (_PyTok_SourceLine(&tok->source, lineno, &line) < 0) {
        return byte_column;
    }
    Py_ssize_t line_len = line.end - line.start;
    Py_ssize_t prefix_len = Py_MIN((Py_ssize_t)byte_column, line_len);
    PyObject *prefix = PyUnicode_DecodeUTF8(
        tok->source.bytes + line.start, prefix_len, "replace");
    if (prefix == NULL) {
        return -1;
    }
    Py_ssize_t result = PyUnicode_GET_LENGTH(prefix);
    Py_DECREF(prefix);
    return result;
}

typedef enum {
    ERROR_BYTE_COLUMNS,
    ERROR_CHAR_COLUMNS,
    ERROR_RUN_END_COLUMN,
} ErrorColumnMode;

static int
raise_syntax_error_object_at(PyTokenizer *tok, PyObject *type,
                             PyObject *message, int include_end,
                             _PyTok_Loc loc, _PyTok_Loc end_loc,
                             ErrorColumnMode column_mode)
{
    const _PyTok_Error *error = &tok->error;
    PyObject *text = error_text(tok, error);
    if (text == NULL) {
        return -1;
    }
    Py_ssize_t col;
    Py_ssize_t end_col;
    if (column_mode == ERROR_RUN_END_COLUMN) {
        col = PyUnicode_GET_LENGTH(text) + 1;
        end_col = col;
    }
    else if (column_mode == ERROR_CHAR_COLUMNS) {
        col = loc.col;
        end_col = end_loc.col;
    }
    else {
        col = character_column(tok, loc.lineno, loc.col);
        end_col = character_column(tok, end_loc.lineno, end_loc.col);
    }
    if (col < 0 || end_col < 0) {
        Py_DECREF(text);
        return -1;
    }
    PyObject *details;
    if (include_end) {
        details = Py_BuildValue(
            "(OnnNnn)", tok->filename != NULL ? tok->filename : Py_None,
            loc.lineno, col, text, end_loc.lineno, end_col);
    }
    else {
        details = Py_BuildValue(
            "(OnnNOO)", tok->filename != NULL ? tok->filename : Py_None,
            loc.lineno, col, text, Py_None, Py_None);
    }
    if (details == NULL) {
        return -1;
    }
    PyObject *value = _PyTuple_FromPair(message, details);
    Py_DECREF(details);
    if (value == NULL) {
        return -1;
    }
    PyErr_SetObject(type, value);
    Py_DECREF(value);
    return -1;
}

static int
raise_syntax_error_at(PyTokenizer *tok, PyObject *type, const char *msg,
                      int include_end, _PyTok_Loc loc, _PyTok_Loc end_loc,
                      ErrorColumnMode column_mode)
{
    PyObject *message = PyUnicode_FromString(msg);
    if (message == NULL) {
        return -1;
    }
    int result = raise_syntax_error_object_at(
        tok, type, message, include_end, loc, end_loc, column_mode);
    Py_DECREF(message);
    return result;
}

static int
raise_syntax_error(PyTokenizer *tok, PyObject *type, const char *msg,
                   int include_end)
{
    const _PyTok_Error *error = &tok->error;
    return raise_syntax_error_at(
        tok, type, msg, include_end, error->loc, error->end_loc,
        error->columns_are_chars ? ERROR_CHAR_COLUMNS : ERROR_BYTE_COLUMNS);
}

const _PyTok_Error *
_PyTok_GetError(const PyTokenizer *tok)
{
    return _PyTok_HasError(tok) ? &tok->error : NULL;
}

int
_PyTok_RaiseError(PyTokenizer *tok)
{
    const _PyTok_Error *error = _PyTok_GetError(tok);
    if (error == NULL) {
        PyErr_SetString(PyExc_SystemError, "tokenizer has no error");
        return -1;
    }
    if (error->kind == _PYTOK_ERR_PROPAGATE || error->pending) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_SystemError,
                            "tokenizer lost a pending exception");
        }
        return -1;
    }
    if (PyErr_Occurred()) {
        PyErr_Clear();
    }
    switch (error->kind) {
        case _PYTOK_ERR_SYNTAX:
            if (error->detail == _PYTOK_DETAIL_NON_UTF8) {
                PyObject *message;
                if (tok->filename == NULL || tok->filename == Py_None) {
                    message = PyUnicode_FromFormat(
                        "Non-UTF-8 code starting with '\\x%02x' on line %d, "
                        "but no encoding declared; see "
                        "https://peps.python.org/pep-0263/ for details",
                        error->invalid_byte, error->loc.lineno);
                }
                else {
                    message = PyUnicode_FromFormat(
                        "Non-UTF-8 code starting with '\\x%02x' in file %U "
                        "on line %d, but no encoding declared; see "
                        "https://peps.python.org/pep-0263/ for details",
                        error->invalid_byte, tok->filename,
                        error->loc.lineno);
                }
                if (message == NULL) {
                    return -1;
                }
                int result = raise_syntax_error_object_at(
                    tok, PyExc_SyntaxError, message, 1,
                    error->loc, error->end_loc, ERROR_CHAR_COLUMNS);
                Py_DECREF(message);
                return result;
            }
            return raise_syntax_error(tok, PyExc_SyntaxError,
                                      error->msg, 1);
        case _PYTOK_ERR_DECODE:
        {
            if (error->detail != _PYTOK_DETAIL_BOM_ENCODING ||
                    tok->error_detail == NULL) {
                PyErr_SetString(PyExc_SystemError,
                                "tokenizer has incomplete decode error");
                return -1;
            }
            PyObject *message = PyUnicode_FromFormat(
                "encoding problem: %s with BOM", tok->error_detail);
            if (message == NULL) {
                return -1;
            }
            int result = raise_syntax_error_object_at(
                tok, PyExc_SyntaxError, message, 1,
                error->loc, error->end_loc, ERROR_BYTE_COLUMNS);
            Py_DECREF(message);
            return result;
        }
        case _PYTOK_ERR_EOF_IN_CONSTRUCT:
            return raise_syntax_error(
                tok, PyExc_SyntaxError,
                "unexpected EOF in multi-line statement", 0);
        case _PYTOK_ERR_EOF_IN_STRING:
        case _PYTOK_ERR_EOL_IN_STRING:
            return raise_syntax_error(
                tok, PyExc_SyntaxError,
                error->msg[0] != '\0' ? error->msg :
                "unterminated string literal", 1);
        case _PYTOK_ERR_DEDENT_MISMATCH:
            return raise_syntax_error(tok, PyExc_IndentationError,
                                      error->msg, 0);
        case _PYTOK_ERR_TABSPACE:
            return raise_syntax_error(tok, PyExc_TabError, error->msg, 0);
        case _PYTOK_ERR_TOODEEP:
            return raise_syntax_error(tok, PyExc_IndentationError,
                                      error->msg, 0);
        case _PYTOK_ERR_LINECONT:
        {
            int col = Py_SAFE_DOWNCAST(
                tok->cursor.line_end - tok->cursor.run_start,
                _PyTok_Off, int);
            _PyTok_Loc loc = {error->loc.lineno, col};
            return raise_syntax_error_at(
                tok, PyExc_SyntaxError, error->msg, 0, loc, loc,
                ERROR_RUN_END_COLUMN);
        }
        case _PYTOK_ERR_COLUMN_OVERFLOW:
            return raise_syntax_error(
                tok, PyExc_SyntaxError, "unknown tokenization error", 0);
        case _PYTOK_ERR_INTERRUPT:
            PyErr_SetNone(PyExc_KeyboardInterrupt);
            return -1;
        case _PYTOK_ERR_NOMEM:
            PyErr_NoMemory();
            return -1;
        case _PYTOK_ERR_NONE:
        case _PYTOK_ERR_PROPAGATE:
            break;
    }
    Py_UNREACHABLE();
}

int
_PyTok_ErrorIsIncomplete(const PyTokenizer *tok)
{
    const _PyTok_Error *error = _PyTok_GetError(tok);
    return (error == NULL && _PyTok_InputExhausted(tok)) ||
        (error != NULL &&
         (error->kind == _PYTOK_ERR_EOF_IN_CONSTRUCT ||
          error->kind == _PYTOK_ERR_EOF_IN_STRING ||
          error->kind == _PYTOK_ERR_EOL_IN_STRING));
}
