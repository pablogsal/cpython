#include "Python.h"
#include "pycore_token.h"
#include "pycore_unicodeobject.h"

#include "lexer_internal.h"
#include "../tokenizer/errors.h"
#include "../tokenizer/helpers.h"

static inline int
frame_middle_token(const _PyTok_Frame *frame)
{
    return frame->string_kind == _PYTOK_TSTRING
        ? TSTRING_MIDDLE
        : FSTRING_MIDDLE;
}

static inline int
frame_end_token(const _PyTok_Frame *frame)
{
    return frame->string_kind == _PYTOK_TSTRING
        ? TSTRING_END
        : FSTRING_END;
}

static inline char
current_string_prefix(struct _PyTokenizer *tok)
{
    return _PyTok_CurrentFrame(tok)->string_kind == _PYTOK_TSTRING ? 't' : 'f';
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
frame_body_depth(struct _PyTokenizer *tok)
{
    int depth = 0;
    for (int i = 1; i <= tok->frame_index; i++) {
        depth += tok->frames[i].kind == _PYTOK_FRAME_FSTRING_BODY;
    }
    return depth;
}

static int
frame_body_paren_depth(struct _PyTokenizer *tok)
{
    for (int i = tok->frame_index; i > 0; i--) {
        _PyTok_Frame *frame = &tok->frames[i];
        if (frame->kind == _PYTOK_FRAME_FSTRING_BODY) {
            return frame->paren_depth_at_entry;
        }
    }
    Py_UNREACHABLE();
}

static _PyTok_Frame *
frame_push(struct _PyTokenizer *tok, _PyTok_FrameKind kind)
{
    if (tok->frame_index + 1 >= _PYTOK_MAX_FRAMES) {
        _PyTok_SyntaxError(tok, "too many nested tokenizer frames");
        return NULL;
    }
    _PyTok_Frame *parent = _PyTok_CurrentFrame(tok);
    _PyTok_Frame *frame = &tok->frames[++tok->frame_index];
    *frame = (_PyTok_Frame){
        .kind = kind,
        .paren_depth_at_entry = tok->level,
        .quote = parent->quote,
        .quote_size = parent->quote_size,
        .raw = parent->raw,
        .body_start = parent->body_start,
        .debug_expr_start = -1,
        .debug_expr_end = -1,
        .string_kind = parent->string_kind,
    };
    return frame;
}

static _PyTok_Frame *
frame_pop(struct _PyTokenizer *tok)
{
    assert(tok->frame_index > 0);
    tok->frame_index--;
    _PyTok_Frame *frame = _PyTok_CurrentFrame(tok);
    if (frame->kind == _PYTOK_FRAME_FORMAT_SPEC) {
        frame->spec_degraded = 1;
    }
    return frame;
}

static void
frame_pop_fstring(struct _PyTokenizer *tok)
{
    while (tok->frame_index > 0 &&
            _PyTok_CurrentFrame(tok)->kind != _PYTOK_FRAME_FSTRING_BODY) {
        frame_pop(tok);
    }
    assert(tok->frame_index > 0);
    frame_pop(tok);
}

static int
frame_expr_depth(struct _PyTokenizer *tok)
{
    int depth = 0;
    for (int i = tok->frame_index; i > 0; i--) {
        _PyTok_Frame *frame = &tok->frames[i];
        if (frame->kind == _PYTOK_FRAME_FSTRING_BODY) {
            break;
        }
        depth += frame->kind == _PYTOK_FRAME_FSTRING_EXPR;
    }
    return depth;
}

static int
materialize_frame_metadata(struct _PyTokenizer* tok, _PyTok_Token *token, char c) {
    assert(token != NULL);
    assert(c == '}' || c == ':' || c == '!');
    _PyTok_Frame *frame = _PyTok_CurrentFrame(tok);

    if (!(frame->in_debug || frame->string_kind == _PYTOK_TSTRING) || token->metadata) {
        return 0;
    }
    if (frame->debug_expr_start < 0 ||
            frame->debug_expr_end < frame->debug_expr_start ||
            frame->debug_expr_end > tok->source.len) {
        _PyTok_RecordCurrentError(
            tok, _PYTOK_ERR_SYNTAX,
            "invalid f-string expression span");
        return -1;
    }
    _PyTok_Span span = _PyTok_SpanFromBounds(
        frame->debug_expr_start, frame->debug_expr_end);
    return tok->seam->intern_metadata(tok, span, &token->metadata);
}

static void
update_debug_span(struct _PyTokenizer *tok, char cur)
{
    assert(tok->cursor.cache != NULL);
    _PyTok_Frame *frame = _PyTok_CurrentFrame(tok);

    switch (cur) {
        case 0:
            break;
        case '{':
            frame->debug_expr_start = tok->cursor.pos;
            frame->debug_expr_end = -1;
            break;
        case '}':
        case '!':
            frame->debug_expr_end = tok->start;
            break;
        case ':':
            if (frame->debug_expr_end < 0) {
                frame->debug_expr_end = tok->start;
            }
            break;
        default:
            Py_UNREACHABLE();
    }
}

static int
incompatible_prefixes(struct _PyTokenizer *tok, const char *first,
                      const char *second)
{
    (void)_PyTok_SyntaxErrorRange(
        tok, (int)(tok->start + 1 - tok->cursor.line_start),
        (int)(tok->cursor.pos - tok->cursor.line_start),
        "'%s' and '%s' prefixes are incompatible", first, second);
    tok->error.columns_are_chars = 1;
    return -1;
}

int
_PyTok_CheckStringPrefixes(struct _PyTokenizer *tok,
                                             int saw_b, int saw_r, int saw_u,
                                             int saw_f, int saw_t) {
    if (saw_u && saw_b) {
        return incompatible_prefixes(tok, "u", "b");
    }
    if (saw_u && saw_r) {
        return incompatible_prefixes(tok, "u", "r");
    }
    if (saw_u && saw_f) {
        return incompatible_prefixes(tok, "u", "f");
    }
    if (saw_u && saw_t) {
        return incompatible_prefixes(tok, "u", "t");
    }

    if (saw_b && saw_f) {
        return incompatible_prefixes(tok, "b", "f");
    }
    if (saw_b && saw_t) {
        return incompatible_prefixes(tok, "b", "t");
    }

    if (saw_f && saw_t) {
        return incompatible_prefixes(tok, "f", "t");
    }

    return 0;
}

int
_PyTok_LexFStringStart(struct _PyTokenizer *tok, _PyTok_Token *token, int quote)
{
    _PyTok_Off start = tok->start;
    const char *token_start = tok->source.bytes + start;
    int quote_size = 1;
    if (_PyTok_CursorPeek(tok, 0) == quote &&
            _PyTok_CursorPeek(tok, 1) == quote) {
        (void)_PyTok_CursorAdvance(tok);
        (void)_PyTok_CursorAdvance(tok);
        quote_size = 3;
    }

    _PyTok_Off end = tok->cursor.pos;
    if (frame_body_depth(tok) >= MAXFSTRINGLEVEL) {
        return _PyTok_EmitToken(
            tok, token,
            _PyTok_SyntaxError(
                tok, "too many nested f-strings or t-strings"),
            start, end);
    }
    _PyTok_Frame *frame = frame_push(tok, _PYTOK_FRAME_FSTRING_BODY);
    if (frame == NULL) {
        return _PyTok_EmitToken(tok, token, ERRORTOKEN, start, end);
    }
    frame->quote = quote;
    frame->quote_size = quote_size;
    frame->body_start = start;

    _PyTok_StringKind string_kind = _PYTOK_FSTRING;
    switch (*token_start) {
        case 'T':
        case 't':
            frame->raw = Py_TOLOWER(token_start[1]) == 'r';
            string_kind = _PYTOK_TSTRING;
            break;
        case 'F':
        case 'f':
            frame->raw = Py_TOLOWER(token_start[1]) == 'r';
            break;
        case 'R':
        case 'r':
            frame->raw = 1;
            if (Py_TOLOWER(token_start[1]) == 't') {
                string_kind = _PYTOK_TSTRING;
            }
            break;
        default:
            Py_UNREACHABLE();
    }
    frame->string_kind = string_kind;
    int type = string_kind == _PYTOK_TSTRING ? TSTRING_START : FSTRING_START;
    return _PyTok_EmitToken(tok, token, type, start, end);
}

int
_PyTok_LexString(struct _PyTokenizer *tok, _PyTok_Token *token, int quote)
{
    _PyTok_Off start = tok->start;
    int quote_size = 1;
    int end_quote_size = 0;
    int has_escaped_quote = 0;
    if (_PyTok_CursorPeek(tok, 0) == quote &&
            _PyTok_CursorPeek(tok, 1) == quote) {
        (void)_PyTok_CursorAdvance(tok);
        (void)_PyTok_CursorAdvance(tok);
        quote_size = 3;
    }

    while (end_quote_size != quote_size) {
        int c = _PyTok_CursorAdvance(tok);
        if (_PyTok_HasError(tok)) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        if (c == EOF || (quote_size == 1 && c == '\n')) {
            int detected_line = tok->lineno;
            _PyTok_Loc loc = source_location(tok, start + 1);
            if (INSIDE_FSTRING(tok)) {
                _PyTok_Frame *frame = _PyTok_CurrentFrame(tok);
                if (frame->quote == quote && frame->quote_size == quote_size) {
                    int type = _PyTok_FormattedErrorAt(
                        tok, _PYTOK_ERR_SYNTAX, loc, loc, loc.lineno,
                        "%c-string: expecting '}'",
                        current_string_prefix(tok));
                    return _PyTok_EmitToken(tok, token, type, -1, -1);
                }
            }

            _PyTok_ErrKind kind = _PYTOK_ERR_SYNTAX;
            if (c != '\n') {
                kind = quote_size == 3
                    ? _PYTOK_ERR_EOF_IN_STRING
                    : _PYTOK_ERR_EOL_IN_STRING;
            }
            if (quote_size == 3) {
                _PyTok_FormattedErrorAt(
                    tok, kind, loc, loc, loc.lineno,
                    "unterminated triple-quoted string literal"
                    " (detected at line %d)", detected_line);
            }
            else if (has_escaped_quote) {
                _PyTok_FormattedErrorAt(
                    tok, kind, loc, loc, loc.lineno,
                    "unterminated string literal (detected at line %d); "
                    "perhaps you escaped the end quote?", detected_line);
            }
            else {
                _PyTok_FormattedErrorAt(
                    tok, kind, loc, loc, loc.lineno,
                    "unterminated string literal (detected at line %d)",
                    detected_line);
            }
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        if (c == quote) {
            end_quote_size++;
        }
        else {
            end_quote_size = 0;
            if (c == '\\') {
                c = _PyTok_CursorAdvance(tok);
                if (c == quote) {
                    has_escaped_quote = 1;
                }
                if (c == '\r') {
                    (void)_PyTok_CursorAdvance(tok);
                }
            }
        }
    }
    return _PyTok_EmitToken(tok, token, STRING, start, tok->cursor.pos);
}

int
_PyTok_LexOperator(struct _PyTokenizer *tok, _PyTok_Frame *frame,
                   _PyTok_Token *token, int c)
{
    _PyTok_Off p_start = -1;
    _PyTok_Off p_end = -1;
    /* Punctuation character */
    int is_punctuation = (c == ':' || c == '}' || c == '!' || c == '{');
    int is_not_equal = c == '!' && _PyTok_CursorPeek(tok, 0) == '=';
    if (is_punctuation && !is_not_equal && INSIDE_FSTRING(tok) &&
            INSIDE_FSTRING_EXPR(frame)) {
        int relative_depth =
            tok->level - frame->paren_depth_at_entry - (c != '{');
        int valid_depth = relative_depth == 0 ||
            (relative_depth == 1 && frame->in_debug);
        if (valid_depth) {
            update_debug_span(tok, c);
        }
        if (valid_depth && c != '{' &&
                materialize_frame_metadata(tok, token, c)) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
        }

        if (c == ':' && relative_depth == 0) {
            if (frame_push(tok, _PYTOK_FRAME_FORMAT_SPEC) == NULL) {
                return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
            }
            p_start = tok->start;
            p_end = tok->cursor.pos;
            return _PyTok_EmitToken(tok, token, _PyToken_OneChar(c), p_start, p_end);
        }
    }

    int c2 = _PyTok_CursorPeek(tok, 0);
    int current_token = _PyToken_TwoChars(c, c2);
    if (current_token != OP) {
        (void)_PyTok_CursorAdvance(tok);
        int c3 = _PyTok_CursorPeek(tok, 0);
        int current_token3 = _PyToken_ThreeChars(c, c2, c3);
        if (current_token3 != OP) {
            (void)_PyTok_CursorAdvance(tok);
            current_token = current_token3;
        }
        p_start = tok->start;
        p_end = tok->cursor.pos;
        return _PyTok_EmitToken(tok, token, current_token, p_start, p_end);
    }

    /* Keep track of parentheses nesting level */
    switch (c) {
    case '(':
    case '[':
    case '{':
        if (tok->level >= MAXLEVEL) {
            return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok, "too many nested parentheses"), p_start, p_end);
        }
        tok->parenstack[tok->level] = c;
        tok->parenlinenostack[tok->level] = tok->lineno;
        tok->parencolstack[tok->level] = (int)(
            tok->start - tok->cursor.line_start);
        tok->level++;
        break;
    case ')':
    case ']':
    case '}':
        if (INSIDE_FSTRING_EXPR(frame) && c != '}' &&
                tok->level == frame->paren_depth_at_entry) {
            return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(
                tok, "%c-string: unmatched '%c'",
                current_string_prefix(tok), c), p_start, p_end);
        }
        if (INSIDE_FSTRING_EXPR(frame) && c == '}' &&
                tok->level == frame_body_paren_depth(tok)) {
            return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok,
                "%c-string: single '}' is not allowed", current_string_prefix(tok)), p_start, p_end);
        }
        if (!tok->extra_tokens && !tok->level) {
            return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok, "unmatched '%c'", c), p_start, p_end);
        }
        if (tok->level > 0) {
            tok->level--;
            int opening = tok->parenstack[tok->level];
            if (!tok->extra_tokens && !((opening == '(' && c == ')') ||
                                            (opening == '[' && c == ']') ||
                                            (opening == '{' && c == '}'))) {
                /* If the opening bracket belongs to an f-string's expression
                part (e.g. f"{)}") and the closing bracket is an arbitrary
                nested expression, then instead of matching a different
                syntactical construct with it; we'll throw an unmatched
                parentheses error. */
                if (INSIDE_FSTRING_EXPR(frame) && opening == '{') {
                    if (tok->level == frame->paren_depth_at_entry) {
                        return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok,
                            "%c-string: unmatched '%c'", current_string_prefix(tok), c), p_start, p_end);
                    }
                }
                if (tok->parenlinenostack[tok->level] != tok->lineno) {
                    return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok,
                            "closing parenthesis '%c' does not match "
                            "opening parenthesis '%c' on line %d",
                            c, opening, tok->parenlinenostack[tok->level]), p_start, p_end);
                }
                else {
                    return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok,
                            "closing parenthesis '%c' does not match "
                            "opening parenthesis '%c'",
                            c, opening), p_start, p_end);
                }
            }
        }

        if (INSIDE_FSTRING_EXPR(frame) && c == '}' &&
                tok->level == frame->paren_depth_at_entry) {
            frame->in_debug = 0;
            frame_pop(tok);
        }
        break;
    default:
        break;
    }

    if (!Py_UNICODE_ISPRINTABLE(c)) {
        return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok, "invalid non-printable character U+%04X", c), p_start, p_end);
    }

    if (c == '=' && INSIDE_FSTRING_EXPR_AT_TOP(tok, frame)) {
        frame->in_debug = 1;
    }

    /* Punctuation character */
    p_start = tok->start;
    p_end = tok->cursor.pos;
    return _PyTok_EmitToken(tok, token, _PyToken_OneChar(c), p_start, p_end);
}

