#include "Python.h"
#include "codecs.h"
#include "pycore_codecs.h"

#include "errors.h"
#include "helpers.h"
#include "reader_internal.h"
#include "../lexer/state.h"

char *
_PyTok_NormalizeNewlines(const char *data, Py_ssize_t len, int preserve_crlf,
                         int add_final_newline, Py_ssize_t *out_len,
                         int *implicit_newline)
{
    if (len > PY_SSIZE_T_MAX - 2) {
        PyErr_NoMemory();
        return NULL;
    }
    char *result = PyMem_Malloc((size_t)len + 2);
    if (result == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    Py_ssize_t write = 0;
    for (Py_ssize_t read = 0; read < len; read++) {
        char c = data[read];
        if (!preserve_crlf && c == '\r') {
            if (read + 1 < len && data[read + 1] == '\n') {
                read++;
            }
            c = '\n';
        }
        result[write++] = c;
    }
    int implicit = add_final_newline && write > 0 && result[write - 1] != '\n';
    if (implicit) {
        result[write++] = '\n';
    }
    result[write] = '\0';
    *out_len = write;
    if (implicit_newline != NULL) {
        *implicit_newline = implicit;
    }
    return result;
}

static const char *
normal_encoding_name(const char *encoding)
{
    char normalized[13];
    int i;
    for (i = 0; i < 12 && encoding[i] != '\0'; i++) {
        normalized[i] = encoding[i] == '_'
            ? '-'
            : Py_TOLOWER(encoding[i]);
    }
    normalized[i] = '\0';
    if (strcmp(normalized, "utf-8") == 0 ||
            strncmp(normalized, "utf-8-", 6) == 0) {
        return "utf-8";
    }
    if (strcmp(normalized, "latin-1") == 0 ||
            strcmp(normalized, "iso-8859-1") == 0 ||
            strcmp(normalized, "iso-latin-1") == 0 ||
            strncmp(normalized, "latin-1-", 8) == 0 ||
            strncmp(normalized, "iso-8859-1-", 11) == 0 ||
            strncmp(normalized, "iso-latin-1-", 12) == 0) {
        return "iso-8859-1";
    }
    return encoding;
}

static int
find_cookie_in_line(const char *line, Py_ssize_t len, char **encoding,
                    int *scan_next)
{
    Py_ssize_t i = 0;
    *encoding = NULL;
    *scan_next = 1;
    for (; i < len; i++) {
        if (line[i] == '#') {
            break;
        }
        if (line[i] == '\n' || line[i] == '\r') {
            return 0;
        }
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\f') {
            *scan_next = 0;
            return 0;
        }
    }
    for (; i + 6 < len; i++) {
        if (memcmp(line + i, "coding", 6) != 0) {
            continue;
        }
        const char *cursor = line + i + 6;
        if (*cursor != ':' && *cursor != '=') {
            continue;
        }
        do {
            cursor++;
        } while (*cursor == ' ' || *cursor == '\t');
        const char *start = cursor;
        const char *limit = line + len;
        while (cursor < limit &&
                (Py_ISALNUM(*cursor) || *cursor == '-' ||
                 *cursor == '_' || *cursor == '.')) {
            cursor++;
        }
        if (cursor == start) {
            continue;
        }
        char *found = _PyTok_CopyBytes(start, cursor - start);
        if (found == NULL) {
            return -1;
        }
        const char *normal = normal_encoding_name(found);
        if (normal != found) {
            PyMem_Free(found);
            found = _PyTok_CopyBytes(normal, strlen(normal));
            if (found == NULL) {
                return -1;
            }
        }
        *encoding = found;
        *scan_next = 0;
        return 0;
    }
    return 0;
}

int
_PyTok_SetEncoding(struct _PyTokenizer *tok, const char *encoding)
{
    char *copy = _PyTok_CopyString(encoding, strlen(encoding), tok);
    if (copy == NULL) {
        return -1;
    }
    PyMem_Free(tok->encoding);
    tok->encoding = copy;
    return 0;
}

