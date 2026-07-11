#include "Python.h"
#include "pycore_runtime.h"       // _Py_ID()
#include "pycore_token.h"         // ERRORTOKEN
#include "pycore_tuple.h"         // _PyTuple_FromPair

#include "errors.h"
#include "../lexer/state.h"


/* ############## ERRORS ############## */

static void
append_format_bytes(char *output, size_t size, size_t *used,
                    const char *value, size_t len)
{
    if (*used < size) {
        size_t available = size - *used - 1;
        size_t copied = Py_MIN(available, len);
        memcpy(output + *used, value, copied);
        *used += copied;
        output[*used] = '\0';
    }
}

static void
append_codepoint(char *output, size_t size, size_t *used, unsigned int ch)
{
    char encoded[4];
    size_t len;
    if (ch <= 0x7f) {
        encoded[0] = (char)ch;
        len = 1;
    }
    else if (ch <= 0x7ff) {
        encoded[0] = (char)(0xc0 | (ch >> 6));
        encoded[1] = (char)(0x80 | (ch & 0x3f));
        len = 2;
    }
    else if (ch <= 0xffff) {
        encoded[0] = (char)(0xe0 | (ch >> 12));
        encoded[1] = (char)(0x80 | ((ch >> 6) & 0x3f));
        encoded[2] = (char)(0x80 | (ch & 0x3f));
        len = 3;
    }
    else {
        encoded[0] = (char)(0xf0 | (ch >> 18));
        encoded[1] = (char)(0x80 | ((ch >> 12) & 0x3f));
        encoded[2] = (char)(0x80 | ((ch >> 6) & 0x3f));
        encoded[3] = (char)(0x80 | (ch & 0x3f));
        len = 4;
    }
    append_format_bytes(output, size, used, encoded, len);
}

static void
format_error(char *output, size_t size, const char *format, va_list args)
{
    size_t used = 0;
    output[0] = '\0';
    while (*format != '\0') {
        if (*format != '%') {
            append_format_bytes(output, size, &used, format++, 1);
            continue;
        }
        const char *spec_start = format++;
        if (*format == '%') {
            append_format_bytes(output, size, &used, format++, 1);
            continue;
        }
        while (*format == '0' || *format == '.' ||
                Py_ISDIGIT((unsigned char)*format)) {
            format++;
        }
        char conversion = *format++;
        if (conversion == 's') {
            const char *value = va_arg(args, const char *);
            append_format_bytes(output, size, &used, value, strlen(value));
        }
        else if (conversion == 'c') {
            append_codepoint(output, size, &used,
                             (unsigned int)va_arg(args, int));
        }
        else {
            char spec[16];
            size_t spec_len = Py_MIN(
                (size_t)(format - spec_start), sizeof(spec) - 1);
            memcpy(spec, spec_start, spec_len);
            spec[spec_len] = '\0';
            char number[64];
            if (conversion == 'd' || conversion == 'i') {
                PyOS_snprintf(number, sizeof(number), spec,
                              va_arg(args, int));
            }
            else {
                PyOS_snprintf(number, sizeof(number), spec,
                              va_arg(args, unsigned int));
            }
            append_format_bytes(output, size, &used,
                                number, strlen(number));
        }
    }
}

static int
_syntaxerror_range(struct _PyTokenizer *tok, _PyTok_ErrKind kind,
                   const char *format,
                   int col_offset, int end_col_offset,
                   va_list vargs)
{
    if (_PyTok_HasError(tok)) {
        return ERRORTOKEN;
    }
    if (col_offset == -1) {
        col_offset = Py_SAFE_DOWNCAST(
            tok->cursor.pos - tok->cursor.line_start, _PyTok_Off, int);
    }
    if (end_col_offset == -1) {
        end_col_offset = col_offset;
    }
    char message[256];
    format_error(message, sizeof(message), format, vargs);
    _PyTok_Loc start = {tok->lineno, col_offset};
    _PyTok_Loc end = {tok->lineno, end_col_offset};
    _PyTok_RecordError(tok, kind, start, end,
                       tok->lineno, message);
    return ERRORTOKEN;
}

int
_PyTok_SyntaxError(struct _PyTokenizer *tok, const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    int ret = _syntaxerror_range(
        tok, _PYTOK_ERR_SYNTAX, format, -1, -1, vargs);
    va_end(vargs);
    return ret;
}

