#include <Python.h>
#include <errcode.h>

#include "pycore_runtime.h"       // _Py_ID()
#include "pycore_tuple.h"         // _PyTuple_FromPair
#include "pegen.h"

// TOKENIZER ERRORS

static inline void
raise_unclosed_parentheses_error(Parser *p) {
       int depth = _PyTok_ParenDepth(p->tok);
       char opening;
       int error_lineno;
       int error_col;
       _PyTok_ParenEntry(p->tok, depth - 1, &opening,
                         &error_lineno, &error_col);
       RAISE_ERROR_KNOWN_LOCATION(p, PyExc_SyntaxError,
                                  error_lineno, error_col, error_lineno, -1,
                                  "'%c' was never closed",
                                  opening);
}

static Py_ssize_t
eof_error_col_offset(Parser *p, const _PyTok_Error *error)
{
    Py_ssize_t col_offset = error->loc.col - 1;
    if (_PyTok_GetSourceKind(p->tok) != _PYTOK_SOURCE_FILE) {
        return col_offset;
    }
    Py_ssize_t len;
    const char *line = _PyTok_LineView(p->tok, error->loc.lineno, &len);
    if (line == NULL) {
        PyErr_Clear();
        return col_offset;
    }
    Py_ssize_t backslash_col = error->loc.col - 2;
    if (backslash_col < 0 || backslash_col > len) {
        return col_offset;
    }
    for (Py_ssize_t i = 0; i < backslash_col; i++) {
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\f') {
            return col_offset;
        }
    }
    return -1;
}

int
_Pypegen_tokenizer_error(Parser *p)
{
    const _PyTok_Error *error = _PyTok_GetError(p->tok);
    if (error == NULL) {
        PyErr_SetString(PyExc_SystemError,
                        "tokenizer failed without an error record");
        return -1;
    }
    if (error->kind == _PYTOK_ERR_PROPAGATE) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_SystemError,
                            "tokenizer lost a pending exception");
        }
        return -1;
    }
    if (error->kind == _PYTOK_ERR_DECODE && error->pending) {
        return _Pypegen_raise_decode_error(p);
    }
    if (PyErr_Occurred()) {
        return -1;
    }
    p->error_indicator = 1;
    switch (error->kind) {
        case _PYTOK_ERR_EOF_IN_CONSTRUCT:
            if (_PyTok_ParenDepth(p->tok)) {
                raise_unclosed_parentheses_error(p);
            } else {
                RAISE_ERROR_KNOWN_LOCATION(
                    p, PyExc_SyntaxError,
                    error->loc.lineno, eof_error_col_offset(p, error),
                    error->loc.lineno, -2,
                    "unexpected EOF while parsing");
            }
            return -1;
        case _PYTOK_ERR_COLUMN_OVERFLOW:
            PyErr_SetString(PyExc_OverflowError,
                    "Parser column offset overflow - source line is too big");
            return -1;
        case _PYTOK_ERR_LINECONT:
            RAISE_ERROR_KNOWN_LOCATION(
                p, PyExc_SyntaxError,
                error->loc.lineno, error->loc.col - 1,
                error->loc.lineno, -1,
                "unexpected character after line continuation character");
            return -1;
        default:
            return _PyTok_RaiseError(p->tok);
    }
}

int
_Pypegen_raise_decode_error(Parser *p)
{
    assert(PyErr_Occurred());
    const char *errtype = NULL;
    if (PyErr_ExceptionMatches(PyExc_UnicodeError)) {
        errtype = "unicode error";
    }
    else if (PyErr_ExceptionMatches(PyExc_ValueError)) {
        errtype = "value error";
    }
    if (errtype) {
        PyObject *type;
        PyObject *value;
        PyObject *tback;
        PyObject *errstr;
        PyErr_Fetch(&type, &value, &tback);
        errstr = PyObject_Str(value);
        if (errstr) {
            RAISE_SYNTAX_ERROR("(%s) %U", errtype, errstr);
            Py_DECREF(errstr);
        }
        else {
            PyErr_Clear();
            RAISE_SYNTAX_ERROR("(%s) unknown error", errtype);
        }
        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(tback);
    }

    return -1;
}

