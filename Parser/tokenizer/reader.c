#include "Python.h"
#include "pycore_call.h"
#include "pycore_fileutils.h"

#include "helpers.h"
#include "errors.h"
#include "../lexer/lexer.h"
#include "reader.h"
#include "reader_internal.h"
#include "../lexer/state.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

static _PyTok_ReadResult next_prepared(struct _PyTokenizer *, _PyTok_Chunk *);
static _PyTok_ReadResult next_file(struct _PyTokenizer *, _PyTok_Chunk *);
static _PyTok_ReadResult next_readline(struct _PyTokenizer *, _PyTok_Chunk *);
static _PyTok_ReadResult next_interactive(struct _PyTokenizer *, _PyTok_Chunk *);

void
_PyTok_ChunkReleaseData(_PyTok_Chunk *chunk)
{
    if (chunk->owner != NULL) {
        Py_DECREF(chunk->owner);
    }
    else {
        PyMem_Free(chunk->data);
    }
    chunk->data = NULL;
    chunk->owner = NULL;
}

void
_PyTok_ChunkClear(_PyTok_Chunk *chunk)
{
    _PyTok_ChunkReleaseData(chunk);
    *chunk = (_PyTok_Chunk){0};
}

void
_PyTok_ReaderInit(_PyTok_Reader *reader)
{
    *reader = (_PyTok_Reader){0};
}

void
_PyTok_ReaderClear(_PyTok_Reader *reader)
{
    Py_XDECREF(reader->readline);
    Py_XDECREF(reader->decoder);
    PyMem_Free(reader->prepared);
    PyMem_Free(reader->raw);
    for (int i = 0; i < 2; i++) {
        _PyTok_ChunkClear(&reader->pending[i]);
    }
    _PyTok_ReaderInit(reader);
}

char *
_PyTok_CopyBytes(const char *data, Py_ssize_t len)
{
    if (len < 0 || len == PY_SSIZE_T_MAX) {
        PyErr_NoMemory();
        return NULL;
    }
    char *copy = PyMem_Malloc((size_t)len + 1);
    if (copy == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    memcpy(copy, data, len);
    copy[len] = '\0';
    return copy;
}

int
_PyTok_AppendPreparedSource(struct _PyTokenizer *tok, const char *data,
                       Py_ssize_t len, int implicit_newline)
{
    _PyTok_Off source_start = _PyTok_SourceAppend(&tok->source, data, len);
    if (source_start < 0) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }
    Py_ssize_t line_start = 0;
    while (line_start < len) {
        const char *newline = memchr(
            data + line_start, '\n', len - line_start);
        Py_ssize_t line_end = newline == NULL
            ? len
            : newline - data + 1;
        int implicit = line_end == len &&
            implicit_newline;
        if (_PyTok_SourceAddLine(
                &tok->source, source_start + line_start,
                source_start + line_end, implicit) < 0) {
            _PyTok_RecordNoMemory(tok);
            return -1;
        }
        line_start = line_end;
    }
    return 0;
}

static struct _PyTokenizer *
reader_new(_PyTok_ReaderKind kind)
{
    struct _PyTokenizer *tok = _PyTok_StateNew();
    if (tok == NULL) {
        return NULL;
    }
    tok->reader.kind = kind;
    switch (kind) {
        case _PYTOK_READER_STRING:
        case _PYTOK_READER_UTF8:
            tok->reader.next_line = next_prepared;
            break;
        case _PYTOK_READER_FILE:
            tok->reader.next_line = next_file;
            break;
        case _PYTOK_READER_READLINE:
            tok->reader.next_line = next_readline;
            break;
        case _PYTOK_READER_INTERACTIVE:
            tok->reader.next_line = next_interactive;
            break;
    }
    return tok;
}

struct _PyTokenizer *
_PyTok_StateFromString(const char *input, int exec_input, int preserve_crlf)
{
    struct _PyTokenizer *tok = reader_new(_PYTOK_READER_STRING);
    if (tok == NULL) {
        return NULL;
    }
    tok->reader.exec_input = exec_input;
    tok->reader.preserve_crlf = preserve_crlf;
    if (_PyTok_PrepareString(tok, input, 0) < 0) {
        if (!PyErr_Occurred() && _PyTok_GetError(tok) != NULL) {
            _PyTok_RaiseError(tok);
        }
        _PyTok_StateFree(tok);
        return NULL;
    }
    return tok;
}

