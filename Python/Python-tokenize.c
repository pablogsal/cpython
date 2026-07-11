#include "Python.h"
#include "internal/pycore_critical_section.h"   // Py_BEGIN_CRITICAL_SECTION
#include "internal/pycore_tuple.h"              // _PyTuple_FromPair
#include "../Parser/tokenizer/tokenizer.h"
#include "../Parser/pegen.h"                    // _PyPegen_byte_offset_to_character_offset()

static struct PyModuleDef _tokenizemodule;

typedef struct {
    PyTypeObject *TokenizerIter;
} tokenize_state;

static tokenize_state *
get_tokenize_state(PyObject *module) {
    return (tokenize_state *)PyModule_GetState(module);
}

#define _tokenize_get_state_by_type(type) \
    get_tokenize_state(PyType_GetModuleByDef(type, &_tokenizemodule))

#include "pycore_runtime.h"
#include "clinic/Python-tokenize.c.h"

/*[clinic input]
module _tokenizer
class _tokenizer.tokenizeriter "tokenizeriterobject *" "_tokenize_get_state_by_type(type)->TokenizerIter"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=96d98ee2fef7a8bc]*/

typedef struct
{
    PyObject_HEAD PyTokenizer *tok;
    int done;

    /* Needed to cache line for performance */
    PyObject *last_line;
    Py_ssize_t last_lineno;
    Py_ssize_t last_end_lineno;
    Py_ssize_t byte_col_offset_diff;
} tokenizeriterobject;

/*[clinic input]
@classmethod
_tokenizer.tokenizeriter.__new__ as tokenizeriter_new

    readline: object
    /
    *
    extra_tokens: bool
    encoding: str(c_default="NULL") = 'utf-8'
[clinic start generated code]*/

static PyObject *
tokenizeriter_new_impl(PyTypeObject *type, PyObject *readline,
                       int extra_tokens, const char *encoding)
/*[clinic end generated code: output=7501a1211683ce16 input=f7dddf8a613ae8bd]*/
{
    tokenizeriterobject *self = (tokenizeriterobject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    PyObject *filename = PyUnicode_FromString("<string>");
    if (filename == NULL) {
        Py_DECREF(self);
        return NULL;
    }
    _PyTok_Config config = {
        .kind = _PYTOK_SOURCE_READLINE,
        .source.readline = {readline, encoding},
        .exec_input = 1,
        .preserve_crlf = 1,
        .extra_tokens = extra_tokens,
        .filename = filename,
    };
    self->tok = _PyTok_New(&config);
    if (self->tok == NULL) {
        Py_DECREF(filename);
        Py_DECREF(self);
        return NULL;
    }
    Py_DECREF(filename);
    self->done = 0;

    self->last_line = NULL;
    self->byte_col_offset_diff = 0;
    self->last_lineno = 0;
    self->last_end_lineno = 0;

    return (PyObject *)self;
}

static int
_tokenizer_error(tokenizeriterobject *it)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(it);
    if (PyErr_Occurred()) {
        return -1;
    }

    const _PyTok_Error *error = _PyTok_GetError(it->tok);
    if (error != NULL && error->kind == _PYTOK_ERR_EOF_IN_CONSTRUCT) {
        PyErr_SetString(PyExc_SyntaxError,
                        "unexpected EOF in multi-line statement");
        PyErr_SyntaxLocationObject(
            _PyTok_Filename(it->tok), _PyTok_Lineno(it->tok),
            _PyTok_RunColumn(it->tok));
        return -1;
    }

    return _PyTok_RaiseError(it->tok);
}

static PyObject *
_get_current_line(tokenizeriterobject *it, const _PyTok_Token *token,
                  int type, int *line_changed)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(it);
    PyObject *line;
    if (token->start.lineno != it->last_lineno ||
            token->end.lineno != it->last_end_lineno) {
        Py_ssize_t size;
        const char *view;
        if (type == ENDMARKER ||
                (type == DEDENT && _PyTok_InputExhausted(it->tok))) {
            view = _PyTok_CurrentRunView(it->tok, &size);
        }
        else if (ISSTRINGLIT(type)) {
            view = _PyTok_LineRangeView(
                it->tok, token->start.lineno, token->end.lineno, &size);
        }
        else {
            view = _PyTok_LineView(it->tok, token->end.lineno, &size);
        }
        if (view == NULL) {
            return NULL;
        }
        if (size > 0 && (token->flags & _PYTOK_IMPLICIT_NL)) {
            size--;
        }
        Py_XDECREF(it->last_line);
        line = PyUnicode_DecodeUTF8(view, size, "replace");
        it->last_line = line;
        it->byte_col_offset_diff = 0;
    }
    else {
        line = it->last_line;
        *line_changed = 0;
    }
    return line;
}

