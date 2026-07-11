#include "Python.h"
#include "pycore_token.h"
#include "pycore_unicodeobject.h"

#include "state.h"
#include "lexer_internal.h"
#include "../tokenizer/helpers.h"
#include "../tokenizer/errors.h"

/* Alternate tab spacing */
#define ALTTABSIZE 1

#define is_potential_identifier_start(c) (\
              (c >= 'a' && c <= 'z')\
               || (c >= 'A' && c <= 'Z')\
               || c == '_'\
               || (c >= 128))

#define is_potential_identifier_char(c) (\
              (c >= 'a' && c <= 'z')\
               || (c >= 'A' && c <= 'Z')\
               || (c >= '0' && c <= '9')\
               || c == '_'\
               || (c >= 128))

static int
emit_type_comment(struct _PyTokenizer *tok, _PyTok_Token *token, int type,
                  int col, int end_col, _PyTok_Off start, _PyTok_Off end)
{
    token->type = type;
    token->level = tok->level;
    token->start = (_PyTok_Loc){tok->lineno, col};
    token->end = (_PyTok_Loc){tok->lineno, end_col};
    token->span = start < 0 || end < start
        ? _PyTok_InvalidSpan()
        : _PyTok_SpanFromBounds(start, end);
    token->flags = tok->implicit_newline ? _PYTOK_IMPLICIT_NL : 0;
    return type;
}

static inline void
discard_line(struct _PyTokenizer *tok)
{
    tok->cursor.pos = tok->cursor.line_end;
}

static _PyTok_Loc
source_location(struct _PyTokenizer *tok, _PyTok_Off offset)
{
    _PyTok_Loc loc;
    int result = _PyTok_SourceLocation(&tok->source, offset, &loc);
    assert(result == 0);
    (void)result;
    return loc;
}

static int
source_column(struct _PyTokenizer *tok, _PyTok_Off offset)
{
    return Py_SAFE_DOWNCAST(
        offset - tok->cursor.line_start, _PyTok_Off, int);
}

/* Spaces in this constant are treated as "zero or more spaces or tabs" when
   tokenizing. */
static const char type_comment_prefix[] = "# type: ";

/* Verify that the identifier follows PEP 3131. */
static int
verify_identifier(struct _PyTokenizer *tok)
{
    if (tok->extra_tokens) {
        return 1;
    }
    if (_PyTok_HasError(tok))
        return 0;
    _PyTok_Span span = _PyTok_SpanFromBounds(
        tok->start, tok->cursor.pos);
    _PyTok_Off invalid_end;
    unsigned int ch;
    int result = tok->seam->verify_identifier(
        tok, span, &invalid_end, &ch);
    if (result < 0) {
        return 0;
    }
    if (result == 0) {
        _PyTok_Loc loc = source_location(tok, invalid_end);
        if (Py_UNICODE_ISPRINTABLE(ch)) {
            _PyTok_FormattedErrorAt(
                tok, _PYTOK_ERR_SYNTAX, loc, loc, loc.lineno,
                "invalid character '%c' (U+%04X)", ch, ch);
        }
        else {
            _PyTok_FormattedErrorAt(
                tok, _PYTOK_ERR_SYNTAX, loc, loc, loc.lineno,
                "invalid non-printable character U+%04X", ch);
        }
        return 0;
    }
    return 1;
}

static inline int
lex_line_continuation(struct _PyTokenizer *tok) {
    int c = _PyTok_CursorAdvance(tok);
    if (c == '\r') {
        c = _PyTok_CursorAdvance(tok);
    }
    if (c != '\n') {
        if (!_PyTok_HasError(tok)) {
            int col = Py_SAFE_DOWNCAST(
                tok->cursor.pos - tok->cursor.run_start,
                _PyTok_Off, int);
            _PyTok_Loc loc = {tok->lineno, col};
            discard_line(tok);
            _PyTok_RecordError(
                tok, _PYTOK_ERR_LINECONT, loc, loc, tok->lineno,
                "unexpected character after line continuation character");
        }
        return -1;
    }
    c = _PyTok_CursorAdvance(tok);
    if (c == EOF) {
        int col = Py_SAFE_DOWNCAST(
            tok->cursor.line_end - tok->cursor.line_start,
            _PyTok_Off, int);
        _PyTok_Loc loc = {tok->lineno, col};
        _PyTok_RecordError(tok, _PYTOK_ERR_EOF_IN_CONSTRUCT,
                           loc, loc, tok->lineno, NULL);
        discard_line(tok);
        return -1;
    } else {
        if (c != EOF) {
            _PyTok_CursorReset(tok, tok->cursor.pos - 1);
        }
    }
    return c;
}

