#include "Python.h"
#include "pycore_token.h"

#include "helpers.h"
#include "errors.h"
#include "../lexer/lexer.h"
#include "../lexer/state.h"
#include "tokenizer.h"

PyTokenizer *
_PyTok_New(const _PyTok_Config *config)
{
    assert(config != NULL);
    PyTokenizer *tok;
    switch (config->kind) {
        case _PYTOK_SOURCE_STRING:
            tok = _PyTok_StateFromString(
                config->source.string, config->exec_input,
                config->preserve_crlf);
            break;
        case _PYTOK_SOURCE_UTF8:
            tok = _PyTok_StateFromUTF8(
                config->source.string, config->exec_input,
                config->preserve_crlf);
            break;
        case _PYTOK_SOURCE_FILE:
            tok = _PyTok_StateFromFile(
                config->source.file.fp, config->source.file.encoding,
                config->source.file.ps1, config->source.file.ps2);
            break;
        case _PYTOK_SOURCE_READLINE:
            tok = _PyTok_StateFromReadline(
                config->source.readline.readline,
                config->source.readline.encoding,
                config->exec_input, config->preserve_crlf);
            break;
        default:
            PyErr_SetString(PyExc_ValueError,
                            "invalid tokenizer source kind");
            return NULL;
    }
    if (tok == NULL) {
        return NULL;
    }
    tok->source_kind = config->kind;
    tok->extra_tokens = config->extra_tokens != 0;
    tok->type_comments = config->type_comments != 0;
    tok->is_interactive = config->interactive != 0;
    tok->filename = Py_XNewRef(config->filename);
    tok->module = Py_XNewRef(config->module);
    return tok;
}

void
_PyTok_Free(PyTokenizer *tok)
{
    if (tok != NULL) {
        _PyTok_StateFree(tok);
    }
}

int
_PyTok_Traverse(const PyTokenizer *tok, visitproc visit, void *arg)
{
    if (tok == NULL) {
        return 0;
    }
    Py_VISIT(tok->filename);
    Py_VISIT(tok->module);
    Py_VISIT(tok->reader.readline);
    Py_VISIT(tok->reader.decoder);
    for (int i = 0; i < 2; i++) {
        Py_VISIT(tok->reader.pending[i].owner);
    }
    return 0;
}

void
_PyTok_RaiseInitError(PyObject *filename)
{
    _PyTok_RaiseInitException(filename);
}

void
_PyTok_TokenInit(_PyTok_Token *token)
{
    *token = (_PyTok_Token){
        .span = {-1, -1},
        .start = {-1, -1},
        .end = {-1, -1},
    };
}

void
_PyTok_TokenClear(_PyTok_Token *token)
{
    Py_CLEAR(token->metadata);
}

_PyTok_Status
_PyTok_Get(PyTokenizer *tok, _PyTok_Token *token)
{
    if (_PyTok_HasError(tok)) {
        return _PYTOK_ERROR;
    }
    if (tok->busy) {
        PyErr_SetString(PyExc_RuntimeError,
                        "tokenizer is already executing");
        _PyTok_RecordCurrentError(tok, _PYTOK_ERR_PROPAGATE, NULL);
        return _PYTOK_ERROR;
    }
    tok->busy = 1;
    _PyTok_Token produced;
    _PyTok_TokenInit(&produced);
    int type = _PyTok_Lex(tok, &produced);
    tok->busy = 0;
    if (type == ERRORTOKEN || _PyTok_HasError(tok)) {
        if (!_PyTok_HasError(tok)) {
            if (PyErr_Occurred()) {
                _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
            }
            else {
                _PyTok_RecordCurrentError(
                    tok, _PYTOK_ERR_SYNTAX,
                    "unknown tokenization error");
            }
        }
        _PyTok_TokenClear(&produced);
        return _PYTOK_ERROR;
    }
    assert(produced.type == type);
    *token = produced;
    return _PYTOK_OK;
}

