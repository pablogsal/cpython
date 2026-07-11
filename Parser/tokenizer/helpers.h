#ifndef Py_TOKENIZER_HELPERS_H
#define Py_TOKENIZER_HELPERS_H

#include "Python.h"

#include "../lexer/state.h"

int _PyTok_SyntaxError(struct _PyTokenizer *tok, const char *format, ...);
int _PyTok_FormattedError(struct _PyTokenizer *tok, _PyTok_ErrKind kind,
                       const char *format, ...);
int _PyTok_FormattedErrorAt(struct _PyTokenizer *tok,
                                      _PyTok_ErrKind kind,
                                      _PyTok_Loc loc, _PyTok_Loc end_loc,
                                      int display_first_lineno,
                                      const char *format, ...);
int _PyTok_SyntaxErrorRange(struct _PyTokenizer *tok, int col_offset, int end_col_offset, const char *format, ...);
int _PyTok_IndentationError(struct _PyTokenizer *tok);
int _PyTok_WarnInvalidEscape(struct _PyTokenizer *, int, _PyTok_Loc);
int _PyTok_Warn(struct _PyTokenizer *, PyObject *, _PyTok_Loc,
                const char *, ...);
void _PyTok_RaiseInitException(PyObject *filename);

char *_PyTok_CopyString(const char *s, Py_ssize_t len, struct _PyTokenizer *tok);
int _PyTok_EnsureUTF8(const char *line, struct _PyTokenizer *tok, int lineno);

#ifdef Py_DEBUG
void _PyTok_PrintEscape(FILE *f, const char *s, Py_ssize_t size);
#endif


#endif
