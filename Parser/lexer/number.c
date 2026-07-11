#include "Python.h"
#include "pycore_token.h"

#include "lexer_internal.h"
#include "../tokenizer/errors.h"
#include "../tokenizer/helpers.h"

#define is_potential_identifier_char(c) (\
              (c >= 'a' && c <= 'z')\
               || (c >= 'A' && c <= 'Z')\
               || (c >= '0' && c <= '9')\
               || c == '_'\
               || (c >= 128))

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
lookahead(struct _PyTokenizer *tok, const char *test)
{
    int distance = 0;
    while (test[distance] != '\0') {
        if (_PyTok_CursorPeek(tok, distance) != test[distance]) {
            return 0;
        }
        distance++;
    }
    return !is_potential_identifier_char(
        _PyTok_CursorPeek(tok, distance));
}

static int
verify_end_of_number(struct _PyTokenizer *tok, int c, const char *kind) {
    if (tok->extra_tokens) {
        // When we are parsing extra tokens, we don't want to emit warnings
        // about invalid literals, because we want to be a bit more liberal.
        return 1;
    }
    /* Emit a deprecation warning only if the numeric literal is immediately
     * followed by one of keywords which can occur after a numeric literal
     * in valid code: "and", "else", "for", "if", "in", "is" and "or".
     * It allows to gradually deprecate existing valid code without adding
     * warning before error in most cases of invalid numeric literal (which
     * would be confusing and break existing tests).
     * Raise a syntax error with slightly better message than plain
     * "invalid syntax" if the numeric literal is immediately followed by
     * other keyword or identifier.
     */
    int r = 0;
    if (c == 'a') {
        r = lookahead(tok, "nd");
    }
    else if (c == 'e') {
        r = lookahead(tok, "lse");
    }
    else if (c == 'f') {
        r = lookahead(tok, "or");
    }
    else if (c == 'i') {
        int c2 = _PyTok_CursorPeek(tok, 0);
        if (c2 == 'f' || c2 == 'n' || c2 == 's') {
            r = 1;
        }
    }
    else if (c == 'o') {
        r = lookahead(tok, "r");
    }
    else if (c == 'n') {
        r = lookahead(tok, "ot");
    }
    if (r) {
        _PyTok_Off after = _PyTok_CursorMark(tok);
        _PyTok_Loc loc = source_location(tok, after - 1);
        if (tok->seam->warn(
                tok, _PYTOK_WARN_KEYWORD_LITERAL, kind, 0, loc))
        {
            return 0;
        }
    }
    else /* In future releases, only error will remain. */
    if (c < 128 && is_potential_identifier_char(c)) {
        if (c != EOF) {
            _PyTok_CursorReset(tok, tok->cursor.pos - 1);
        }
        _PyTok_SyntaxError(tok, "invalid %s literal", kind);
        return 0;
    }
    return 1;
}

static int
scan_decimal_tail(struct _PyTokenizer *tok)
{
    int c;

    while (1) {
        do {
            c = _PyTok_CursorAdvance(tok);
        } while (Py_ISDIGIT(c));
        if (c != '_') {
            break;
        }
        c = _PyTok_CursorAdvance(tok);
        if (!Py_ISDIGIT(c)) {
            if (c != EOF) {
                _PyTok_CursorReset(tok, tok->cursor.pos - 1);
            }
            _PyTok_SyntaxError(tok, "invalid decimal literal");
            return 0;
        }
    }
    return c;
}

static int
emit_number(struct _PyTokenizer *tok, _PyTok_Token *token, int c,
            _PyTok_Off start)
{
    if (c != EOF) {
        _PyTok_CursorReset(tok, tok->cursor.pos - 1);
    }
    return _PyTok_EmitToken(tok, token, NUMBER, start, tok->cursor.pos);
}