static int
_PyPegen_tokenize_full_source_to_check_for_errors(Parser *p) {
    // Tokenize the whole input to see if there are any tokenization
    // errors such as mismatching parentheses. These will get priority
    // over generic syntax errors only if the line number of the error is
    // before the one that we had for the generic error.

    // We don't want to tokenize to the end for interactive input
    if (_PyTok_IsPrompting(p->tok)) {
        return 0;
    }

    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);

    Token *current_token = p->known_err_token != NULL ? p->known_err_token : p->tokens[p->fill - 1];
    Py_ssize_t current_err_line = current_token->lineno;

    int ret = 0;
    _PyTok_Token new_token;
    _PyTok_TokenInit(&new_token);

    for (;;) {
        _PyTok_Status status = _PyTok_Get(p->tok, &new_token);
        if (status == _PYTOK_ERROR) {
            const _PyTok_Error *error = _PyTok_GetError(p->tok);
            if (error != NULL) {
                if (error->kind == _PYTOK_ERR_SYNTAX ||
                        error->kind == _PYTOK_ERR_EOF_IN_STRING ||
                        error->kind == _PYTOK_ERR_EOL_IN_STRING ||
                        error->kind == _PYTOK_ERR_DECODE ||
                        error->kind == _PYTOK_ERR_PROPAGATE ||
                        error->kind == _PYTOK_ERR_NOMEM ||
                        error->kind == _PYTOK_ERR_INTERRUPT) {
                    _Pypegen_tokenizer_error(p);
                    ret = -1;
                    goto exit;
                }
                if (_PyTok_ParenDepth(p->tok) != 0) {
                    int depth = _PyTok_ParenDepth(p->tok);
                    int error_lineno;
                    _PyTok_ParenEntry(p->tok, depth - 1, NULL,
                                      &error_lineno, NULL);
                    if (current_err_line > error_lineno) {
                        raise_unclosed_parentheses_error(p);
                        ret = -1;
                        goto exit;
                    }
                }
            }
            break;
        }
        if (new_token.type != ENDMARKER) {
            _PyTok_TokenClear(&new_token);
            _PyTok_TokenInit(&new_token);
            continue;
        }
        break;
    }


exit:
    _PyTok_TokenClear(&new_token);
    // If we're in an f-string, we want the syntax error in the expression part
    // to propagate, so that tokenizer errors (like expecting '}') that happen afterwards
    // do not swallow it.
    if (PyErr_Occurred() && _PyTok_FStringDepth(p->tok) <= 0) {
        Py_XDECREF(value);
        Py_XDECREF(type);
        Py_XDECREF(traceback);
    } else {
        PyErr_Restore(type, value, traceback);
    }
    return ret;
}

// PARSER ERRORS

void *
_PyPegen_raise_error(Parser *p, PyObject *errtype, int use_mark, const char *errmsg, ...)
{
    // Bail out if we already have an error set.
    if (p->error_indicator && PyErr_Occurred()) {
        return NULL;
    }
    if (p->fill == 0) {
        va_list va;
        va_start(va, errmsg);
        _PyPegen_raise_error_known_location(p, errtype, 0, 0, 0, -1, errmsg, va);
        va_end(va);
        return NULL;
    }
    if (use_mark && p->mark == p->fill && _PyPegen_fill_token(p) < 0) {
        p->error_indicator = 1;
        return NULL;
    }
    Token *t = p->known_err_token != NULL
                   ? p->known_err_token
                   : p->tokens[use_mark ? p->mark : p->fill - 1];
    Py_ssize_t col_offset;
    Py_ssize_t end_col_offset = -1;
    if (t->col_offset == -1) {
        col_offset = _PyTok_Column(p->tok);
    } else {
        col_offset = t->col_offset + 1;
    }

    if (t->end_col_offset != -1) {
        end_col_offset = t->end_col_offset + 1;
    }

    va_list va;
    va_start(va, errmsg);
    _PyPegen_raise_error_known_location(p, errtype, t->lineno, col_offset, t->end_lineno, end_col_offset, errmsg, va);
    va_end(va);

    return NULL;
}

static PyObject *
get_error_line_from_tokenizer_buffers(Parser *p, Py_ssize_t lineno)
{
    Py_ssize_t relative_lineno = p->starting_lineno
        ? lineno - p->starting_lineno + 1
        : lineno;
    Py_ssize_t len;
    const char *line = _PyTok_LineView(p->tok, (int)relative_lineno, &len);
    if (line == NULL) {
        PyErr_Clear();
        return Py_GetConstant(Py_CONSTANT_EMPTY_STR);
    }
    const char *nul = memchr(line, '\0', len);
    if (nul != NULL) {
        len = nul - line;
    }
    if (_PyTok_GetSourceKind(p->tok) == _PYTOK_SOURCE_FILE &&
            _PyTok_LineIsImplicit(p->tok, (int)relative_lineno) &&
            len > 0 && line[len - 1] == '\n') {
        len--;
    }
    return PyUnicode_DecodeUTF8(line, len, "replace");
}