int
_PyTok_FormattedError(struct _PyTokenizer *tok, _PyTok_ErrKind kind,
                   const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    int ret = _syntaxerror_range(tok, kind, format, -1, -1, vargs);
    va_end(vargs);
    return ret;
}

int
_PyTok_FormattedErrorAt(struct _PyTokenizer *tok,
                                  _PyTok_ErrKind kind,
                                  _PyTok_Loc loc, _PyTok_Loc end_loc,
                                  int display_first_lineno,
                                  const char *format, ...)
{
    if (_PyTok_HasError(tok)) {
        return ERRORTOKEN;
    }
    char message[256];
    va_list vargs;
    va_start(vargs, format);
    format_error(message, sizeof(message), format, vargs);
    va_end(vargs);
    _PyTok_RecordError(tok, kind, loc, end_loc,
                       display_first_lineno, message);
    return ERRORTOKEN;
}

int
_PyTok_SyntaxErrorRange(struct _PyTokenizer *tok,
                        int col_offset, int end_col_offset,
                        const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    int ret = _syntaxerror_range(
        tok, _PYTOK_ERR_SYNTAX, format,
        col_offset, end_col_offset, vargs);
    va_end(vargs);
    return ret;
}

int
_PyTok_IndentationError(struct _PyTokenizer *tok)
{
    tok->cursor.pos = tok->cursor.line_end;
    _PyTok_RecordCurrentError(
        tok, _PYTOK_ERR_TABSPACE,
        "inconsistent use of tabs and spaces in indentation");
    return ERRORTOKEN;
}

int
_PyTok_WarnInvalidEscape(struct _PyTokenizer *tok,
                         int first_invalid_escape_char, _PyTok_Loc loc)
{
    PyObject *msg = PyUnicode_FromFormat(
        "\"\\%c\" is an invalid escape sequence. "
        "Such sequences will not work in the future. "
        "Did you mean \"\\\\%c\"? A raw string is also an option.",
        (char) first_invalid_escape_char,
        (char) first_invalid_escape_char
    );

    if (msg == NULL) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }

    if (PyErr_WarnExplicitObject(PyExc_SyntaxWarning, msg, tok->filename,
                                 tok->lineno, tok->module, NULL) < 0) {
        Py_DECREF(msg);

        if (PyErr_ExceptionMatches(PyExc_SyntaxWarning)) {
            /* Replace the SyntaxWarning exception with a SyntaxError
               to get a more accurate error report */
            PyErr_Clear();

            return _PyTok_FormattedErrorAt(
                tok, _PYTOK_ERR_SYNTAX, loc, loc, loc.lineno,
                "\"\\%c\" is an invalid escape sequence. "
                "Did you mean \"\\\\%c\"? A raw string is also an option.",
                (char) first_invalid_escape_char,
                (char) first_invalid_escape_char);
        }

        _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
        return -1;
    }

    Py_DECREF(msg);
    return 0;
}

void
_PyTok_RaiseInitException(PyObject *filename)
{
    if (!(PyErr_ExceptionMatches(PyExc_LookupError)
          || PyErr_ExceptionMatches(PyExc_SyntaxError)
          || PyErr_ExceptionMatches(PyExc_ValueError)
          || PyErr_ExceptionMatches(PyExc_UnicodeDecodeError))) {
        return;
    }
    PyObject *errstr = NULL;
    PyObject *tuple = NULL;
    PyObject *type;
    PyObject *value;
    PyObject *tback;
    PyErr_Fetch(&type, &value, &tback);
    if (PyErr_GivenExceptionMatches(value, PyExc_SyntaxError)) {
        if (PyObject_SetAttr(value, &_Py_ID(filename), filename)) {
            goto error;
        }
        PyErr_Restore(type, value, tback);
        return;
    }
    errstr = PyObject_Str(value);
    if (!errstr) {
        goto error;
    }

    PyObject *tmp = Py_BuildValue("(OiiO)", filename, 0, -1, Py_None);
    if (!tmp) {
        goto error;
    }

    tuple = _PyTuple_FromPair(errstr, tmp);
    Py_DECREF(tmp);
    if (!tuple) {
        goto error;
    }
    PyErr_SetObject(PyExc_SyntaxError, tuple);

error:
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(tback);
    Py_XDECREF(errstr);
    Py_XDECREF(tuple);
}