static int
lex_name(struct _PyTokenizer *tok, _PyTok_Token *token, int c)
{
    int saw_b = 0;
    int saw_r = 0;
    int saw_u = 0;
    int saw_f = 0;
    int saw_t = 0;
    while (1) {
        if (!saw_b && (c == 'b' || c == 'B')) {
            saw_b = 1;
        }
        else if (!saw_u && (c == 'u'|| c == 'U')) {
            saw_u = 1;
        }
        else if (!saw_r && (c == 'r' || c == 'R')) {
            saw_r = 1;
        }
        else if (!saw_f && (c == 'f' || c == 'F')) {
            saw_f = 1;
        }
        else if (!saw_t && (c == 't' || c == 'T')) {
            saw_t = 1;
        }
        else {
            break;
        }
        c = _PyTok_CursorAdvance(tok);
        if (c == '"' || c == '\'') {
            if (_PyTok_CheckStringPrefixes(
                    tok, saw_b, saw_r, saw_u, saw_f, saw_t) < 0) {
                return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
            }
            if (saw_f || saw_t) {
                return _PyTok_LexFStringStart(tok, token, c);
            }
            return _PyTok_LexString(tok, token, c);
        }
    }

    int nonascii = 0;
    while (is_potential_identifier_char(c)) {
        if (c >= 128) {
            nonascii = 1;
        }
        c = _PyTok_CursorAdvance(tok);
    }
    if (c != EOF) {
        _PyTok_CursorReset(tok, tok->cursor.pos - 1);
    }
    if (nonascii && !verify_identifier(tok)) {
        return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
    }
    return _PyTok_EmitToken(
        tok, token, NAME, tok->start, tok->cursor.pos);
}

static int
lex_indentation(struct _PyTokenizer *tok, _PyTok_Token *token, int *blankline)
{
    _PyTok_Off p_start = -1;
    _PyTok_Off p_end = -1;
    int c;
    /* Get indentation level */
    if (tok->atbol) {
        int col = 0;
        int altcol = 0;
        tok->atbol = 0;
        int cont_line_col = 0;
        for (;;) {
            c = _PyTok_CursorAdvance(tok);
            if (c == ' ') {
                col++, altcol++;
            }
            else if (c == '\t') {
                col = (col / tok->tabsize + 1) * tok->tabsize;
                altcol = (altcol / ALTTABSIZE + 1) * ALTTABSIZE;
            }
            else if (c == '\014')  {/* Control-L (formfeed) */
                col = altcol = 0; /* For Emacs users */
            }
            else if (c == '\\') {
                // Indentation cannot be split over multiple physical lines
                // using backslashes. This means that if we found a backslash
                // preceded by whitespace, **the first one we find** determines
                // the level of indentation of whatever comes next.
                cont_line_col = cont_line_col ? cont_line_col : col;
                if ((c = lex_line_continuation(tok)) == -1) {
                    return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
                }
            }
            else if (c == EOF && _PyTok_HasError(tok)) {
                return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
            }
            else {
                break;
            }
        }
        if (c != EOF) {
            _PyTok_CursorReset(tok, tok->cursor.pos - 1);
        }
        if (c == '#' || c == '\n' || c == '\r') {
            /* Lines with only whitespace and/or comments
               shouldn't affect the indentation and are
               not passed to the parser as NEWLINE tokens,
               except *totally* empty lines in interactive
               mode, which signal the end of a command group. */
            if (col == 0 && c == '\n' && tok->is_prompting) {
                *blankline = 0; /* Let it through */
            }
            else if (tok->is_prompting && tok->lineno == 1) {
                /* In interactive mode, if the first line contains
                   only spaces and/or a comment, let it through. */
                *blankline = 0;
                col = altcol = 0;
            }
            else {
                *blankline = 1; /* Ignore completely */
            }
            /* We can't jump back right here since we still
               may need to skip to the end of a comment */
        }
        if (!*blankline && tok->level == 0) {
            col = cont_line_col ? cont_line_col : col;
            altcol = cont_line_col ? cont_line_col : altcol;
            if (col == tok->indstack[tok->indent]) {
                /* No change */
                if (altcol != tok->altindstack[tok->indent]) {
                    return _PyTok_EmitToken(tok, token, _PyTok_IndentationError(tok), p_start, p_end);
                }
            }
            else if (col > tok->indstack[tok->indent]) {
                /* Indent -- always one */
                if (tok->indent+1 >= MAXINDENT) {
                    discard_line(tok);
                    _PyTok_RecordCurrentError(
                        tok, _PYTOK_ERR_TOODEEP,
                        "too many levels of indentation");
                    return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
                }
                if (altcol <= tok->altindstack[tok->indent]) {
                    return _PyTok_EmitToken(tok, token, _PyTok_IndentationError(tok), p_start, p_end);
                }
                tok->pendin++;
                tok->indstack[++tok->indent] = col;
                tok->altindstack[tok->indent] = altcol;
            }
            else /* col < tok->indstack[tok->indent] */ {
                /* Dedent -- any number, must be consistent */
                while (tok->indent > 0 &&
                    col < tok->indstack[tok->indent]) {
                    tok->pendin--;
                    tok->indent--;
                }
                if (col != tok->indstack[tok->indent]) {
                    discard_line(tok);
                    _PyTok_RecordCurrentError(
                        tok, _PYTOK_ERR_DEDENT_MISMATCH,
                        "unindent does not match any outer indentation level");
                    return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
                }
                if (altcol != tok->altindstack[tok->indent]) {
                    return _PyTok_EmitToken(tok, token, _PyTok_IndentationError(tok), p_start, p_end);
                }
            }
        }
    }

    tok->start = tok->cursor.pos;

    /* Return pending indents/dedents */
    if (tok->pendin != 0) {
        if (tok->pendin < 0) {
            if (tok->extra_tokens) {
                p_start = tok->cursor.pos;
                p_end = tok->cursor.pos;
            }
            tok->pendin++;
            return _PyTok_EmitToken(tok, token, DEDENT, p_start, p_end);
        }
        else {
            if (tok->extra_tokens) {
                p_start = tok->cursor.run_start;
                p_end = tok->cursor.pos;
            }
            tok->pendin--;
            return _PyTok_EmitToken(tok, token, INDENT, p_start, p_end);
        }
    }

    return -1;
}

