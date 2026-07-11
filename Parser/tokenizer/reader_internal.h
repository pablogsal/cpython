#ifndef Py_TOKENIZER_READER_INTERNAL_H
#define Py_TOKENIZER_READER_INTERNAL_H

#include "reader.h"

void _PyTok_ChunkReleaseData(_PyTok_Chunk *);
void _PyTok_ChunkClear(_PyTok_Chunk *);
char *_PyTok_CopyBytes(const char *, Py_ssize_t);
char *_PyTok_NormalizeNewlines(const char *, Py_ssize_t, int, int,
                               Py_ssize_t *, int *);
int _PyTok_SetEncoding(struct _PyTokenizer *, const char *);
int _PyTok_DetectEncoding(struct _PyTokenizer *, _PyTok_Chunk *,
                          _PyTok_Chunk *, int);
int _PyTok_DecodeBytesOnce(struct _PyTokenizer *, const char *, Py_ssize_t,
                           const char *, char **, Py_ssize_t *);
int _PyTok_AppendPreparedSource(struct _PyTokenizer *, const char *,
                                Py_ssize_t, int);
int _PyTok_PrepareString(struct _PyTokenizer *, const char *, int);
int _PyTok_StartIncrementalDecoder(struct _PyTokenizer *, const char *);
int _PyTok_DecodeIncremental(struct _PyTokenizer *, _PyTok_Chunk *, int);

#endif
