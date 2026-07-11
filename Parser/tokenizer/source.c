#include "Python.h"

#include "source.h"

void
_PyTok_SourceInit(_PyTok_SourceText *source)
{
    *source = (_PyTok_SourceText){0};
}

void
_PyTok_SourceClear(_PyTok_SourceText *source)
{
    PyMem_Free(source->bytes);
    PyMem_Free(source->line_checkpoints);
    PyMem_Free(source->implicit_lines);
    _PyTok_SourceInit(source);
}

static int
reserve_bytes(_PyTok_SourceText *source, Py_ssize_t needed)
{
#ifndef Py_DEBUG
    if (needed <= source->cap) {
        return 0;
    }
#endif
    Py_ssize_t cap = source->cap > 0 ? source->cap : BUFSIZ;
    while (cap < needed) {
        if (cap > PY_SSIZE_T_MAX / 2) {
            cap = needed;
            break;
        }
        cap *= 2;
    }
    char *bytes;
#ifdef Py_DEBUG
    bytes = PyMem_Malloc(cap);
    if (bytes != NULL && source->len > 0) {
        memcpy(bytes, source->bytes, source->len);
    }
#else
    bytes = PyMem_Realloc(source->bytes, cap);
#endif
    if (bytes == NULL) {
        PyErr_NoMemory();
        return -1;
    }
#ifdef Py_DEBUG
    if (source->bytes != NULL) {
        memset(source->bytes, 0xDD, source->cap);
        PyMem_Free(source->bytes);
    }
#endif
    source->bytes = bytes;
    source->cap = cap;
    return 0;
}

_PyTok_Off
_PyTok_SourceAppend(_PyTok_SourceText *source, const char *bytes, Py_ssize_t len)
{
    if (len < 0 || source->len > PY_SSIZE_T_MAX - len - 1) {
        PyErr_NoMemory();
        return -1;
    }
    _PyTok_Off start = source->len;
    if (reserve_bytes(source, source->len + len + 1) < 0) {
        return -1;
    }
    if (len > 0) {
        memcpy(source->bytes + source->len, bytes, len);
    }
    source->len += len;
    source->bytes[source->len] = '\0';
    return start;
}

int
_PyTok_SourceAddLine(_PyTok_SourceText *source, _PyTok_Off start,
                     _PyTok_Off end, int implicit_newline)
{
    if (start < 0 || end < start || end > source->len) {
        PyErr_SetString(PyExc_SystemError, "invalid tokenizer line span");
        return -1;
    }
    if ((source->nlines == 0 && start != 0) ||
            (source->nlines > 0 && start != source->last_line_end)) {
        PyErr_SetString(PyExc_SystemError,
                        "non-contiguous tokenizer line span");
        return -1;
    }
    if ((source->nlines & 255) == 0) {
        if (source->ncheckpoints == source->checkpoints_cap) {
            int cap;
            if (source->checkpoints_cap == 0) {
                cap = 16;
            }
            else if (source->checkpoints_cap <= INT_MAX / 2) {
                cap = source->checkpoints_cap * 2;
            }
            else {
                cap = INT_MAX;
            }
            if (cap <= source->checkpoints_cap ||
                    (size_t)cap > (size_t)PY_SSIZE_T_MAX /
                        sizeof(*source->line_checkpoints)) {
                PyErr_NoMemory();
                return -1;
            }
            _PyTok_Off *checkpoints = PyMem_Realloc(
                source->line_checkpoints,
                (size_t)cap * sizeof(*source->line_checkpoints));
            if (checkpoints == NULL) {
                PyErr_NoMemory();
                return -1;
            }
            source->line_checkpoints = checkpoints;
            source->checkpoints_cap = cap;
        }
        source->line_checkpoints[source->ncheckpoints++] = start;
    }
    source->nlines++;
    source->last_line_end = end;
    if (implicit_newline) {
        Py_ssize_t needed = ((Py_ssize_t)source->nlines + 7) / 8;
        if (needed > source->implicit_bytes) {
            unsigned char *lines = PyMem_Realloc(
                source->implicit_lines, needed);
            if (lines == NULL) {
                PyErr_NoMemory();
                return -1;
            }
            memset(lines + source->implicit_bytes, 0,
                   needed - source->implicit_bytes);
            source->implicit_lines = lines;
            source->implicit_bytes = needed;
        }
        source->implicit_lines[(source->nlines - 1) / 8] |=
            (unsigned char)(1U << ((source->nlines - 1) & 7));
    }
    return 0;
}

const char *
_PyTok_SourceSpanView(const _PyTok_SourceText *source, _PyTok_Span span,
                      Py_ssize_t *len)
{
    if (!_PyTok_SpanIsValid(span) || span.end > source->len) {
        PyErr_SetString(PyExc_SystemError, "invalid tokenizer source span");
        return NULL;
    }
    *len = span.end - span.start;
    return source->bytes + span.start;
}

int
_PyTok_SourceLine(const _PyTok_SourceText *source, int lineno,
                  _PyTok_Line *line)
{
    if (lineno < 1 || lineno > source->nlines) {
        return -1;
    }
    int checkpoint = (lineno - 1) >> 8;
    int current = (checkpoint << 8) + 1;
    _PyTok_Off start = source->line_checkpoints[checkpoint];
    while (current < lineno) {
        const char *newline = memchr(
            source->bytes + start, '\n', source->len - start);
        if (newline == NULL) {
            return -1;
        }
        start = newline - source->bytes + 1;
        current++;
    }
    _PyTok_Off end = source->last_line_end;
    if (lineno < source->nlines) {
        const char *newline = memchr(
            source->bytes + start, '\n', source->len - start);
        if (newline == NULL) {
            return -1;
        }
        end = newline - source->bytes + 1;
    }
    *line = (_PyTok_Line){
        .start = start,
        .end = end,
        .implicit_newline = _PyTok_SourceLineIsImplicit(source, lineno),
        .contains_nul = end > start && memchr(
            source->bytes + start, 0, end - start) != NULL,
    };
    return 0;
}

int
_PyTok_SourceLineIsImplicit(const _PyTok_SourceText *source, int lineno)
{
    if (lineno < 1 || lineno > source->nlines ||
            (lineno - 1) / 8 >= source->implicit_bytes) {
        return 0;
    }
    return (source->implicit_lines[(lineno - 1) / 8] >>
            ((lineno - 1) & 7)) & 1;
}

int
_PyTok_SourceLocation(const _PyTok_SourceText *source, _PyTok_Off offset,
                      _PyTok_Loc *loc)
{
    if (offset < 0 || offset > source->len || source->nlines == 0) {
        return -1;
    }
    int low = 0;
    int high = source->ncheckpoints;
    while (low < high) {
        int middle = low + (high - low) / 2;
        if (source->line_checkpoints[middle] <= offset) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    int checkpoint = low - 1;
    if (checkpoint < 0) {
        return -1;
    }
    int lineno = (checkpoint << 8) + 1;
    _PyTok_Off start = source->line_checkpoints[checkpoint];
    while (lineno < source->nlines) {
        const char *newline = memchr(
            source->bytes + start, '\n', source->len - start);
        if (newline == NULL) {
            return -1;
        }
        _PyTok_Off end = newline - source->bytes + 1;
        if (offset < end) {
            break;
        }
        start = end;
        lineno++;
    }
    if (offset > source->last_line_end) {
        return -1;
    }
    _PyTok_Off column = offset - start;
    if (column > INT_MAX) {
        return -1;
    }
    *loc = (_PyTok_Loc){lineno, (int)column};
    return 0;
}
