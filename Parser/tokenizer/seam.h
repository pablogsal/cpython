#ifndef Py_TOKENIZER_SEAM_H
#define Py_TOKENIZER_SEAM_H

#include "tokenizer.h"

struct _PyTokenizer;

typedef enum {
    _PYTOK_WARN_KEYWORD_LITERAL,
    _PYTOK_WARN_INVALID_ESCAPE,
} _PyTok_WarnKind;

typedef struct {
    int (*warn)(struct _PyTokenizer *, _PyTok_WarnKind,
                const char *, int, _PyTok_Loc);
    int (*verify_identifier)(struct _PyTokenizer *, _PyTok_Span,
                             _PyTok_Off *, unsigned int *);
    int (*intern_metadata)(struct _PyTokenizer *, _PyTok_Span, PyObject **);
} _PyTok_Seam;

const _PyTok_Seam *_PyTok_DefaultSeam(void);
PyAPI_FUNC(void) _PyTok_SetSeam(PyTokenizer *, const _PyTok_Seam *);

#endif