int
_PyTok_DetectEncoding(struct _PyTokenizer *tok, _PyTok_Chunk *first,
                      _PyTok_Chunk *second, int have_second)
{
    int bom = first->len >= 3 &&
        (unsigned char)first->data[0] == 0xEF &&
        (unsigned char)first->data[1] == 0xBB &&
        (unsigned char)first->data[2] == 0xBF;
    if (bom) {
        memmove(first->data, first->data + 3, (size_t)first->len - 2);
        first->len -= 3;
        if (_PyTok_SetEncoding(tok, "utf-8") < 0) {
            return -1;
        }
    }

    char *cookie = NULL;
    int scan_next = 0;
    int cookie_line = 1;
    if (find_cookie_in_line(first->data, first->len,
                            &cookie, &scan_next) < 0) {
        return -1;
    }
    if (cookie == NULL && scan_next && have_second &&
            find_cookie_in_line(second->data, second->len,
                                &cookie, &scan_next) < 0) {
        return -1;
    }
    if (cookie != NULL && scan_next == 0 && have_second) {
        char *first_cookie = NULL;
        int ignored;
        if (find_cookie_in_line(first->data, first->len,
                                &first_cookie, &ignored) < 0) {
            PyMem_Free(cookie);
            return -1;
        }
        cookie_line = first_cookie == NULL ? 2 : 1;
        PyMem_Free(first_cookie);
    }
    if (cookie == NULL) {
        return 0;
    }
    if (bom && strcmp(cookie, "utf-8") != 0) {
        _PyTok_Chunk *line = cookie_line == 2 ? second : first;
        for (int i = 1; i <= cookie_line; i++) {
            _PyTok_Chunk *retained = i == 1 ? first : second;
            _PyTok_Off start = _PyTok_SourceAppend(
                &tok->source, retained->data, retained->len);
            if (start < 0 || _PyTok_SourceAddLine(
                    &tok->source, start, start + retained->len, 0) < 0) {
                PyMem_Free(cookie);
                return -1;
            }
        }
        _PyTok_Line retained_line;
        int result = _PyTok_SourceLine(
            &tok->source, cookie_line, &retained_line);
        assert(result == 0);
        (void)result;
        _PyTok_CursorSetLine(tok, retained_line.start,
                             retained_line.end, 1);
        tok->lineno = cookie_line;
        int end_col = (int)Py_MIN(line->len, INT_MAX);
        if (end_col > 0 && (line->data[end_col - 1] == '\n' ||
                            line->data[end_col - 1] == '\r')) {
            end_col--;
        }
        _PyTok_Loc start = {cookie_line, 0};
        _PyTok_Loc end = {cookie_line, end_col};
        _PyTok_RecordError(
            tok, _PYTOK_ERR_DECODE, start, end, cookie_line, NULL);
        tok->error.detail = _PYTOK_DETAIL_BOM_ENCODING;
        tok->error_detail = cookie;
        return -1;
    }
    if (!bom && _PyTok_SetEncoding(tok, cookie) < 0) {
        PyMem_Free(cookie);
        return -1;
    }
    PyMem_Free(cookie);
    return 0;
}

int
_PyTok_DecodeBytesOnce(struct _PyTokenizer *tok, const char *data,
                       Py_ssize_t len, const char *encoding, char **decoded,
                       Py_ssize_t *decoded_len)
{
    PyObject *unicode = PyUnicode_Decode(data, len, encoding, NULL);
    if (unicode == NULL) {
        _PyTok_RecordPending(tok, _PYTOK_ERR_DECODE);
        return -1;
    }
    PyObject *utf8 = PyUnicode_AsUTF8String(unicode);
    Py_DECREF(unicode);
    if (utf8 == NULL) {
        if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
            _PyTok_RecordNoMemory(tok);
        }
        else {
            _PyTok_RecordPending(tok, _PYTOK_ERR_DECODE);
        }
        return -1;
    }
    *decoded_len = PyBytes_GET_SIZE(utf8);
    *decoded = _PyTok_CopyBytes(PyBytes_AS_STRING(utf8), *decoded_len);
    Py_DECREF(utf8);
    if (*decoded == NULL) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }
    return 0;
}

static Py_ssize_t
raw_line_length(const char *data, Py_ssize_t len)
{
    for (Py_ssize_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            return i + 1;
        }
        if (data[i] == '\r') {
            return i + 1 < len && data[i + 1] == '\n' ? i + 2 : i + 1;
        }
    }
    return len;
}