static int
lex_exponent(struct _PyTokenizer *tok, _PyTok_Token *token,
             _PyTok_Off start, int exponent)
{
    int c = _PyTok_CursorAdvance(tok);
    if (c == '+' || c == '-') {
        c = _PyTok_CursorAdvance(tok);
        if (!Py_ISDIGIT(c)) {
            if (c != EOF) {
                _PyTok_CursorReset(tok, tok->cursor.pos - 1);
            }
            return _PyTok_EmitToken(
                tok, token,
                _PyTok_SyntaxError(tok, "invalid decimal literal"),
                -1, -1);
        }
    }
    else if (!Py_ISDIGIT(c)) {
        if (c != EOF) {
            _PyTok_CursorReset(tok, tok->cursor.pos - 1);
        }
        if (!verify_end_of_number(tok, exponent, "decimal")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        if (exponent != EOF) {
            _PyTok_CursorReset(tok, tok->cursor.pos - 1);
        }
        return _PyTok_EmitToken(
            tok, token, NUMBER, start, tok->cursor.pos);
    }
    c = scan_decimal_tail(tok);
    if (c == 0) {
        return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
    }
    if (c == 'j' || c == 'J') {
        c = _PyTok_CursorAdvance(tok);
        if (!verify_end_of_number(tok, c, "imaginary")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
    }
    else if (!verify_end_of_number(tok, c, "decimal")) {
        return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
    }
    return emit_number(tok, token, c, start);
}

int
_PyTok_LexFraction(struct _PyTokenizer *tok, _PyTok_Token *token,
             _PyTok_Off start, int c)
{
    if (Py_ISDIGIT(c)) {
        c = scan_decimal_tail(tok);
        if (c == 0) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
    }
    if (c == 'e' || c == 'E') {
        return lex_exponent(tok, token, start, c);
    }
    if (c == 'j' || c == 'J') {
        c = _PyTok_CursorAdvance(tok);
        if (!verify_end_of_number(tok, c, "imaginary")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
    }
    else if (!verify_end_of_number(tok, c, "decimal")) {
        return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
    }
    return emit_number(tok, token, c, start);
}

int
_PyTok_LexNumber(struct _PyTokenizer *tok, _PyTok_Token *token, int first)
{
    _PyTok_Off start = tok->start;
    int c;
    if (first != '0') {
        c = scan_decimal_tail(tok);
        if (c == 0) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        if (c == '.') {
            return _PyTok_LexFraction(tok, token, start, _PyTok_CursorAdvance(tok));
        }
        if (c == 'e' || c == 'E') {
            return lex_exponent(tok, token, start, c);
        }
        if (c == 'j' || c == 'J') {
            c = _PyTok_CursorAdvance(tok);
            if (!verify_end_of_number(tok, c, "imaginary")) {
                return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
            }
        }
        else if (!verify_end_of_number(tok, c, "decimal")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        return emit_number(tok, token, c, start);
    }

    c = _PyTok_CursorAdvance(tok);
    if (c == 'x' || c == 'X') {
        c = _PyTok_CursorAdvance(tok);
        do {
            if (c == '_') {
                c = _PyTok_CursorAdvance(tok);
            }
            if (!Py_ISXDIGIT(c)) {
                if (c != EOF) {
                    _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                }
                return _PyTok_EmitToken(
                    tok, token,
                    _PyTok_SyntaxError(
                        tok, "invalid hexadecimal literal"),
                    -1, -1);
            }
            do {
                c = _PyTok_CursorAdvance(tok);
            } while (Py_ISXDIGIT(c));
        } while (c == '_');
        if (!verify_end_of_number(tok, c, "hexadecimal")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        return emit_number(tok, token, c, start);
    }
    if (c == 'o' || c == 'O') {
        c = _PyTok_CursorAdvance(tok);
        do {
            if (c == '_') {
                c = _PyTok_CursorAdvance(tok);
            }
            if (c < '0' || c >= '8') {
                if (Py_ISDIGIT(c)) {
                    return _PyTok_EmitToken(
                        tok, token,
                        _PyTok_SyntaxError(
                            tok, "invalid digit '%c' in octal literal", c),
                        -1, -1);
                }
                if (c != EOF) {
                    _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                }
                return _PyTok_EmitToken(
                    tok, token,
                    _PyTok_SyntaxError(tok, "invalid octal literal"),
                    -1, -1);
            }
            do {
                c = _PyTok_CursorAdvance(tok);
            } while ('0' <= c && c < '8');
        } while (c == '_');
        if (Py_ISDIGIT(c)) {
            return _PyTok_EmitToken(
                tok, token,
                _PyTok_SyntaxError(
                    tok, "invalid digit '%c' in octal literal", c),
                -1, -1);
        }
        if (!verify_end_of_number(tok, c, "octal")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        return emit_number(tok, token, c, start);
    }
    if (c == 'b' || c == 'B') {
        c = _PyTok_CursorAdvance(tok);
        do {
            if (c == '_') {
                c = _PyTok_CursorAdvance(tok);
            }
            if (c != '0' && c != '1') {
                if (Py_ISDIGIT(c)) {
                    return _PyTok_EmitToken(
                        tok, token,
                        _PyTok_SyntaxError(
                            tok, "invalid digit '%c' in binary literal", c),
                        -1, -1);
                }
                if (c != EOF) {
                    _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                }
                return _PyTok_EmitToken(
                    tok, token,
                    _PyTok_SyntaxError(tok, "invalid binary literal"),
                    -1, -1);
            }
            do {
                c = _PyTok_CursorAdvance(tok);
            } while (c == '0' || c == '1');
        } while (c == '_');
        if (Py_ISDIGIT(c)) {
            return _PyTok_EmitToken(
                tok, token,
                _PyTok_SyntaxError(
                    tok, "invalid digit '%c' in binary literal", c),
                -1, -1);
        }
        if (!verify_end_of_number(tok, c, "binary")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        return emit_number(tok, token, c, start);
    }

    int nonzero = 0;
    while (1) {
        if (c == '_') {
            c = _PyTok_CursorAdvance(tok);
            if (!Py_ISDIGIT(c)) {
                if (c != EOF) {
                    _PyTok_CursorReset(tok, tok->cursor.pos - 1);
                }
                return _PyTok_EmitToken(
                    tok, token,
                    _PyTok_SyntaxError(tok, "invalid decimal literal"),
                    -1, -1);
            }
        }
        if (c != '0') {
            break;
        }
        c = _PyTok_CursorAdvance(tok);
    }
    _PyTok_Off zeros_end = tok->cursor.pos;
    if (Py_ISDIGIT(c)) {
        nonzero = 1;
        c = scan_decimal_tail(tok);
        if (c == 0) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
    }
    if (c == '.') {
        return _PyTok_LexFraction(tok, token, start, _PyTok_CursorAdvance(tok));
    }
    if (c == 'e' || c == 'E') {
        return lex_exponent(tok, token, start, c);
    }
    if (c == 'j' || c == 'J') {
        c = _PyTok_CursorAdvance(tok);
        if (!verify_end_of_number(tok, c, "imaginary")) {
            return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
        }
        return emit_number(tok, token, c, start);
    }
    if (nonzero && !tok->extra_tokens) {
        if (c != EOF) {
            _PyTok_CursorReset(tok, tok->cursor.pos - 1);
        }
        return _PyTok_EmitToken(
            tok, token,
            _PyTok_SyntaxErrorRange(
                tok, (int)(tok->start + 1 - tok->cursor.line_start),
                (int)(zeros_end - tok->cursor.line_start),
                "leading zeros in decimal integer literals are not permitted; "
                "use an 0o prefix for octal integers"),
            -1, -1);
    }
    if (!verify_end_of_number(tok, c, "decimal")) {
        return _PyTok_EmitToken(tok, token, ERRORTOKEN, -1, -1);
    }
    return emit_number(tok, token, c, start);
}
