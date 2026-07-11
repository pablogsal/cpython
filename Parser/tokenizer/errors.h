#ifndef Py_TOKENIZER_ERRORS_H
#define Py_TOKENIZER_ERRORS_H

#include "tokenizer.h"

struct _PyTokenizer;

int _PyTok_HasError(const struct _PyTokenizer *);
void _PyTok_RecordError(struct _PyTokenizer *, _PyTok_ErrKind,
                        _PyTok_Loc, _PyTok_Loc, int, const char *);
void _PyTok_RecordCurrentError(struct _PyTokenizer *, _PyTok_ErrKind,
                               const char *);
void _PyTok_RecordNoMemory(struct _PyTokenizer *);
void _PyTok_RecordPending(struct _PyTokenizer *, _PyTok_ErrKind);

#endif