static int
_get_col_offsets(tokenizeriterobject *it, const _PyTok_Token *token,
                 PyObject *line, int line_changed, Py_ssize_t *col_offset,
                 Py_ssize_t *end_col_offset)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(it);
    _PyTok_Loc start;
    _PyTok_Loc end;
    if (_PyTok_TokenSpanLocations(it->tok, token, &start, &end) < 0) {
        return -1;
    }
    *col_offset = start.col;
    *end_col_offset = end.col;
    if (start.col >= 0) {
        if (line_changed) {
            *col_offset = _PyPegen_byte_offset_to_character_offset_line(
                line, 0, start.col);
            if (*col_offset < 0) {
                return -1;
            }
            it->byte_col_offset_diff = start.col - *col_offset;
        }
        else {
            *col_offset = start.col - it->byte_col_offset_diff;
        }
    }
    if (end.col >= 0) {
        if (start.lineno == end.lineno) {
            Py_ssize_t width = _PyPegen_byte_offset_to_character_offset_line(
                line, start.col, end.col);
            if (width < 0) {
                return -1;
            }
            *end_col_offset = *col_offset + width;
            it->byte_col_offset_diff += end.col - start.col - width;
        }
        else {
            Py_ssize_t size;
            const char *end_line = _PyTok_LineView(
                it->tok, end.lineno, &size);
            if (end_line == NULL) {
                return -1;
            }
            *end_col_offset = _PyPegen_byte_offset_to_character_offset_raw(
                end_line, end.col);
            if (*end_col_offset < 0) {
                return -1;
            }
            it->byte_col_offset_diff += end.col - *end_col_offset;
        }
    }
    it->last_lineno = start.lineno;
    it->last_end_lineno = end.lineno;
    return 0;
}

static PyObject *
tokenizeriter_next(PyObject *op)
{
    tokenizeriterobject *it = (tokenizeriterobject*)op;
    PyObject* result = NULL;

    Py_BEGIN_CRITICAL_SECTION(it);

    _PyTok_Token token;
    _PyTok_TokenInit(&token);
    int type = ERRORTOKEN;

    _PyTok_Status status = _PyTok_Get(it->tok, &token);
    if (status == _PYTOK_ERROR) {
        _tokenizer_error(it);
        assert(PyErr_Occurred());
        goto exit;
    }
    type = token.type;
    if (it->done) {
        PyErr_SetString(PyExc_StopIteration, "EOF");
        it->done = 1;
        goto exit;
    }
    PyObject *str = NULL;
    Py_ssize_t token_len;
    const char *token_view = _PyTok_TokenView(it->tok, &token, &token_len);
    str = token_view == NULL
        ? NULL
        : PyUnicode_FromStringAndSize(token_view, token_len);
    if (str == NULL) {
        goto exit;
    }

    int is_trailing_token = 0;
    if (type == ENDMARKER ||
            (type == DEDENT && _PyTok_InputExhausted(it->tok))) {
        is_trailing_token = 1;
    }

    PyObject* line = NULL;
    int line_changed = 1;
    if (_PyTok_ExtraTokens(it->tok) && is_trailing_token) {
        line = Py_GetConstant(Py_CONSTANT_EMPTY_STR);
    }
    else {
        line = _get_current_line(it, &token, type, &line_changed);
    }
    if (line == NULL) {
        Py_DECREF(str);
        goto exit;
    }

    Py_ssize_t lineno = token.start.lineno;
    Py_ssize_t end_lineno = token.end.lineno;
    Py_ssize_t col_offset = -1;
    Py_ssize_t end_col_offset = -1;
    if (_PyTok_ExtraTokens(it->tok) && is_trailing_token) {
        lineno = end_lineno = lineno + 1;
        col_offset = end_col_offset = 0;
    }
    else {
        if (_get_col_offsets(it, &token, line, line_changed, &col_offset,
                             &end_col_offset) < 0) {
            Py_DECREF(str);
            Py_DECREF(line);
            goto exit;
        }
    }

    if (_PyTok_ExtraTokens(it->tok)) {
        // Necessary adjustments to match the original Python tokenize
        // implementation
        if (type > DEDENT && type < OP) {
            type = OP;
        }
        else if (type == NEWLINE) {
            Py_DECREF(str);
            if (!(token.flags & _PYTOK_IMPLICIT_NL)) {
                if (token_len > 0 && token_view[0] == '\r') {
                    str = PyUnicode_FromString("\r\n");
                } else {
                    str = PyUnicode_FromString("\n");
                }
            }
            end_col_offset++;
        }
        else if (type == NL) {
            if (token.flags & _PYTOK_IMPLICIT_NL) {
                Py_DECREF(str);
                str = Py_GetConstant(Py_CONSTANT_EMPTY_STR);
            }
        }

        if (str == NULL) {
            Py_DECREF(line);
            goto exit;
        }
    }

    result = Py_BuildValue("(iN(nn)(nn)O)", type, str, lineno, col_offset, end_lineno, end_col_offset, line);
exit:
    _PyTok_TokenClear(&token);
    if (type == ENDMARKER) {
        it->done = 1;
    }

    Py_END_CRITICAL_SECTION();
    return result;
}

