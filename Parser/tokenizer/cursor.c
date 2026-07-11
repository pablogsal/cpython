#include "Python.h"

#include "cursor.h"
#include "errors.h"
#include "helpers.h"
#include "../lexer/state.h"

void
_PyTok_CursorInit(_PyTok_Cursor *cursor)
{
    *cursor = (_PyTok_Cursor){0};
}

void
_PyTok_CursorRefresh(struct _PyTokenizer *tok)
{
    tok->cursor.cache = tok->source.bytes;
}

void
_PyTok_CursorSetLine(struct _PyTokenizer *tok, _PyTok_Off start,
                     _PyTok_Off end, int reset_run)
{
    tok->cursor.pos = start;
    tok->cursor.line_start = start;
    tok->cursor.line_end = end;
    if (reset_run) {
        tok->cursor.run_start = start;
    }
    _PyTok_CursorRefresh(tok);
}

static int
current_line_is_valid(struct _PyTokenizer *tok)
{
    if (memchr(tok->source.bytes + tok->cursor.line_start, 0,
               tok->cursor.line_end - tok->cursor.line_start) == NULL) {
        return 1;
    }
    _PyTok_SyntaxError(tok, "source code cannot contain null bytes");
    tok->cursor.pos = tok->cursor.line_end;
    return 0;
}

int
_PyTok_CursorAdvance(struct _PyTokenizer *tok)
{
    for (;;) {
        if (tok->cursor.pos < tok->cursor.line_end) {
            _PyTok_Off column = tok->cursor.pos - tok->cursor.line_start;
            if (column >= INT_MAX) {
                _PyTok_RecordCurrentError(
                    tok, _PYTOK_ERR_COLUMN_OVERFLOW, NULL);
                return EOF;
            }
            return Py_CHARMASK(tok->cursor.cache[tok->cursor.pos++]);
        }
        if (tok->lineno < tok->source.nlines) {
            _PyTok_Off start = tok->cursor.line_end;
            _PyTok_Off end = tok->source.last_line_end;
            if (tok->lineno + 1 < tok->source.nlines) {
                const char *newline = memchr(
                    tok->source.bytes + start, '\n',
                    tok->source.len - start);
                assert(newline != NULL);
                end = newline - tok->source.bytes + 1;
            }
            int reset_run = tok->start < 0 && !INSIDE_FSTRING(tok);
            _PyTok_CursorSetLine(tok, start, end, reset_run);
            tok->lineno++;
            tok->implicit_newline = _PyTok_SourceLineIsImplicit(
                &tok->source, tok->lineno);
            if (!current_line_is_valid(tok)) {
                return EOF;
            }
            continue;
        }
        if (_PyTok_HasError(tok) || tok->source.complete) {
            return EOF;
        }
        int result = _PyTok_ReaderUnderflow(tok);
#ifdef Py_DEBUG
        if (tok->debug) {
            fprintf(stderr, "line[%d] = ", tok->lineno);
            _PyTok_PrintEscape(
                stderr, tok->cursor.cache + tok->cursor.pos,
                tok->cursor.line_end - tok->cursor.pos);
            fprintf(stderr, "  tok->error = %d\n", tok->error.kind);
        }
#endif
        if (!result) {
            tok->cursor.pos = tok->cursor.line_end;
            return EOF;
        }
        if (!current_line_is_valid(tok)) {
            return EOF;
        }
    }
}

int
_PyTok_CursorPeek(const struct _PyTokenizer *tok, int distance)
{
    assert(distance >= 0);
    if ((_PyTok_Off)distance > PY_SSIZE_T_MAX - tok->cursor.pos) {
        return EOF;
    }
    _PyTok_Off offset = tok->cursor.pos + distance;
    if (offset >= tok->cursor.line_end) {
        return EOF;
    }
    return Py_CHARMASK(tok->cursor.cache[offset]);
}

_PyTok_Off
_PyTok_CursorMark(const struct _PyTokenizer *tok)
{
    return tok->cursor.pos;
}

void
_PyTok_CursorReset(struct _PyTokenizer *tok, _PyTok_Off offset)
{
    if (offset < tok->cursor.line_start || offset > tok->cursor.line_end) {
        _PyTok_Loc loc;
        int result = _PyTok_SourceLocation(&tok->source, offset, &loc);
        assert(result == 0);
        (void)result;
        _PyTok_Line line;
        result = _PyTok_SourceLine(&tok->source, loc.lineno, &line);
        assert(result == 0);
        tok->cursor.line_start = line.start;
        tok->cursor.line_end = line.end;
        tok->implicit_newline = line.implicit_newline;
        tok->lineno = loc.lineno;
        _PyTok_CursorRefresh(tok);
    }
    tok->cursor.pos = offset;
}