static int
validate_prepared_utf8(struct _PyTokenizer *tok, const char *data,
                       Py_ssize_t len)
{
    if (_PyTok_AppendPreparedSource(
            tok, data, len, tok->reader.prepared_implicit_newline) < 0) {
        return -1;
    }
    if (len > 0) {
        _PyTok_Line first;
        int result = _PyTok_SourceLine(&tok->source, 1, &first);
        assert(result == 0);
        (void)result;
        _PyTok_CursorSetLine(tok, first.start, first.end, 1);
        tok->lineno = 1;
        if (!_PyTok_EnsureUTF8(tok->source.bytes, tok, 1)) {
            return -1;
        }
    }
#ifdef Py_DEBUG
    _PyTok_SourceClear(&tok->source);
#endif
    _PyTok_CursorInit(&tok->cursor);
    tok->lineno = 0;
    return 0;
}

int
_PyTok_PrepareString(struct _PyTokenizer *tok, const char *input, int utf8_only)
{
    _PyTok_Reader *reader = &tok->reader;
    Py_ssize_t raw_len = strlen(input);
    char *raw = _PyTok_CopyBytes(input, raw_len);
    if (raw == NULL) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }

    if (utf8_only) {
        if (_PyTok_SetEncoding(tok, "utf-8") < 0) {
            PyMem_Free(raw);
            return -1;
        }
    }
    else {
        Py_ssize_t first_len = raw_line_length(raw, raw_len);
        _PyTok_Chunk first = {raw, first_len, 0};
        _PyTok_Chunk second = {0};
        int have_second = first_len < raw_len;
        if (have_second) {
            char *second_start = raw + first_len;
            second.data = second_start;
            second.len = raw_line_length(
                second_start, raw_len - first_len);
        }
        if (_PyTok_DetectEncoding(tok, &first, &second, have_second) < 0) {
            PyMem_Free(raw);
            return -1;
        }
        if (first.len != first_len) {
            Py_ssize_t removed = first_len - first.len;
            memmove(raw + first.len, raw + first_len,
                    raw_len - first_len + 1);
            raw_len -= removed;
        }
    }

    char *decoded = raw;
    Py_ssize_t decoded_len = raw_len;
    if (tok->encoding != NULL && strcmp(tok->encoding, "utf-8") != 0) {
        if (_PyTok_DecodeBytesOnce(tok, raw, raw_len,
                                  tok->encoding, &decoded,
                                  &decoded_len) < 0) {
            PyMem_Free(raw);
            return -1;
        }
        PyMem_Free(raw);
    }

    char *final = _PyTok_NormalizeNewlines(
        decoded, decoded_len, reader->preserve_crlf,
        reader->exec_input, &reader->prepared_len,
        &reader->prepared_implicit_newline);
    PyMem_Free(decoded);
    if (final == NULL) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }
    reader->prepared = final;
    if (!utf8_only &&
            (tok->encoding == NULL || strcmp(tok->encoding, "utf-8") == 0) &&
            validate_prepared_utf8(tok, final, reader->prepared_len) < 0) {
        return -1;
    }
#ifndef Py_DEBUG
    if (tok->source.len == 0 && reader->prepared_len > 0 &&
            _PyTok_AppendPreparedSource(
                tok, final, reader->prepared_len,
                reader->prepared_implicit_newline) < 0) {
        return -1;
    }
    PyMem_Free(reader->prepared);
    reader->prepared = NULL;
    reader->prepared_len = 0;
#endif
    return 0;
}

int
_PyTok_StartIncrementalDecoder(struct _PyTokenizer *tok, const char *errors)
{
    _PyTok_Reader *reader = &tok->reader;
    if (tok->encoding == NULL || reader->decoder != NULL ||
            reader->utf8_decoder) {
        return 0;
    }
    if (strcmp(tok->encoding, "utf-8") == 0) {
        if (reader->kind == _PYTOK_READER_READLINE) {
            reader->utf8_decoder = 1;
        }
        return 0;
    }
    if (reader->kind == _PYTOK_READER_FILE) {
        PyObject *codec = _PyCodec_LookupTextEncoding(tok->encoding, NULL);
        if (codec != NULL) {
            PyObject *factory = PyObject_GetAttrString(
                codec, "incrementaldecoder");
            Py_DECREF(codec);
            if (factory != NULL) {
                reader->decoder = PyObject_CallFunction(
                    factory, "s", errors);
                Py_DECREF(factory);
            }
        }
    }
    else {
        reader->decoder = PyCodec_IncrementalDecoder(
            tok->encoding, errors);
    }
    if (reader->decoder == NULL) {
        if (reader->kind == _PYTOK_READER_FILE) {
            _PyTok_RaiseInitException(
                tok->filename != NULL ? tok->filename : Py_None);
            _PyTok_RecordPending(tok, _PYTOK_ERR_DECODE);
        }
        return -1;
    }
    return 0;
}

