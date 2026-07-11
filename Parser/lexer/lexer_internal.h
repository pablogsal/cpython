#ifndef _PY_LEXER_INTERNAL_H_
#define _PY_LEXER_INTERNAL_H_

#include "state.h"

static inline _PyTok_Frame *
_PyTok_CurrentFrame(struct _PyTokenizer *tok)
{
    assert(tok->frame_index >= 0);
    assert(tok->frame_index < _PYTOK_MAX_FRAMES);
    return &tok->frames[tok->frame_index];
}

static inline int
_PyTok_EmitToken(struct _PyTokenizer *tok, _PyTok_Token *token, int type,
                 _PyTok_Off start, _PyTok_Off end)
{
    assert((start < 0 && end < 0) || (start >= 0 && end >= start));
    token->type = type;
    token->level = tok->level;
    token->span = start < 0
        ? _PyTok_InvalidSpan()
        : _PyTok_SpanFromBounds(start, end);
    token->start = (_PyTok_Loc){tok->lineno, -1};
    token->end = token->start;
    if (start >= 0) {
        if (tok->start >= tok->cursor.line_start) {
            token->start.col = Py_SAFE_DOWNCAST(
                tok->start - tok->cursor.line_start, _PyTok_Off, int);
        }
        else {
            int result = _PyTok_SourceLocation(
                &tok->source, tok->start, &token->start);
            assert(result == 0);
            (void)result;
        }
        token->end.col = Py_SAFE_DOWNCAST(
            tok->cursor.pos - tok->cursor.line_start, _PyTok_Off, int);
    }
    token->flags = tok->implicit_newline ? _PYTOK_IMPLICIT_NL : 0;
    if (!_PyTok_SpanIsValid(token->span) ||
            ((type == DEDENT || type == ENDMARKER) && start == end)) {
        token->flags |= _PYTOK_SYNTH;
    }
    return type;
}
int _PyTok_LexNumber(struct _PyTokenizer *, _PyTok_Token *, int);
int _PyTok_LexFraction(struct _PyTokenizer *, _PyTok_Token *,
                       _PyTok_Off, int);
int _PyTok_CheckStringPrefixes(struct _PyTokenizer *, int, int, int, int,
                              int);
int _PyTok_LexFStringStart(struct _PyTokenizer *, _PyTok_Token *, int);
int _PyTok_LexString(struct _PyTokenizer *, _PyTok_Token *, int);
int _PyTok_LexOperator(struct _PyTokenizer *, _PyTok_Frame *,
                       _PyTok_Token *, int);
int _PyTok_LexNormal(struct _PyTokenizer *, _PyTok_Frame *, _PyTok_Token *);
int _PyTok_LexFStringMiddle(struct _PyTokenizer *, _PyTok_Frame *,
                            _PyTok_Token *);

#endif
