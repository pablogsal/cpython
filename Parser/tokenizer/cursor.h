#ifndef Py_TOKENIZER_CURSOR_H
#define Py_TOKENIZER_CURSOR_H

#include "source.h"

struct _PyTokenizer;

typedef struct {
    _PyTok_Off pos;
    _PyTok_Off line_start;
    _PyTok_Off line_end;
    _PyTok_Off run_start;
    const char *cache;
} _PyTok_Cursor;

void _PyTok_CursorInit(_PyTok_Cursor *);
void _PyTok_CursorRefresh(struct _PyTokenizer *);
void _PyTok_CursorSetLine(struct _PyTokenizer *, _PyTok_Off, _PyTok_Off, int);
int _PyTok_CursorAdvance(struct _PyTokenizer *);
int _PyTok_CursorPeek(const struct _PyTokenizer *, int);
_PyTok_Off _PyTok_CursorMark(const struct _PyTokenizer *);
void _PyTok_CursorReset(struct _PyTokenizer *, _PyTok_Off);

#endif