struct _PyTokenizer *
_PyTok_StateFromUTF8(const char *input, int exec_input, int preserve_crlf)
{
    struct _PyTokenizer *tok = reader_new(_PYTOK_READER_UTF8);
    if (tok == NULL) {
        return NULL;
    }
    tok->reader.exec_input = exec_input;
    tok->reader.preserve_crlf = preserve_crlf;
    if (_PyTok_PrepareString(tok, input, 1) < 0) {
        if (!PyErr_Occurred() && _PyTok_GetError(tok) != NULL) {
            _PyTok_RaiseError(tok);
        }
        _PyTok_StateFree(tok);
        return NULL;
    }
    return tok;
}

struct _PyTokenizer *
_PyTok_StateFromReadline(PyObject *readline, const char *encoding,
                          int exec_input, int preserve_crlf)
{
    struct _PyTokenizer *tok = reader_new(_PYTOK_READER_READLINE);
    if (tok == NULL) {
        return NULL;
    }
    if (encoding != NULL && _PyTok_SetEncoding(tok, encoding) < 0) {
        if (!PyErr_Occurred() && _PyTok_GetError(tok) != NULL) {
            _PyTok_RaiseError(tok);
        }
        _PyTok_StateFree(tok);
        return NULL;
    }
    tok->reader.readline = Py_NewRef(readline);
    tok->reader.exec_input = exec_input;
    tok->reader.preserve_crlf = preserve_crlf;
    return tok;
}

struct _PyTokenizer *
_PyTok_StateFromFile(FILE *fp, const char *encoding,
                      const char *ps1, const char *ps2)
{
    _PyTok_ReaderKind kind = ps1 || ps2
        ? _PYTOK_READER_INTERACTIVE
        : _PYTOK_READER_FILE;
    struct _PyTokenizer *tok = reader_new(kind);
    if (tok == NULL) {
        return NULL;
    }
    if (encoding != NULL && _PyTok_SetEncoding(tok, encoding) < 0) {
        if (!PyErr_Occurred() && _PyTok_GetError(tok) != NULL) {
            _PyTok_RaiseError(tok);
        }
        _PyTok_StateFree(tok);
        return NULL;
    }
    tok->is_prompting = ps1 != NULL || ps2 != NULL;
    tok->reader.fp = fp;
    tok->reader.prompt = ps1;
    tok->reader.nextprompt = ps2;
    return tok;
}

static _PyTok_ReadResult
next_prepared(struct _PyTokenizer *tok, _PyTok_Chunk *chunk)
{
    _PyTok_Reader *reader = &tok->reader;
    if (reader->prepared_pos >= reader->prepared_len) {
        return _PYTOK_RD_EOF;
    }
    const char *start = reader->prepared + reader->prepared_pos;
    const char *newline = memchr(
        start, '\n', reader->prepared_len - reader->prepared_pos);
    Py_ssize_t len = newline == NULL
        ? reader->prepared_len - reader->prepared_pos
        : newline - start + 1;
    chunk->data = _PyTok_CopyBytes(start, len);
    if (chunk->data == NULL) {
        return _PYTOK_RD_ERROR;
    }
    chunk->len = len;
    chunk->implicit_newline =
        reader->prepared_pos + len == reader->prepared_len &&
        reader->prepared_implicit_newline;
    reader->prepared_pos += len;
    return _PYTOK_RD_LINE;
}