int
_PyTok_Warn(struct _PyTokenizer *tok, PyObject *category, _PyTok_Loc loc,
            const char *format, ...)
{
    PyObject *errmsg;
    va_list vargs;
    va_start(vargs, format);
    errmsg = PyUnicode_FromFormatV(format, vargs);
    va_end(vargs);
    if (!errmsg) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }

    if (PyErr_WarnExplicitObject(category, errmsg, tok->filename,
                                 tok->lineno, tok->module, NULL) < 0) {
        if (PyErr_ExceptionMatches(category)) {
            /* Replace the DeprecationWarning exception with a SyntaxError
               to get a more accurate error report */
            PyErr_Clear();
            const char *message = PyUnicode_AsUTF8(errmsg);
            if (message != NULL) {
                _PyTok_FormattedErrorAt(
                    tok, _PYTOK_ERR_SYNTAX, loc, loc, loc.lineno,
                    "%s", message);
            }
        }
        else {
            _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
        }
        goto error;
    }
    Py_DECREF(errmsg);
    return 0;

error:
    Py_XDECREF(errmsg);
    return -1;
}


/* ############## STRING MANIPULATION ############## */

char *
_PyTok_CopyString(const char *s, Py_ssize_t len, struct _PyTokenizer *tok)
{
    char* result = (char *)PyMem_Malloc(len + 1);
    if (!result) {
        PyErr_NoMemory();
        _PyTok_RecordNoMemory(tok);
        return NULL;
    }
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

static int
valid_utf8(const unsigned char *s, const unsigned char *end)
{
    int continuation;
    Py_ssize_t remaining = end - s;
    if (remaining <= 0) {
        return 0;
    }
    if (*s < 0x80) {
        return 1;
    }
    if (*s < 0xE0) {
        if (*s < 0xC2) {
            return 0;
        }
        continuation = 1;
    }
    else if (*s < 0xF0) {
        continuation = 2;
    }
    else if (*s < 0xF5) {
        continuation = 3;
    }
    else {
        return 0;
    }
    if (remaining <= continuation) {
        return 0;
    }
    if ((*s == 0xE0 && s[1] < 0xA0) ||
            (*s == 0xED && s[1] >= 0xA0) ||
            (*s == 0xF0 && s[1] < 0x90) ||
            (*s == 0xF4 && s[1] >= 0x90)) {
        return 0;
    }
    for (int i = 1; i <= continuation; i++) {
        if (s[i] < 0x80 || s[i] >= 0xC0) {
            return 0;
        }
    }
    return continuation + 1;
}

int
_PyTok_EnsureUTF8(const char *line, struct _PyTokenizer *tok, int lineno)
{
    const char *badchar = NULL;
    const char *c;
    int length;
    int col_offset = 0;
    const char *end = line + strlen(line);
    for (c = line; c < end; c += length) {
        if (!(length = valid_utf8((const unsigned char *)c,
                                  (const unsigned char *)end))) {
            badchar = c;
            break;
        }
        col_offset++;
        if (*c == '\n') {
            lineno++;
            col_offset = 0;
        }
    }
    if (badchar) {
        _PyTok_Loc loc = {lineno, col_offset + 1};
        _PyTok_RecordError(
            tok, _PYTOK_ERR_SYNTAX, loc, loc, lineno, NULL);
        tok->error.detail = _PYTOK_DETAIL_NON_UTF8;
        tok->error.invalid_byte = (unsigned char)*badchar;
        tok->error.columns_are_chars = 1;
        return 0;
    }
    return 1;
}


/* ############## DEBUGGING STUFF ############## */

#ifdef Py_DEBUG
void
_PyTok_PrintEscape(FILE *f, const char *s, Py_ssize_t size)
{
    if (s == NULL) {
        fputs("NULL", f);
        return;
    }
    putc('"', f);
    while (size-- > 0) {
        unsigned char c = *s++;
        switch (c) {
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            case '\f': fputs("\\f", f); break;
            case '\'': fputs("\\'", f); break;
            case '"': fputs("\\\"", f); break;
            default:
                if (0x20 <= c && c <= 0x7f)
                    putc(c, f);
                else
                    fprintf(f, "\\x%02x", c);
        }
    }
    putc('"', f);
}

#endif