void *
_PyPegen_raise_error_known_location(Parser *p, PyObject *errtype,
                                    Py_ssize_t lineno, Py_ssize_t col_offset,
                                    Py_ssize_t end_lineno, Py_ssize_t end_col_offset,
                                    const char *errmsg, va_list va)
{
    // Bail out if we already have an error set.
    if (p->error_indicator && PyErr_Occurred()) {
        return NULL;
    }
    PyObject *value = NULL;
    PyObject *errstr = NULL;
    PyObject *error_line = NULL;
    PyObject *tmp = NULL;
    p->error_indicator = 1;

    if (end_lineno == CURRENT_POS) {
        end_lineno = _PyTok_Lineno(p->tok);
    }
    if (end_col_offset == CURRENT_POS) {
        end_col_offset = _PyTok_Column(p->tok);
    }

    errstr = PyUnicode_FromFormatV(errmsg, va);
    if (!errstr) {
        goto error;
    }

    error_line = get_error_line_from_tokenizer_buffers(p, lineno);
    if (!error_line) {
        goto error;
    }

    Py_ssize_t col_number = col_offset;
    Py_ssize_t end_col_number = end_col_offset;

    col_number = _PyPegen_byte_offset_to_character_offset(error_line, col_offset);
    if (col_number < 0) {
        goto error;
    }

    if (end_col_offset > 0) {
        end_col_number = _PyPegen_byte_offset_to_character_offset(error_line, end_col_offset);
        if (end_col_number < 0) {
            goto error;
        }
    }

    tmp = Py_BuildValue("(OnnNnn)", _PyTok_Filename(p->tok), lineno,
                        col_number, error_line, end_lineno, end_col_number);
    if (!tmp) {
        goto error;
    }
    value = _PyTuple_FromPair(errstr, tmp);
    Py_DECREF(tmp);
    if (!value) {
        goto error;
    }
    PyErr_SetObject(errtype, value);

    Py_DECREF(errstr);
    Py_DECREF(value);
    return NULL;

error:
    Py_XDECREF(errstr);
    Py_XDECREF(error_line);
    return NULL;
}

void
_Pypegen_set_syntax_error(Parser* p, Token* last_token) {
    // Existing syntax error
    if (PyErr_Occurred()) {
        // Prioritize tokenizer errors to custom syntax errors raised
        // on the second phase only if the errors come from the parser.
        int is_tok_ok = _PyTok_GetError(p->tok) == NULL;
        if (is_tok_ok && PyErr_ExceptionMatches(PyExc_SyntaxError)) {
            _PyPegen_tokenize_full_source_to_check_for_errors(p);
        }
        // Propagate the existing syntax error.
        return;
    }
    // Initialization error
    if (p->fill == 0) {
        RAISE_SYNTAX_ERROR("error at start before reading any input");
    }
    // Parser encountered EOF (End of File) unexpectedtly
    const _PyTok_Error *tok_error = _PyTok_GetError(p->tok);
    if (last_token->type == ERRORTOKEN && tok_error != NULL &&
            tok_error->kind == _PYTOK_ERR_EOF_IN_CONSTRUCT) {
        if (_PyTok_ParenDepth(p->tok)) {
            raise_unclosed_parentheses_error(p);
        } else {
            RAISE_SYNTAX_ERROR("unexpected EOF while parsing");
        }
        return;
    }
    // Indentation error in the tokenizer
    if (last_token->type == INDENT || last_token->type == DEDENT) {
        RAISE_INDENTATION_ERROR(last_token->type == INDENT ? "unexpected indent" : "unexpected unindent");
        return;
    }
    // Unknown error (generic case)

    // Use the last token we found on the first pass to avoid reporting
    // incorrect locations for generic syntax errors just because we reached
    // further away when trying to find specific syntax errors in the second
    // pass.
    RAISE_SYNTAX_ERROR_KNOWN_LOCATION(last_token, "invalid syntax");
    // _PyPegen_tokenize_full_source_to_check_for_errors will override the existing
    // generic SyntaxError we just raised if errors are found.
    _PyPegen_tokenize_full_source_to_check_for_errors(p);
}

void
_Pypegen_stack_overflow(Parser *p)
{
    p->error_indicator = 1;
    PyErr_SetString(PyExc_MemoryError,
        "Parser stack overflowed - Python source too complex to parse");
}