static int
ensure_raw_capacity(_PyTok_Reader *reader, Py_ssize_t needed)
{
    if (needed <= reader->raw_cap) {
        return 0;
    }
    Py_ssize_t cap = reader->raw_cap > 0 ? reader->raw_cap : BUFSIZ;
    while (cap < needed) {
        if (cap > PY_SSIZE_T_MAX / 2) {
            cap = needed;
            break;
        }
        cap *= 2;
    }
    char *raw = PyMem_Realloc(reader->raw, cap);
    if (raw == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    reader->raw = raw;
    reader->raw_cap = cap;
    return 0;
}

static _PyTok_ReadResult
read_file_line(_PyTok_Reader *reader, _PyTok_Chunk *chunk)
{
    Py_ssize_t len = 0;
    for (;;) {
        if (len > PY_SSIZE_T_MAX - BUFSIZ - 1) {
            PyErr_NoMemory();
            return _PYTOK_RD_ERROR;
        }
        if (ensure_raw_capacity(reader, len + BUFSIZ + 1) < 0) {
            return _PYTOK_RD_ERROR;
        }
        int available = (int)Py_MIN(reader->raw_cap - len, INT_MAX);
        size_t read = 0;
        char *result = _Py_UniversalNewlineFgetsWithSize(
            reader->raw + len, available, reader->fp, NULL, &read);
        if (result == NULL) {
            if (len == 0) {
                return _PYTOK_RD_EOF;
            }
            break;
        }
        len += (Py_ssize_t)read;
        if (len > 0 && reader->raw[len - 1] == '\n') {
            break;
        }
    }
    int implicit = len == 0 || reader->raw[len - 1] != '\n';
    Py_ssize_t final_len = len + implicit;
    chunk->data = PyMem_Malloc((size_t)final_len + 1);
    if (chunk->data == NULL) {
        PyErr_NoMemory();
        return _PYTOK_RD_ERROR;
    }
    memcpy(chunk->data, reader->raw, len);
    if (implicit) {
        chunk->data[len] = '\n';
    }
    chunk->data[final_len] = '\0';
    chunk->len = final_len;
    chunk->implicit_newline = implicit;
    return _PYTOK_RD_LINE;
}

static int
initialize_file(struct _PyTokenizer *tok)
{
    _PyTok_Reader *reader = &tok->reader;
    reader->initialized = 1;
    if (tok->encoding != NULL) {
        return _PyTok_StartIncrementalDecoder(tok, "strict");
    }

    _PyTok_ReadResult result = read_file_line(reader, &reader->pending[0]);
    if (result == _PYTOK_RD_EOF) {
        reader->eof = 1;
        return 0;
    }
    if (result != _PYTOK_RD_LINE) {
        return -1;
    }
    reader->pending_count = 1;
    result = read_file_line(reader, &reader->pending[1]);
    if (result == _PYTOK_RD_LINE) {
        reader->pending_count = 2;
    }
    else if (result == _PYTOK_RD_EOF) {
        reader->eof = 1;
    }
    else {
        return -1;
    }
    if (_PyTok_DetectEncoding(tok, &reader->pending[0],
                        &reader->pending[1],
                        reader->pending_count == 2) < 0) {
        return -1;
    }
    if (_PyTok_StartIncrementalDecoder(tok, "strict") < 0) {
        return -1;
    }
    for (int i = 0; i < reader->pending_count; i++) {
        if (_PyTok_DecodeIncremental(tok, &reader->pending[i], 0) < 0) {
            return -1;
        }
    }
    return 0;
}

static _PyTok_ReadResult
next_file(struct _PyTokenizer *tok, _PyTok_Chunk *chunk)
{
    _PyTok_Reader *reader = &tok->reader;
    if (!reader->initialized && initialize_file(tok) < 0) {
        return _PYTOK_RD_ERROR;
    }
    while (reader->pending_index < reader->pending_count) {
        *chunk = reader->pending[reader->pending_index];
        reader->pending[reader->pending_index++] = (_PyTok_Chunk){0};
        if (chunk->len != 0) {
            return _PYTOK_RD_LINE;
        }
        _PyTok_ChunkClear(chunk);
    }
    if (!reader->eof) {
        _PyTok_ReadResult result = read_file_line(reader, chunk);
        if (result == _PYTOK_RD_LINE) {
            if (_PyTok_DecodeIncremental(tok, chunk, 0) < 0) {
                _PyTok_ChunkClear(chunk);
                return _PYTOK_RD_ERROR;
            }
            if (chunk->len == 0) {
                _PyTok_ChunkClear(chunk);
                return next_file(tok, chunk);
            }
            return result;
        }
        if (result == _PYTOK_RD_ERROR) {
            return result;
        }
        reader->eof = 1;
    }
    if (reader->decoder != NULL && !reader->decoder_finalized) {
        reader->decoder_finalized = 1;
        *chunk = (_PyTok_Chunk){.data = _PyTok_CopyBytes("", 0)};
        if (chunk->data == NULL || _PyTok_DecodeIncremental(tok, chunk, 1) < 0) {
            _PyTok_ChunkClear(chunk);
            return _PYTOK_RD_ERROR;
        }
        if (chunk->len != 0) {
            return _PYTOK_RD_LINE;
        }
        _PyTok_ChunkClear(chunk);
    }
    return _PYTOK_RD_EOF;
}

static _PyTok_ReadResult
finish_readline_chunk(_PyTok_Chunk *chunk)
{
    int implicit = chunk->data[chunk->len - 1] != '\n';
    if (implicit) {
        char *data;
        if (chunk->owner == NULL) {
            data = PyMem_Realloc(
                chunk->data, (size_t)chunk->len + 2);
        }
        else {
            data = PyMem_Malloc((size_t)chunk->len + 2);
            if (data != NULL) {
                memcpy(data, chunk->data, (size_t)chunk->len);
                _PyTok_ChunkReleaseData(chunk);
            }
        }
        if (data == NULL) {
            PyErr_NoMemory();
            return _PYTOK_RD_ERROR;
        }
        chunk->data = data;
        chunk->data[chunk->len++] = '\n';
    }
    if (chunk->owner == NULL) {
        chunk->data[chunk->len] = '\0';
    }
    else {
        assert(chunk->data[chunk->len] == '\0');
    }
    chunk->implicit_newline = implicit;
    return _PYTOK_RD_LINE;
}

static _PyTok_ReadResult
next_readline(struct _PyTokenizer *tok, _PyTok_Chunk *chunk)
{
    _PyTok_Reader *reader = &tok->reader;
    if (reader->decoder_finalized) {
        return _PYTOK_RD_EOF;
    }
    for (;;) {
        PyObject *raw = PyObject_CallNoArgs(reader->readline);
        if (raw == NULL) {
            if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
                PyErr_Clear();
                if (reader->decoder == NULL && !reader->utf8_decoder) {
                    return _PYTOK_RD_EOF;
                }
            }
            else {
                return _PYTOK_RD_ERROR;
            }
        }
        int eof = raw == NULL;
        if (tok->encoding != NULL) {
            if (!eof && !PyBytes_Check(raw)) {
                PyErr_SetString(PyExc_TypeError,
                                "readline() returned a non-bytes object");
                Py_DECREF(raw);
                return _PYTOK_RD_ERROR;
            }
            if (reader->decoder == NULL && !reader->utf8_decoder &&
                    _PyTok_StartIncrementalDecoder(tok, "replace") < 0) {
                Py_XDECREF(raw);
                return _PYTOK_RD_ERROR;
            }
            Py_ssize_t raw_len = eof ? 0 : PyBytes_GET_SIZE(raw);
            if (raw_len == 0) {
                eof = 1;
            }
            if (eof) {
                chunk->data = _PyTok_CopyBytes("", 0);
                Py_XDECREF(raw);
                if (chunk->data == NULL) {
                    return _PYTOK_RD_ERROR;
                }
            }
            else {
                chunk->owner = raw;
                chunk->data = PyBytes_AS_STRING(raw);
            }
            chunk->len = raw_len;
            if (_PyTok_DecodeIncremental(tok, chunk, eof) < 0) {
                _PyTok_ChunkClear(chunk);
                return _PYTOK_RD_ERROR;
            }
        }
        else {
            if (eof) {
                return _PYTOK_RD_EOF;
            }
            if (!PyUnicode_Check(raw)) {
                PyErr_SetString(PyExc_TypeError,
                                "readline() returned a non-string object");
                Py_DECREF(raw);
                return _PYTOK_RD_ERROR;
            }
            PyObject *utf8 = PyUnicode_AsUTF8String(raw);
            Py_DECREF(raw);
            if (utf8 == NULL) {
                return _PYTOK_RD_ERROR;
            }
            Py_ssize_t len = PyBytes_GET_SIZE(utf8);
            if (len == 0) {
                Py_DECREF(utf8);
                return _PYTOK_RD_EOF;
            }
            chunk->owner = utf8;
            chunk->data = PyBytes_AS_STRING(utf8);
            chunk->len = len;
        }
        if (eof) {
            reader->decoder_finalized = 1;
        }
        if (chunk->len > 0) {
            _PyTok_ReadResult result = finish_readline_chunk(chunk);
            if (result == _PYTOK_RD_ERROR) {
                _PyTok_ChunkClear(chunk);
            }
            return result;
        }
        _PyTok_ChunkClear(chunk);
        if (eof) {
            return _PYTOK_RD_EOF;
        }
    }
}

