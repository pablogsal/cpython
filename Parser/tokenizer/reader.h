#ifndef Py_TOKENIZER_READER_H
#define Py_TOKENIZER_READER_H

#include "Python.h"

struct _PyTokenizer;

typedef enum {
    _PYTOK_RD_LINE,
    _PYTOK_RD_EOF,
    _PYTOK_RD_STOPPED,
    _PYTOK_RD_INTERRUPT,
    _PYTOK_RD_ERROR,
} _PyTok_ReadResult;

typedef enum {
    _PYTOK_READER_STRING,
    _PYTOK_READER_UTF8,
    _PYTOK_READER_FILE,
    _PYTOK_READER_READLINE,
    _PYTOK_READER_INTERACTIVE,
} _PyTok_ReaderKind;

typedef struct {
    char *data;
    Py_ssize_t len;
    int implicit_newline;
    PyObject *owner;
} _PyTok_Chunk;

typedef _PyTok_ReadResult (*_PyTok_NextLine)(struct _PyTokenizer *,
                                             _PyTok_Chunk *);

typedef struct {
    _PyTok_ReaderKind kind;
    _PyTok_NextLine next_line;
    FILE *fp;
    PyObject *readline;
    PyObject *decoder;
    const char *prompt;
    const char *nextprompt;
    char *prepared;
    Py_ssize_t prepared_len;
    Py_ssize_t prepared_pos;
    int prepared_implicit_newline;
    char *raw;
    Py_ssize_t raw_cap;
    _PyTok_Chunk pending[2];
    int pending_index;
    int pending_count;
    int initialized;
    int eof;
    int decoder_finalized;
    char utf8_pending[4];
    int utf8_pending_len;
    int utf8_decoder;
    int exec_input;
    int preserve_crlf;
    int stopped;
} _PyTok_Reader;

void _PyTok_ReaderInit(_PyTok_Reader *);
void _PyTok_ReaderClear(_PyTok_Reader *);
_PyTok_ReadResult _PyTok_ReaderNext(struct _PyTokenizer *, _PyTok_Chunk *);
int _PyTok_ReaderUnderflow(struct _PyTokenizer *);
int _PyTok_ReaderBufferRemaining(struct _PyTokenizer *);

#endif
