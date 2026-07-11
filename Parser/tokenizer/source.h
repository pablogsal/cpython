#ifndef Py_TOKENIZER_SOURCE_H
#define Py_TOKENIZER_SOURCE_H

#include "Python.h"

typedef Py_ssize_t _PyTok_Off;

typedef struct {
    _PyTok_Off start;
    _PyTok_Off end;
} _PyTok_Span;

typedef struct {
    int lineno;
    int col;
} _PyTok_Loc;

typedef struct {
    _PyTok_Off start;
    _PyTok_Off end;
    unsigned implicit_newline : 1;
    unsigned contains_nul : 1;
} _PyTok_Line;

typedef struct {
    char *bytes;
    _PyTok_Off len;
    _PyTok_Off cap;
    _PyTok_Off *line_checkpoints;
    unsigned char *implicit_lines;
    int nlines;
    int ncheckpoints;
    int checkpoints_cap;
    Py_ssize_t implicit_bytes;
    _PyTok_Off last_line_end;
    unsigned complete : 1;
} _PyTok_SourceText;

void _PyTok_SourceInit(_PyTok_SourceText *source);
void _PyTok_SourceClear(_PyTok_SourceText *source);
_PyTok_Off _PyTok_SourceAppend(_PyTok_SourceText *source,
                               const char *bytes, Py_ssize_t len);
int _PyTok_SourceAddLine(_PyTok_SourceText *source, _PyTok_Off start,
                         _PyTok_Off end, int implicit_newline);
const char *_PyTok_SourceSpanView(const _PyTok_SourceText *source,
                                  _PyTok_Span span, Py_ssize_t *len);
int _PyTok_SourceLine(const _PyTok_SourceText *source, int lineno,
                      _PyTok_Line *line);
int _PyTok_SourceLineIsImplicit(const _PyTok_SourceText *source, int lineno);
int _PyTok_SourceLocation(const _PyTok_SourceText *source,
                          _PyTok_Off offset, _PyTok_Loc *loc);

static inline _PyTok_Span
_PyTok_SpanFromBounds(_PyTok_Off start, _PyTok_Off end)
{
    return (_PyTok_Span){start, end};
}

static inline _PyTok_Span
_PyTok_InvalidSpan(void)
{
    return (_PyTok_Span){-1, -1};
}

static inline int
_PyTok_SpanIsValid(_PyTok_Span span)
{
    return span.start >= 0 && span.end >= span.start;
}

#endif