int
_PyTok_LexFStringMiddle(struct _PyTokenizer *tok, _PyTok_Frame* frame,
                        _PyTok_Token *token)
{
    _PyTok_Off p_start = -1;
    _PyTok_Off p_end = -1;
    int end_quote_size = 0;
    int unicode_escape = 0;
    int active_format_spec =
        frame->kind == _PYTOK_FRAME_FORMAT_SPEC &&
        !frame->spec_degraded;

    tok->start = tok->cursor.pos;
    int start_char = _PyTok_CursorAdvance(tok);
    if (start_char == '{') {
        if (_PyTok_CursorPeek(tok, 0) != '{') {
            if (frame_expr_depth(tok) >= MAX_EXPR_NESTING) {
                return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok,
                    "%c-string: expressions nested too deeply", current_string_prefix(tok)), p_start, p_end);
            }
            _PyTok_Frame *expr = frame_push(tok, _PYTOK_FRAME_FSTRING_EXPR);
            if (expr == NULL) {
                return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
            }
            update_debug_span(tok, start_char);
            _PyTok_CursorReset(tok, tok->start);
            return _PyTok_LexNormal(tok, expr, token);
        }
        _PyTok_CursorReset(tok, tok->start);
    }
    else if (start_char == '}' &&
             frame->kind == _PYTOK_FRAME_FSTRING_BODY) {
        if (_PyTok_CursorPeek(tok, 0) != '}') {
            if (!frame->pending_single_brace) {
                frame->pending_single_brace = 1;
                _PyTok_CursorReset(tok, tok->start);
                p_start = tok->start;
                p_end = tok->start;
                return _PyTok_EmitToken(tok, token, frame_middle_token(frame), p_start, p_end);
            }
            return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok,
                "%c-string: single '}' is not allowed", current_string_prefix(tok)), p_start, p_end);
        }
        _PyTok_CursorReset(tok, tok->start);
    }
    else {
        if (start_char != EOF) {
            _PyTok_CursorReset(tok, tok->start);
        }
    }

    int at_end = 1;
    for (int i = 0; i < frame->quote_size; i++) {
        if (_PyTok_CursorPeek(tok, i) != frame->quote) {
            at_end = 0;
            break;
        }
    }
    if (at_end) {
        for (int i = 0; i < frame->quote_size; i++) {
            (void)_PyTok_CursorAdvance(tok);
        }
        p_start = tok->start;
        p_end = tok->cursor.pos;
        int end_token = frame_end_token(frame);
        frame_pop_fstring(tok);
        return _PyTok_EmitToken(tok, token, end_token, p_start, p_end);
    }

    while (end_quote_size != frame->quote_size) {
        int c = _PyTok_CursorAdvance(tok);
        if (_PyTok_HasError(tok)) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
        }

       if (c == EOF || (frame->quote_size == 1 && c == '\n')) {
            if (active_format_spec && c == '\n') {
                if (frame->quote_size == 1) {
                    return _PyTok_EmitToken(tok, token,
                        _PyTok_SyntaxError(
                            tok,
                            "%c-string: newlines are not allowed in format specifiers for single quoted %c-strings",
                            current_string_prefix(tok), current_string_prefix(tok)
                        )
                    , p_start, p_end);
                }
                if (c != EOF) {
                    _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                }
                p_start = tok->start;
                p_end = tok->cursor.pos;
                int middle_token = frame_middle_token(frame);
                frame_pop(tok);
                return _PyTok_EmitToken(tok, token, middle_token, p_start, p_end);
            }

            int detected_line = tok->lineno;
            _PyTok_Loc loc = source_location(tok, frame->body_start + 1);

            if (frame->quote_size == 3) {
                _PyTok_ErrKind kind = c != '\n'
                    ? _PYTOK_ERR_EOF_IN_STRING
                    : _PYTOK_ERR_SYNTAX;
                _PyTok_FormattedErrorAt(
                    tok, kind, loc, loc, loc.lineno,
                    "unterminated triple-quoted %c-string literal"
                    " (detected at line %d)",
                    current_string_prefix(tok), detected_line);
                return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
            }
            else {
                return _PyTok_EmitToken(tok, token, _PyTok_FormattedErrorAt(
                    tok, _PYTOK_ERR_SYNTAX, loc, loc, loc.lineno,
                    "unterminated %c-string literal (detected at line %d)",
                    current_string_prefix(tok), detected_line), p_start, p_end);
            }
        }

        if (c == frame->quote) {
            end_quote_size += 1;
            continue;
        } else {
            end_quote_size = 0;
        }

        if (c == '{') {
            int peek = _PyTok_CursorPeek(tok, 0);
            if (peek != '{' || active_format_spec) {
                if (frame_expr_depth(tok) >= MAX_EXPR_NESTING) {
                    return _PyTok_EmitToken(tok, token, _PyTok_SyntaxError(tok,
                        "%c-string: expressions nested too deeply", current_string_prefix(tok)), p_start, p_end);
                }
                if (frame_push(tok, _PYTOK_FRAME_FSTRING_EXPR) == NULL) {
                    return _PyTok_EmitToken(tok, token, ERRORTOKEN, p_start, p_end);
                }
                update_debug_span(tok, c);
                if (c != EOF) {
                    _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                }
                p_start = tok->start;
                p_end = tok->cursor.pos;
            } else {
                (void)_PyTok_CursorAdvance(tok);
                p_start = tok->start;
                p_end = tok->cursor.pos - 1;
            }
            return _PyTok_EmitToken(tok, token, frame_middle_token(frame), p_start, p_end);
        } else if (c == '}') {
            if (unicode_escape) {
                p_start = tok->start;
                p_end = tok->cursor.pos;
                return _PyTok_EmitToken(tok, token, frame_middle_token(frame), p_start, p_end);
            }
            if (frame->kind == _PYTOK_FRAME_FORMAT_SPEC) {
                if (c != EOF) {
                    _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                }
                p_start = tok->start;
                p_end = tok->cursor.pos;
                int middle_token = frame_middle_token(frame);
                frame_pop(tok);
                return _PyTok_EmitToken(tok, token, middle_token, p_start, p_end);
            }
            int peek = _PyTok_CursorPeek(tok, 0);
            if (peek == '}') {
                (void)_PyTok_CursorAdvance(tok);
                p_start = tok->start;
                p_end = tok->cursor.pos - 1;
            }
            else {
                _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                frame->pending_single_brace = 1;
                p_start = tok->start;
                p_end = tok->cursor.pos;
            }
            return _PyTok_EmitToken(tok, token, frame_middle_token(frame), p_start, p_end);
        } else if (c == '\\') {
            int peek = _PyTok_CursorPeek(tok, 0);
            if (peek == '{' || peek == '}') {
                _PyTok_Loc loc = {
                    tok->lineno,
                    Py_SAFE_DOWNCAST(
                        tok->cursor.pos + 1 - tok->cursor.line_start,
                        _PyTok_Off, int),
                };
                if (!frame->raw && tok->seam->warn(
                        tok, _PYTOK_WARN_INVALID_ESCAPE, NULL, peek, loc)) {
                    return _PyTok_EmitToken(
                        tok, token, ERRORTOKEN, p_start, p_end);
                }
                continue;
            }

            peek = _PyTok_CursorAdvance(tok);
            if (peek == '\r') {
                peek = _PyTok_CursorAdvance(tok);
            }
            if (!frame->raw && peek == 'N' &&
                    _PyTok_CursorPeek(tok, 0) == '{') {
                (void)_PyTok_CursorAdvance(tok);
                unicode_escape = 1;
            }
        }
    }

    _PyTok_CursorReset(tok, tok->cursor.pos - frame->quote_size);
    p_start = tok->start;
    p_end = tok->cursor.pos;
    return _PyTok_EmitToken(tok, token, frame_middle_token(frame), p_start, p_end);
}