const char *
_PyTok_TokenView(PyTokenizer *tok, const _PyTok_Token *token,
                 Py_ssize_t *len)
{
    if (!_PyTok_SpanIsValid(token->span)) {
        *len = 0;
        return "";
    }
    return _PyTok_SourceSpanView(&tok->source, token->span, len);
}

PyObject *
_PyTok_TokenBytes(PyTokenizer *tok, const _PyTok_Token *token)
{
    Py_ssize_t len;
    const char *view = _PyTok_TokenView(tok, token, &len);
    if (view == NULL) {
        return NULL;
    }
    return PyBytes_FromStringAndSize(view, len);
}

static int
tokenizer_line(const PyTokenizer *tok, int lineno, _PyTok_Line *line)
{
    if (lineno == tok->lineno && lineno > 0) {
        *line = (_PyTok_Line){
            .start = tok->cursor.line_start,
            .end = tok->cursor.line_end,
            .implicit_newline = tok->implicit_newline,
        };
        return 0;
    }
    return _PyTok_SourceLine(&tok->source, lineno, line);
}

int
_PyTok_TokenSpanLocations(PyTokenizer *tok, const _PyTok_Token *token,
                          _PyTok_Loc *start, _PyTok_Loc *end)
{
    *start = token->start;
    *end = token->end;
    if (!_PyTok_SpanIsValid(token->span)) {
        return 0;
    }
    _PyTok_Line start_line;
    _PyTok_Line end_line;
    if (tokenizer_line(tok, token->start.lineno, &start_line) < 0 ||
            tokenizer_line(tok, token->end.lineno, &end_line) < 0 ||
            token->span.start < start_line.start ||
            token->span.end < end_line.start) {
        PyErr_SetString(PyExc_SystemError,
                        "cannot map tokenizer span to source lines");
        return -1;
    }
    start->col = Py_SAFE_DOWNCAST(
        token->span.start - start_line.start, _PyTok_Off, int);
    end->col = Py_SAFE_DOWNCAST(
        token->span.end - end_line.start, _PyTok_Off, int);
    return 0;
}

void
_PyTok_FlushIndentation(PyTokenizer *tok)
{
    tok->pendin = -tok->indent;
    tok->indent = 0;
}

void
_PyTok_StopPrompting(PyTokenizer *tok)
{
    if (tok->reader.kind == _PYTOK_READER_INTERACTIVE) {
        tok->reader.stopped = 1;
    }
}

int
_PyTok_Lineno(const PyTokenizer *tok)
{
    return tok->lineno;
}

int
_PyTok_Column(const PyTokenizer *tok)
{
    return Py_SAFE_DOWNCAST(
        tok->cursor.pos - tok->cursor.line_start, Py_ssize_t, int);
}

int
_PyTok_RunColumn(const PyTokenizer *tok)
{
    return Py_SAFE_DOWNCAST(
        tok->cursor.pos - tok->cursor.run_start, Py_ssize_t, int);
}

int
_PyTok_ParenDepth(const PyTokenizer *tok)
{
    return tok->level;
}

void
_PyTok_ParenEntry(const PyTokenizer *tok, int index, char *ch,
                  int *lineno, int *col)
{
    assert(index >= 0 && index < tok->level);
    if (ch != NULL) {
        *ch = tok->parenstack[index];
    }
    if (lineno != NULL) {
        *lineno = tok->parenlinenostack[index];
    }
    if (col != NULL) {
        *col = tok->parencolstack[index];
    }
}

int
_PyTok_FStringDepth(const PyTokenizer *tok)
{
    int depth = 0;
    for (int i = 1; i <= tok->frame_index; i++) {
        depth += tok->frames[i].kind == _PYTOK_FRAME_FSTRING_BODY;
    }
    return depth;
}

int
_PyTok_FStringRaw(const PyTokenizer *tok)
{
    if (tok->frame_index <= 0) {
        return 0;
    }
    return tok->frames[tok->frame_index].raw;
}