static _PyTok_ReadResult
next_interactive(struct _PyTokenizer *tok, _PyTok_Chunk *chunk)
{
    _PyTok_Reader *reader = &tok->reader;
    if (reader->stopped) {
        return _PYTOK_RD_STOPPED;
    }
    char *input = PyOS_Readline(
        reader->fp != NULL ? reader->fp : stdin,
        stdout, reader->prompt);
    if (reader->nextprompt != NULL) {
        reader->prompt = reader->nextprompt;
    }
    if (input == NULL) {
        return PyErr_Occurred() ? _PYTOK_RD_ERROR : _PYTOK_RD_INTERRUPT;
    }
    Py_ssize_t len = strlen(input);
    if (len == 0) {
        PyMem_Free(input);
        return _PYTOK_RD_EOF;
    }
    Py_ssize_t normalized_len;
    char *normalized = _PyTok_NormalizeNewlines(input, len, 0, 0,
                                           &normalized_len, NULL);
    PyMem_Free(input);
    if (normalized == NULL) {
        return _PYTOK_RD_ERROR;
    }
    if (tok->encoding != NULL) {
        char *decoded;
        Py_ssize_t decoded_len;
        if (_PyTok_DecodeBytesOnce(tok, normalized, normalized_len,
                              tok->encoding, &decoded, &decoded_len) < 0) {
            PyMem_Free(normalized);
            return _PYTOK_RD_ERROR;
        }
        PyMem_Free(normalized);
        normalized = decoded;
        normalized_len = decoded_len;
    }
    chunk->data = normalized;
    chunk->len = normalized_len;
    return _PYTOK_RD_LINE;
}

