#include "Python.h"
#include "pycore_pystate.h"
#include "pycore_token.h"

#include "state.h"

/* Never change this */
#define TABSIZE 8

struct _PyTokenizer *
_PyTok_StateNew(void)
{
    struct _PyTokenizer *tok = (struct _PyTokenizer *)PyMem_Calloc(
                                            1,
                                            sizeof(struct _PyTokenizer));
    if (tok == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    tok->start = -1;
    _PyTok_SourceInit(&tok->source);
    _PyTok_CursorInit(&tok->cursor);
    _PyTok_ReaderInit(&tok->reader);
    tok->seam = _PyTok_DefaultSeam();
    tok->tabsize = TABSIZE;
    tok->indent = 0;
    tok->indstack[0] = 0;
    tok->atbol = 1;
    tok->pendin = 0;
    tok->lineno = 0;
    tok->level = 0;
    tok->altindstack[0] = 0;
    tok->encoding = NULL;
    tok->filename = NULL;
    tok->module = NULL;
    tok->type_comments = 0;
    tok->extra_tokens = 0;
    tok->comment_newline = 0;
    tok->implicit_newline = 0;
    tok->frames[0] = (_PyTok_Frame){
        .kind = _PYTOK_FRAME_TOPLEVEL,
        .quote = '\0',
        .quote_size = 0,
        .body_start = -1,
        .debug_expr_start = -1,
        .debug_expr_end = -1,
        .in_debug = 0,
    };
    tok->frame_index = 0;
#ifdef Py_DEBUG
    tok->debug = _Py_GetConfig()->parser_debug;
#endif
    return tok;
}

void
_PyTok_StateFree(struct _PyTokenizer *tok)
{
    if (tok->encoding != NULL) {
        PyMem_Free(tok->encoding);
    }
    PyMem_Free(tok->error_detail);
    Py_XDECREF(tok->filename);
    Py_XDECREF(tok->module);
    _PyTok_SourceClear(&tok->source);
    _PyTok_ReaderClear(&tok->reader);
    PyMem_Free(tok);
}