static int
decode_utf8_incremental(struct _PyTokenizer *tok, _PyTok_Chunk *chunk,
                        int final)
{
    _PyTok_Reader *reader = &tok->reader;
    if (reader->utf8_pending_len == 0) {
        Py_ssize_t i = 0;
        while (i < chunk->len && (unsigned char)chunk->data[i] < 0x80) {
            i++;
        }
        if (i == chunk->len) {
            return 0;
        }
    }
    Py_ssize_t input_len = reader->utf8_pending_len + chunk->len;
    const char *input = chunk->data;
    char *combined = NULL;
    if (reader->utf8_pending_len != 0) {
        combined = PyMem_Malloc((size_t)input_len);
        if (combined == NULL) {
            _PyTok_RecordNoMemory(tok);
            return -1;
        }
        memcpy(combined, reader->utf8_pending,
               (size_t)reader->utf8_pending_len);
        memcpy(combined + reader->utf8_pending_len,
               chunk->data, (size_t)chunk->len);
        input = combined;
    }
    Py_ssize_t consumed = input_len;
    PyObject *unicode = PyUnicode_DecodeUTF8Stateful(
        input, input_len, "replace", final ? NULL : &consumed);
    if (unicode == NULL) {
        PyMem_Free(combined);
        if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
            _PyTok_RecordNoMemory(tok);
        }
        return -1;
    }
    reader->utf8_pending_len = Py_SAFE_DOWNCAST(
        input_len - consumed, Py_ssize_t, int);
    assert(reader->utf8_pending_len <= 3);
    if (reader->utf8_pending_len != 0) {
        memcpy(reader->utf8_pending, input + consumed,
               (size_t)reader->utf8_pending_len);
    }
    PyMem_Free(combined);

    PyObject *utf8 = PyUnicode_AsUTF8String(unicode);
    Py_DECREF(unicode);
    if (utf8 == NULL) {
        if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
            _PyTok_RecordNoMemory(tok);
        }
        return -1;
    }
    _PyTok_ChunkReleaseData(chunk);
    chunk->owner = utf8;
    chunk->data = PyBytes_AS_STRING(utf8);
    chunk->len = PyBytes_GET_SIZE(utf8);
    return 0;
}

int
_PyTok_DecodeIncremental(struct _PyTokenizer *tok, _PyTok_Chunk *chunk,
                         int final)
{
    _PyTok_Reader *reader = &tok->reader;
    if (reader->utf8_decoder) {
        return decode_utf8_incremental(tok, chunk, final);
    }
    if (reader->decoder == NULL) {
        return 0;
    }
    PyObject *input = chunk->owner != NULL
        ? Py_NewRef(chunk->owner)
        : PyBytes_FromStringAndSize(chunk->data, chunk->len);
    if (input == NULL) {
        _PyTok_RecordNoMemory(tok);
        return -1;
    }
    PyObject *unicode = PyObject_CallMethod(
        reader->decoder, "decode", "Oi", input, final);
    Py_DECREF(input);
    if (unicode == NULL) {
        if (reader->kind == _PYTOK_READER_FILE) {
            _PyTok_RaiseInitException(
                tok->filename != NULL ? tok->filename : Py_None);
            _PyTok_RecordPending(tok, _PYTOK_ERR_DECODE);
        }
        return -1;
    }
    PyObject *utf8 = PyUnicode_AsUTF8String(unicode);
    Py_DECREF(unicode);
    if (utf8 == NULL) {
        if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
            _PyTok_RecordNoMemory(tok);
        }
        else if (reader->kind == _PYTOK_READER_FILE) {
            _PyTok_RecordPending(tok, _PYTOK_ERR_DECODE);
        }
        return -1;
    }
    _PyTok_ChunkReleaseData(chunk);
    chunk->owner = utf8;
    chunk->data = PyBytes_AS_STRING(utf8);
    chunk->len = PyBytes_GET_SIZE(utf8);
    return 0;
}