_PyTok_ReadResult
_PyTok_ReaderNext(struct _PyTokenizer *tok, _PyTok_Chunk *chunk)
{
    *chunk = (_PyTok_Chunk){0};
    assert(tok->reader.next_line != NULL);
    return tok->reader.next_line(tok, chunk);
}

int
_PyTok_ReaderUnderflow(struct _PyTokenizer *tok)
{
    int reset_run = tok->start < 0 && !INSIDE_FSTRING(tok);
    if (reset_run) {
        tok->cursor.run_start = tok->cursor.pos;
    }
    _PyTok_Chunk chunk;
    _PyTok_ReadResult result = _PyTok_ReaderNext(tok, &chunk);
    if (result != _PYTOK_RD_LINE) {
        tok->source.complete = result == _PYTOK_RD_EOF;
        if (result == _PYTOK_RD_INTERRUPT) {
            _PyTok_RecordCurrentError(tok, _PYTOK_ERR_INTERRUPT, NULL);
        }
        else if (result == _PYTOK_RD_ERROR && !_PyTok_HasError(tok)) {
            if (PyErr_Occurred()) {
                _PyTok_RecordPending(tok, _PYTOK_ERR_PROPAGATE);
            }
            else {
                _PyTok_RecordCurrentError(
                    tok, _PYTOK_ERR_SYNTAX,
                    "unknown tokenization error");
            }
        }
        if (tok->reader.kind == _PYTOK_READER_INTERACTIVE &&
                result != _PYTOK_RD_STOPPED) {
            PySys_WriteStderr("\n");
        }
        return 0;
    }

    _PyTok_Off source_start = _PyTok_SourceAppend(
        &tok->source, chunk.data, chunk.len);
    if (source_start < 0) {
        _PyTok_ChunkClear(&chunk);
        _PyTok_RecordNoMemory(tok);
        return 0;
    }
    tok->implicit_newline = chunk.implicit_newline;
    if (_PyTok_SourceAddLine(&tok->source, source_start,
                             source_start + chunk.len,
                             chunk.implicit_newline) < 0) {
        _PyTok_ChunkClear(&chunk);
        _PyTok_RecordNoMemory(tok);
        return 0;
    }
    _PyTok_CursorSetLine(tok, source_start,
                         source_start + chunk.len,
                         reset_run);
    if (tok->reader.kind == _PYTOK_READER_FILE &&
            (tok->encoding == NULL || strcmp(tok->encoding, "utf-8") == 0) &&
            !_PyTok_EnsureUTF8(
                tok->source.bytes + source_start, tok,
                tok->lineno + 1)) {
        _PyTok_ChunkClear(&chunk);
        return 0;
    }
    _PyTok_ChunkClear(&chunk);
    tok->lineno++;
    return 1;
}

