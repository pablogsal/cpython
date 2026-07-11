#include "Python.h"
#include "pycore_unicodeobject.h"

#include "errors.h"
#include "helpers.h"
#include "seam.h"
#include "../lexer/state.h"

static int
default_warn(struct _PyTokenizer *tok, _PyTok_WarnKind kind,
             const char *text, int value, _PyTok_Loc loc)
{
    if (kind == _PYTOK_WARN_INVALID_ESCAPE) {
        return _PyTok_WarnInvalidEscape(tok, value, loc);
    }
    return _PyTok_Warn(
        tok, PyExc_SyntaxWarning, loc, "invalid %s literal", text);
}

static int
default_verify_identifier(struct _PyTokenizer *tok, _PyTok_Span span,
                          _PyTok_Off *invalid_end, unsigned int *invalid_ch)
{
    Py_ssize_t len;
    const char *view = _PyTok_SourceSpanView(&tok->source, span, &len);
    if (view == NULL) {
        _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
        return -1;
    }
    PyObject *identifier = PyUnicode_DecodeUTF8(view, len, NULL);
    if (identifier == NULL) {
        _PyTok_RecordPending(
            tok, PyErr_ExceptionMatches(PyExc_UnicodeDecodeError)
                ? _PYTOK_ERR_DECODE
                : _PYTOK_ERR_PROPAGATE);
        return -1;
    }
    Py_ssize_t invalid = _PyUnicode_ScanIdentifier(identifier);
    assert(invalid >= 0);
    assert(PyUnicode_GET_LENGTH(identifier) > 0);
    if (invalid == PyUnicode_GET_LENGTH(identifier)) {
        Py_DECREF(identifier);
        return 1;
    }
    *invalid_ch = PyUnicode_READ_CHAR(identifier, invalid);
    if (invalid + 1 == PyUnicode_GET_LENGTH(identifier)) {
        *invalid_end = span.end;
        Py_DECREF(identifier);
        return 0;
    }
    PyObject *prefix = PyUnicode_Substring(identifier, 0, invalid + 1);
    Py_DECREF(identifier);
    if (prefix == NULL) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }
    Py_ssize_t prefix_len;
    if (PyUnicode_AsUTF8AndSize(prefix, &prefix_len) == NULL) {
        Py_DECREF(prefix);
        if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
            _PyTok_RecordNoMemory(tok);
        }
        else {
            _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
        }
        return -1;
    }
    Py_DECREF(prefix);
    *invalid_end = span.start + prefix_len;
    return 0;
}

static int
default_intern_metadata(struct _PyTokenizer *tok, _PyTok_Span span,
                        PyObject **metadata)
{
    Py_ssize_t len;
    const char *expr = _PyTok_SourceSpanView(&tok->source, span, &len);
    if (expr == NULL) {
        _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
        return -1;
    }
    int hash_detected = 0;
    int in_string = 0;
    char quote = 0;
    for (Py_ssize_t i = 0; i < len; i++) {
        char ch = expr[i];
        if (ch == '\\') {
            i++;
            continue;
        }
        if (ch == '"' || ch == '\'') {
            if (!in_string) {
                in_string = 1;
                quote = ch;
            }
            else if (ch == quote) {
                in_string = 0;
            }
            continue;
        }
        if (ch == '#' && !in_string) {
            hash_detected = 1;
            break;
        }
    }
    if (!hash_detected) {
        *metadata = PyUnicode_DecodeUTF8(expr, len, NULL);
    }
    else {
        char *cleaned = PyMem_Malloc((size_t)len + 1);
        if (cleaned == NULL) {
            PyErr_NoMemory();
            _PyTok_RecordNoMemory(tok);
            return -1;
        }
        Py_ssize_t read = 0;
        Py_ssize_t write = 0;
        in_string = 0;
        quote = 0;
        while (read < len) {
            char ch = expr[read];
            if (ch == '"' || ch == '\'') {
                if (!in_string) {
                    in_string = 1;
                    quote = ch;
                }
                else if (ch == quote) {
                    in_string = 0;
                }
                cleaned[write++] = ch;
            }
            else if (ch == '#' && !in_string) {
                while (read < len && expr[read] != '\n') {
                    read++;
                }
                if (read < len) {
                    cleaned[write++] = '\n';
                }
            }
            else {
                cleaned[write++] = ch;
            }
            read++;
        }
        *metadata = PyUnicode_DecodeUTF8(cleaned, write, NULL);
        PyMem_Free(cleaned);
    }
    if (*metadata == NULL) {
        if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
            _PyTok_RecordNoMemory(tok);
        }
        else {
            _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
        }
        return -1;
    }
    return 0;
}

const _PyTok_Seam *
_PyTok_DefaultSeam(void)
{
    static const _PyTok_Seam seam = {
        .warn = default_warn,
        .verify_identifier = default_verify_identifier,
        .intern_metadata = default_intern_metadata,
    };
    return &seam;
}

void
_PyTok_SetSeam(PyTokenizer *tok, const _PyTok_Seam *seam)
{
    assert(tok != NULL);
    assert(seam != NULL);
    assert(seam->warn != NULL);
    assert(seam->verify_identifier != NULL);
    assert(seam->intern_metadata != NULL);
    tok->seam = seam;
}