static int
lex_comment(struct _PyTokenizer *tok, _PyTok_Token *token, int blankline,
            int *next_char)
{
    int c = *next_char;
    if (c != '#') {
        return -1;
    }
    while (c != EOF && c != '\n' && c != '\r') {
        c = _PyTok_CursorAdvance(tok);
    }

    if (tok->type_comments) {
        _PyTok_Off cursor = tok->start;
        _PyTok_Off limit = tok->cursor.pos;
        const char *prefix = type_comment_prefix;
        int start_col = source_column(tok, tok->start);
        while (*prefix && cursor < limit) {
            if (*prefix == ' ') {
                while (cursor < limit &&
                        (tok->source.bytes[cursor] == ' ' ||
                         tok->source.bytes[cursor] == '\t')) {
                    cursor++;
                    start_col++;
                }
            }
            else if (*prefix == tok->source.bytes[cursor]) {
                cursor++;
                start_col++;
            }
            else {
                break;
            }
            prefix++;
        }

        if (*prefix == '\0') {
            _PyTok_Off ignore_end = cursor + 6;
            int ignore_end_col = start_col + 6;
            if (c != EOF) {
                _PyTok_CursorReset(tok, tok->cursor.pos - 1);
            }
            limit = tok->cursor.pos;
            int is_type_ignore =
                limit >= ignore_end &&
                memcmp(tok->source.bytes + cursor, "ignore", 6) == 0 &&
                !(limit > ignore_end &&
                  ((unsigned char)tok->source.bytes[ignore_end] >= 128 ||
                   Py_ISALNUM(tok->source.bytes[ignore_end])));
            if (is_type_ignore) {
                _PyTok_Off end = tok->cursor.pos;
                if (blankline) {
                    (void)_PyTok_CursorAdvance(tok);
                    tok->atbol = 1;
                }
                return emit_type_comment(
                    tok, token, TYPE_IGNORE, ignore_end_col,
                    source_column(tok, tok->cursor.pos), ignore_end, end);
            }
            return emit_type_comment(
                tok, token, TYPE_COMMENT, start_col,
                source_column(tok, tok->cursor.pos), cursor,
                tok->cursor.pos);
        }
    }

    if (tok->extra_tokens) {
        if (c != EOF) {
            _PyTok_CursorReset(tok, tok->cursor.pos - 1);
        }
        tok->comment_newline = blankline;
        return _PyTok_EmitToken(
            tok, token, COMMENT, tok->start, tok->cursor.pos);
    }
    *next_char = c;
    return -1;
}

