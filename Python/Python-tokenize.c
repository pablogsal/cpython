#include "Python.h"
#include "Parser/tokenizer.h"

static struct PyModuleDef _tokenizemodule;

typedef struct {
    PyTypeObject *TokenizerIter;
} tokenize_state;

static tokenize_state*
get_tokenize_state(PyObject *module)
{
    return (tokenize_state*)PyModule_GetState(module);
}

static tokenize_state *
tokenize_find_state_by_type(PyTypeObject *tp)
{
    PyObject *m = _PyType_GetModuleByDef(tp, &_tokenizemodule);
    assert(m != NULL);
    return get_tokenize_state(m);
}
#define clinic_find_state() tokenize_find_state_by_type(type)

#include "clinic/Python-tokenize.c.h"

/*[clinic input]
module _tokenizer
class _tokenizer.tokenizeriter "tokenizeriterobject *" "clinic_find_state()->TokenizerIter"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=234c30cda2bee15d]*/



typedef struct {
    PyObject_HEAD
    struct tok_state *tok;
} tokenizeriterobject;

/*[clinic input]
@classmethod
_tokenizer.tokenizeriter.__new__ as tokenizeriter_new

    readline: object
    encoding: str
    /
[clinic start generated code]*/

static PyObject *
tokenizeriter_new_impl(PyTypeObject *type, PyObject *readline,
                       const char *encoding)
/*[clinic end generated code: output=6cb601519163ec24 input=2d3e008380a2742c]*/
{
    tokenizeriterobject* self = (tokenizeriterobject*) type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    self->tok = tok_new();
    if (self->tok == NULL) {
        return NULL;
    }
    self->tok->filename = PyUnicode_FromString("<string>");
    self->tok->blech = 1;
    if ((self->tok->buf = (char *)PyMem_Malloc(BUFSIZ)) == NULL) {
        PyTokenizer_Free(self->tok);
        return NULL;
    }
    self->tok->cur = self->tok->inp = self->tok->buf;
    self->tok->end = self->tok->buf + BUFSIZ;
    self->tok->fp = NULL;
    self->tok->input = NULL;
    self->tok->decoding_state = STATE_NORMAL;
    self->tok->read_coding_spec = 1;
    self->tok->enc = NULL;
    self->tok->encoding = (char *)PyMem_Malloc(strlen(encoding)+1);
    if (!self->tok->encoding) {
        PyTokenizer_Free(self->tok);
        return NULL;
    }
    strcpy(self->tok->encoding, encoding);

    Py_INCREF(readline);
    self->tok->decoding_readline = readline;
    return (PyObject*) self;
}


static PyObject *
tokenizeriter_next(tokenizeriterobject *it)
{
    const char *start;
    const char *end;
    int type = PyTokenizer_Get(it->tok, &start, &end);
    if (type == ERRORTOKEN && PyErr_Occurred()) {
        return NULL;
    }
    if (type == ENDMARKER) {
        PyErr_SetString(PyExc_StopIteration, "EOF");
        return NULL;
    }
    PyObject* str = PyUnicode_FromStringAndSize(start, end - start);
    if (str == NULL) {
        return NULL;
    }

    Py_ssize_t size = it->tok->inp - it->tok->buf;
    PyObject* line = PyUnicode_DecodeUTF8(it->tok->buf, size, "replace");
    if (line == NULL) {
        Py_DECREF(str);
        return NULL;
    }
    const char *line_start = type == STRING ? it->tok->multi_line_start : it->tok->line_start;
    int lineno = type == STRING ? it->tok->first_lineno : it->tok->lineno;
    int end_lineno = it->tok->lineno;
    int col_offset = -1;
    int end_col_offset = -1;
    if (start != NULL && start >= line_start) {
        col_offset = (int)(start - line_start);
    }
    if (end != NULL && end >= it->tok->line_start) {
        end_col_offset = (int)(end - it->tok->line_start);
    }
    return Py_BuildValue("(NiiiiiN)", str, type, lineno, end_lineno, col_offset, end_col_offset, line);
}

static void
tokenizeriter_dealloc(tokenizeriterobject *it)
{
    PyTokenizer_Free(it->tok);
    PyObject_Free(it);
}

static PyType_Slot tokenizeriter_slots[] = {
        {Py_tp_new, tokenizeriter_new},
        {Py_tp_dealloc, tokenizeriter_dealloc},
        {Py_tp_getattro, PyObject_GenericGetAttr},
        {Py_tp_iter, PyObject_SelfIter},
        {Py_tp_iternext, tokenizeriter_next},
        {0, NULL},
};

static PyType_Spec tokenizeriter_spec = {
        .name = "_tokenize.tokenizeriterobject",
        .basicsize = sizeof(tokenizeriterobject),
        .flags = Py_TPFLAGS_DEFAULT,
        .slots = tokenizeriter_slots,
};

#define CREATE_TYPE(module, type, spec)                                  \
do {                                                                     \
    type = (PyTypeObject *)PyType_FromModuleAndSpec(module, spec, NULL); \
    if (type == NULL) {                                                  \
        return -1;                                                       \
    }                                                                    \
} while (0)

static int
tokenizemodule_exec(PyObject *m) {
    tokenize_state *state = get_tokenize_state(m);
    if (state == NULL) {
        return -1;
    }
    CREATE_TYPE(m, state->TokenizerIter, &tokenizeriter_spec);
    Py_INCREF((PyObject *)state->TokenizerIter);
    if (PyModule_AddObject(m, "TokenizerIter", (PyObject *)state->TokenizerIter) < 0) {
        Py_DECREF((PyObject *)state->TokenizerIter);
        return -1;
    }

    if (PyModule_AddObjectRef(m, "lel", Py_None) < 0) {
        return -1;
    }
    return 0;
}

static PyMethodDef tokenize_methods[] = {
        {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyModuleDef_Slot tokenizemodule_slots[] = {
        {Py_mod_exec, tokenizemodule_exec},
        {0, NULL}
};

static struct PyModuleDef _tokenizemodule = {
        PyModuleDef_HEAD_INIT,
        .m_name = "_tokenize",
        // The _tokenize module uses a per-interpreter state (PyInterpreterState.tokenize)
        .m_size = 0,
        .m_slots = tokenizemodule_slots,
        .m_methods = tokenize_methods,
};

PyMODINIT_FUNC
PyInit__tokenize(void)
{
    return PyModuleDef_Init(&_tokenizemodule);
}