int
_PyTok_ReaderBufferRemaining(struct _PyTokenizer *tok)
{
    _PyTok_Reader *reader = &tok->reader;
    if (reader->prepared == NULL ||
            reader->prepared_pos >= reader->prepared_len) {
        return 0;
    }
    Py_ssize_t remaining = reader->prepared_len - reader->prepared_pos;
    if (_PyTok_AppendPreparedSource(
            tok, reader->prepared + reader->prepared_pos, remaining,
            reader->prepared_implicit_newline) < 0) {
        return -1;
    }
    reader->prepared_pos = reader->prepared_len;
    _PyTok_CursorRefresh(tok);
    return 0;
}

#if defined(__wasi__) || (defined(__EMSCRIPTEN__) && (__EMSCRIPTEN_major__ >= 3))
typedef union {
    void *cookie;
    int fd;
} borrowed_fd;

static ssize_t
borrow_read(void *cookie, char *buffer, size_t size)
{
    borrowed_fd borrowed = {.cookie = cookie};
    return read(borrowed.fd, buffer, size);
}

static FILE *
fdopen_borrow(int fd)
{
    cookie_io_functions_t callbacks = {borrow_read, NULL, NULL, NULL};
    borrowed_fd borrowed = {.fd = fd};
    return fopencookie(borrowed.cookie, "r", callbacks);
}
#else
static FILE *
fdopen_borrow(int fd)
{
    int copy = _Py_dup(fd);
    return copy < 0 ? NULL : fdopen(copy, "r");
}
#endif

static int
ignore_warning(struct _PyTokenizer *tok, _PyTok_WarnKind kind,
               const char *text, int value, _PyTok_Loc loc)
{
    return 0;
}

char *
_PyTok_FindEncodingFilename(int fd, PyObject *filename)
{
    FILE *fp = fdopen_borrow(fd);
    if (fp == NULL) {
        return NULL;
    }
    PyObject *default_filename = NULL;
    if (filename == NULL) {
        default_filename = PyUnicode_FromString("<string>");
        if (default_filename == NULL) {
            fclose(fp);
            return NULL;
        }
        filename = default_filename;
    }
    _PyTok_Config config = {
        .kind = _PYTOK_SOURCE_FILE,
        .source.file = {fp, NULL, NULL, NULL},
        .filename = filename,
    };
    PyTokenizer *tok = _PyTok_New(&config);
    Py_XDECREF(default_filename);
    if (tok == NULL) {
        fclose(fp);
        return NULL;
    }
    _PyTok_Seam seam = *_PyTok_DefaultSeam();
    seam.warn = ignore_warning;
    _PyTok_SetSeam(tok, &seam);
    while (_PyTok_Lineno(tok) < 2 && _PyTok_GetError(tok) == NULL &&
            !_PyTok_InputExhausted(tok)) {
        _PyTok_Token token;
        _PyTok_TokenInit(&token);
        _PyTok_Get(tok, &token);
        _PyTok_TokenClear(&token);
    }
    fclose(fp);
    const char *detected = _PyTok_Encoding(tok);
    char *encoding = detected == NULL
        ? NULL
        : _PyTok_CopyBytes(detected, strlen(detected));
    _PyTok_Free(tok);
    return encoding;
}