char
_PyTok_StringPrefix(const PyTokenizer *tok)
{
    if (tok->frame_index <= 0) {
        return 'f';
    }
    _PyTok_Frame mode = tok->frames[tok->frame_index];
    return mode.string_kind == _PYTOK_TSTRING ? 't' : 'f';
}

int
_PyTok_IndentDepth(const PyTokenizer *tok)
{
    return tok->indent;
}

const char *
_PyTok_Encoding(const PyTokenizer *tok)
{
    return tok->encoding;
}

PyObject *
_PyTok_Filename(const PyTokenizer *tok)
{
    return tok->filename;
}

PyObject *
_PyTok_Module(const PyTokenizer *tok)
{
    return tok->module;
}

int
_PyTok_IsInteractive(const PyTokenizer *tok)
{
    return tok->is_interactive;
}

int
_PyTok_IsPrompting(const PyTokenizer *tok)
{
    return tok->is_prompting;
}

_PyTok_SourceKind
_PyTok_GetSourceKind(const PyTokenizer *tok)
{
    return (_PyTok_SourceKind)tok->source_kind;
}

int
_PyTok_InputExhausted(const PyTokenizer *tok)
{
    return tok->source.complete;
}

int
_PyTok_ExtraTokens(const PyTokenizer *tok)
{
    return tok->extra_tokens;
}

const char *
_PyTok_SourceView(PyTokenizer *tok, Py_ssize_t *len)
{
    if (_PyTok_ReaderBufferRemaining(tok) < 0) {
        return NULL;
    }
    *len = tok->source.len;
    return tok->source.bytes != NULL ? tok->source.bytes : "";
}

const char *
_PyTok_LineView(const PyTokenizer *tok, int lineno, Py_ssize_t *len)
{
    _PyTok_Line line;
    if (tokenizer_line(tok, lineno, &line) < 0) {
        PyErr_SetString(PyExc_IndexError, "tokenizer line is unavailable");
        return NULL;
    }
    return _PyTok_SourceSpanView(
        &tok->source, _PyTok_SpanFromBounds(line.start, line.end), len);
}

int
_PyTok_LineIsImplicit(const PyTokenizer *tok, int lineno)
{
    return _PyTok_SourceLineIsImplicit(&tok->source, lineno);
}

const char *
_PyTok_LineRangeView(const PyTokenizer *tok, int first, int last,
                     Py_ssize_t *len)
{
    _PyTok_Line start;
    _PyTok_Line end;
    if (tokenizer_line(tok, first, &start) < 0 ||
            tokenizer_line(tok, last, &end) < 0 ||
            end.end < start.start) {
        PyErr_SetString(PyExc_IndexError, "tokenizer line range is unavailable");
        return NULL;
    }
    return _PyTok_SourceSpanView(
        &tok->source, _PyTok_SpanFromBounds(start.start, end.end), len);
}

static const char *
offset_range_view(const PyTokenizer *tok, _PyTok_Off start,
                  _PyTok_Off end, Py_ssize_t *len)
{
    if (start < 0 || end < start) {
        PyErr_SetString(PyExc_SystemError,
                        "cannot map tokenizer offset to retained source");
        return NULL;
    }
    return _PyTok_SourceSpanView(
        &tok->source, _PyTok_SpanFromBounds(start, end), len);
}

const char *
_PyTok_RemainingView(PyTokenizer *tok, Py_ssize_t *len)
{
    if (_PyTok_ReaderBufferRemaining(tok) < 0) {
        return NULL;
    }
    return offset_range_view(tok, tok->cursor.pos, tok->source.len, len);
}

const char *
_PyTok_CurrentRunView(const PyTokenizer *tok, Py_ssize_t *len)
{
    return offset_range_view(
        tok, tok->cursor.run_start, tok->cursor.line_end, len);
}