int
_PyTok_LexNormal(struct _PyTokenizer *tok, _PyTok_Frame *frame,
           _PyTok_Token *token)
{
    _PyTok_Off p_start = -1;
    _PyTok_Off p_end = -1;
    for (;;) {
        tok->start = -1;
        int blankline = 0;
        int indentation = lex_indentation(tok, token, &blankline);
        if (indentation >= 0) {
            return indentation;
        }

        for (;;) {
            tok->start = -1;
            int c;
            do {
                c = _PyTok_CursorAdvance(tok);
            } while (c == ' ' || c == '\t' || c == '\014');

            tok->start = tok->cursor.pos - (c != EOF);

            int comment = lex_comment(tok, token, blankline, &c);
            if (comment >= 0) {
                return comment;
            }

            if (c == EOF) {
                if (_PyTok_HasError(tok)) {
                    return _PyTok_EmitToken(
                        tok, token, ERRORTOKEN, p_start, p_end);
                }
                if (tok->reader.stopped) {
                    return _PyTok_EmitToken(
                        tok, token, ENDMARKER, p_start, p_end);
                }
                if (tok->level) {
                    _PyTok_RecordCurrentError(
                        tok, _PYTOK_ERR_EOF_IN_CONSTRUCT, NULL);
                    return _PyTok_EmitToken(
                        tok, token, ERRORTOKEN, p_start, p_end);
                }
                return _PyTok_EmitToken(
                    tok, token, ENDMARKER, p_start, p_end);
            }

            if (is_potential_identifier_start(c)) {
                return lex_name(tok, token, c);
            }

            if (c == '\r') {
                c = _PyTok_CursorAdvance(tok);
            }

            if (c == '\n') {
                tok->atbol = 1;
                if (blankline || tok->level > 0) {
                    if (tok->extra_tokens) {
                        tok->comment_newline = 0;
                        p_start = tok->start;
                        p_end = tok->cursor.pos;
                        return _PyTok_EmitToken(
                            tok, token, NL, p_start, p_end);
                    }
                    break;
                }
                if (tok->comment_newline && tok->extra_tokens) {
                    tok->comment_newline = 0;
                    p_start = tok->start;
                    p_end = tok->cursor.pos;
                    return _PyTok_EmitToken(
                        tok, token, NL, p_start, p_end);
                }
                p_start = tok->start;
                p_end = tok->cursor.pos - 1;
                return _PyTok_EmitToken(
                    tok, token, NEWLINE, p_start, p_end);
            }

            if (c == '.') {
                int next = _PyTok_CursorPeek(tok, 0);
                if (Py_ISDIGIT(next)) {
                    return _PyTok_LexFraction(
                        tok, token, tok->start, _PyTok_CursorAdvance(tok));
                }
                if (next == '.' && _PyTok_CursorPeek(tok, 1) == '.') {
                    (void)_PyTok_CursorAdvance(tok);
                    (void)_PyTok_CursorAdvance(tok);
                    return _PyTok_EmitToken(
                        tok, token, ELLIPSIS,
                        tok->start, tok->cursor.pos);
                }
                return _PyTok_EmitToken(
                    tok, token, DOT, tok->start, tok->cursor.pos);
            }

            if (Py_ISDIGIT(c)) {
                return _PyTok_LexNumber(tok, token, c);
            }

            if (c == '\'' || c == '"') {
                return _PyTok_LexString(tok, token, c);
            }

            if (c == '\\') {
                if (lex_line_continuation(tok) == -1) {
                    return _PyTok_EmitToken(
                        tok, token, ERRORTOKEN, p_start, p_end);
                }
                continue;
            }

            return _PyTok_LexOperator(tok, frame, token, c);
        }
    }
}

static int
dispatch_token(struct _PyTokenizer *tok, _PyTok_Token *token)
{
    _PyTok_Frame *frame = _PyTok_CurrentFrame(tok);
    if (frame->kind == _PYTOK_FRAME_TOPLEVEL ||
            frame->kind == _PYTOK_FRAME_FSTRING_EXPR) {
        return _PyTok_LexNormal(tok, frame, token);
    }
    return _PyTok_LexFStringMiddle(tok, frame, token);
}

int
_PyTok_Lex(struct _PyTokenizer *tok, _PyTok_Token *token)
{
    int result = dispatch_token(tok, token);
    return _PyTok_HasError(tok) ? ERRORTOKEN : result;
}