static int
tokenizeriter_traverse(PyObject *op, visitproc visit, void *arg)
{
    tokenizeriterobject *it = (tokenizeriterobject *)op;
    int result = _PyTok_Traverse(it->tok, visit, arg);
    if (result != 0) {
        return result;
    }
    Py_VISIT(it->last_line);
    return 0;
}

static int
tokenizeriter_clear(PyObject *op)
{
    tokenizeriterobject *it = (tokenizeriterobject *)op;
    PyTokenizer *tok = it->tok;
    it->tok = NULL;
    Py_CLEAR(it->last_line);
    _PyTok_Free(tok);
    return 0;
}

static void
tokenizeriter_dealloc(PyObject *op)
{
    tokenizeriterobject *it = (tokenizeriterobject*)op;
    PyTypeObject *tp = Py_TYPE(it);
    PyObject_GC_UnTrack(op);
    tokenizeriter_clear(op);
    tp->tp_free(it);
    Py_DECREF(tp);
}

static PyType_Slot tokenizeriter_slots[] = {
    {Py_tp_new, tokenizeriter_new},
    {Py_tp_dealloc, tokenizeriter_dealloc},
    {Py_tp_traverse, tokenizeriter_traverse},
    {Py_tp_clear, tokenizeriter_clear},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, tokenizeriter_next},
    {0, NULL},
};

static PyType_Spec tokenizeriter_spec = {
    .name = "_tokenize.TokenizerIter",
    .basicsize = sizeof(tokenizeriterobject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_HAVE_GC),
    .slots = tokenizeriter_slots,
};

static int
tokenizemodule_exec(PyObject *m)
{
    tokenize_state *state = get_tokenize_state(m);
    if (state == NULL) {
        return -1;
    }

    state->TokenizerIter = (PyTypeObject *)PyType_FromModuleAndSpec(m, &tokenizeriter_spec, NULL);
    if (state->TokenizerIter == NULL) {
        return -1;
    }
    if (PyModule_AddType(m, state->TokenizerIter) < 0) {
        return -1;
    }

    return 0;
}

static PyMethodDef tokenize_methods[] = {
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static PyModuleDef_Slot tokenizemodule_slots[] = {
    _Py_ABI_SLOT,
    {Py_mod_exec, tokenizemodule_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL}
};

static int
tokenizemodule_traverse(PyObject *m, visitproc visit, void *arg)
{
    tokenize_state *state = get_tokenize_state(m);
    Py_VISIT(state->TokenizerIter);
    return 0;
}

static int
tokenizemodule_clear(PyObject *m)
{
    tokenize_state *state = get_tokenize_state(m);
    Py_CLEAR(state->TokenizerIter);
    return 0;
}

static void
tokenizemodule_free(void *m)
{
    tokenizemodule_clear((PyObject *)m);
}

static struct PyModuleDef _tokenizemodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_tokenize",
    .m_size = sizeof(tokenize_state),
    .m_slots = tokenizemodule_slots,
    .m_methods = tokenize_methods,
    .m_traverse = tokenizemodule_traverse,
    .m_clear = tokenizemodule_clear,
    .m_free = tokenizemodule_free,
};

PyMODINIT_FUNC
PyInit__tokenize(void)
{
    return PyModuleDef_Init(&_tokenizemodule);
}
