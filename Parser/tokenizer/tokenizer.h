#ifndef Py_TOKENIZER_H
#define Py_TOKENIZER_H

#include "Python.h"
#include "source.h"

typedef struct _PyTokenizer PyTokenizer;

typedef enum {
    _PYTOK_OK,
    _PYTOK_ERROR,
} _PyTok_Status;

typedef enum {
    _PYTOK_ERR_NONE,
    _PYTOK_ERR_SYNTAX,
    _PYTOK_ERR_EOF_IN_CONSTRUCT,
    _PYTOK_ERR_EOF_IN_STRING,
    _PYTOK_ERR_EOL_IN_STRING,
    _PYTOK_ERR_DEDENT_MISMATCH,
    _PYTOK_ERR_TABSPACE,
    _PYTOK_ERR_TOODEEP,
    _PYTOK_ERR_LINECONT,
    _PYTOK_ERR_DECODE,
    _PYTOK_ERR_COLUMN_OVERFLOW,
    _PYTOK_ERR_INTERRUPT,
    _PYTOK_ERR_NOMEM,
    _PYTOK_ERR_PROPAGATE,
} _PyTok_ErrKind;

typedef enum {
    _PYTOK_DETAIL_NONE,
    _PYTOK_DETAIL_BOM_ENCODING,
    _PYTOK_DETAIL_NON_UTF8,
} _PyTok_ErrorDetail;

typedef struct {
    _PyTok_ErrKind kind;
    _PyTok_Loc loc;
    _PyTok_Loc end_loc;
    int display_first_lineno;
    char msg[256];
    _PyTok_ErrorDetail detail;
    unsigned char invalid_byte;
    unsigned pending : 1;
    unsigned columns_are_chars : 1;
} _PyTok_Error;

typedef enum {
    _PYTOK_SOURCE_STRING,
    _PYTOK_SOURCE_UTF8,
    _PYTOK_SOURCE_FILE,
    _PYTOK_SOURCE_READLINE,
} _PyTok_SourceKind;

typedef enum {
    _PYTOK_FSTRING,
    _PYTOK_TSTRING,
} _PyTok_StringKind;

enum {
    _PYTOK_SYNTH = 1 << 0,
    _PYTOK_IMPLICIT_NL = 1 << 1,
};

typedef struct {
    int type;
    _PyTok_Span span;
    _PyTok_Loc start;
    _PyTok_Loc end;
    int level;
    unsigned flags;
    PyObject *metadata;
} _PyTok_Token;

typedef struct {
    _PyTok_SourceKind kind;
    union {
        const char *string;
        struct {
            FILE *fp;
            const char *encoding;
            const char *ps1;
            const char *ps2;
        } file;
        struct {
            PyObject *readline;
            const char *encoding;
        } readline;
    } source;
    int exec_input;
    int preserve_crlf;
    int extra_tokens;
    int type_comments;
    int interactive;
    PyObject *filename;
    PyObject *module;
} _PyTok_Config;

PyAPI_FUNC(PyTokenizer *) _PyTok_New(const _PyTok_Config *);
PyAPI_FUNC(void) _PyTok_Free(PyTokenizer *);
int _PyTok_Traverse(const PyTokenizer *, visitproc, void *);
void _PyTok_RaiseInitError(PyObject *);

PyAPI_FUNC(_PyTok_Status) _PyTok_Get(PyTokenizer *, _PyTok_Token *);
const _PyTok_Error *_PyTok_GetError(const PyTokenizer *);
int _PyTok_RaiseError(PyTokenizer *);
int _PyTok_ErrorIsIncomplete(const PyTokenizer *);
PyAPI_FUNC(void) _PyTok_TokenInit(_PyTok_Token *);
PyAPI_FUNC(void) _PyTok_TokenClear(_PyTok_Token *);
const char *_PyTok_TokenView(PyTokenizer *, const _PyTok_Token *, Py_ssize_t *);
PyObject *_PyTok_TokenBytes(PyTokenizer *, const _PyTok_Token *);
int _PyTok_TokenSpanLocations(PyTokenizer *, const _PyTok_Token *,
                              _PyTok_Loc *, _PyTok_Loc *);

void _PyTok_FlushIndentation(PyTokenizer *);
void _PyTok_StopPrompting(PyTokenizer *);

int _PyTok_Lineno(const PyTokenizer *);
int _PyTok_Column(const PyTokenizer *);
int _PyTok_RunColumn(const PyTokenizer *);
int _PyTok_ParenDepth(const PyTokenizer *);
void _PyTok_ParenEntry(const PyTokenizer *, int, char *, int *, int *);
int _PyTok_FStringDepth(const PyTokenizer *);
int _PyTok_FStringRaw(const PyTokenizer *);
char _PyTok_StringPrefix(const PyTokenizer *);
int _PyTok_IndentDepth(const PyTokenizer *);
const char *_PyTok_Encoding(const PyTokenizer *);
PyObject *_PyTok_Filename(const PyTokenizer *);
PyObject *_PyTok_Module(const PyTokenizer *);
int _PyTok_IsInteractive(const PyTokenizer *);
int _PyTok_IsPrompting(const PyTokenizer *);
_PyTok_SourceKind _PyTok_GetSourceKind(const PyTokenizer *);
int _PyTok_InputExhausted(const PyTokenizer *);
int _PyTok_ExtraTokens(const PyTokenizer *);

const char *_PyTok_SourceView(PyTokenizer *, Py_ssize_t *);
const char *_PyTok_LineView(const PyTokenizer *, int, Py_ssize_t *);
int _PyTok_LineIsImplicit(const PyTokenizer *, int);
const char *_PyTok_LineRangeView(const PyTokenizer *, int, int, Py_ssize_t *);
const char *_PyTok_RemainingView(PyTokenizer *, Py_ssize_t *);
const char *_PyTok_CurrentRunView(const PyTokenizer *, Py_ssize_t *);

char *_PyTok_FindEncodingFilename(int, PyObject *);

#endif
