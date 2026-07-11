#ifndef _PY_LEXER_H_
#define _PY_LEXER_H_

#include "object.h"
#include "../tokenizer/cursor.h"
#include "../tokenizer/reader.h"
#include "../tokenizer/seam.h"
#include "../tokenizer/source.h"
#include "../tokenizer/tokenizer.h"

#define MAXINDENT 100       /* Max indentation level */
#define MAXLEVEL 200        /* Max parentheses level */
#define MAXFSTRINGLEVEL 150 /* Max f-string nesting level */
#define _PYTOK_MAX_FRAMES (MAXFSTRINGLEVEL * 3)

#define INSIDE_FSTRING(tok) (tok->frame_index > 0)
#define INSIDE_FSTRING_EXPR(mode) ((mode)->kind == _PYTOK_FRAME_FSTRING_EXPR)
#define INSIDE_FSTRING_EXPR_AT_TOP(tok, mode) \
    ((mode)->kind == _PYTOK_FRAME_FSTRING_EXPR && \
     (tok)->level == (mode)->paren_depth_at_entry + 1)

typedef enum {
    _PYTOK_FRAME_TOPLEVEL,
    _PYTOK_FRAME_FSTRING_BODY,
    _PYTOK_FRAME_FSTRING_EXPR,
    _PYTOK_FRAME_FORMAT_SPEC,
} _PyTok_FrameKind;

#define MAX_EXPR_NESTING 3

typedef struct {
    _PyTok_FrameKind kind;
    int paren_depth_at_entry;

    char quote;
    int quote_size;
    int raw;
    _PyTok_Off body_start;

    _PyTok_Off debug_expr_start;
    _PyTok_Off debug_expr_end;
    int in_debug;
    int spec_degraded;
    int pending_single_brace;

    _PyTok_StringKind string_kind;
} _PyTok_Frame;

/* Tokenizer state */
struct _PyTokenizer {
    _PyTok_Off start;
    _PyTok_Error error;
    char *error_detail;
    int busy;
    int tabsize;        /* Tab spacing */
    int indent;         /* Current indentation index */
    int indstack[MAXINDENT];            /* Stack of indents */
    int atbol;          /* Nonzero if at begin of new line */
    int pendin;         /* Pending indents (if > 0) or dedents (if < 0) */
    int lineno;         /* Current line number */
    int level;          /* () [] {} Parentheses nesting level */
            /* Used to allow free continuations inside them */
    char parenstack[MAXLEVEL];
    int parenlinenostack[MAXLEVEL];
    int parencolstack[MAXLEVEL];
    PyObject *filename;
    PyObject *module;
    /* Stuff for checking on different tab sizes */
    int altindstack[MAXINDENT];         /* Stack of alternate indents */
    char *encoding;         /* Source encoding. */
    _PyTok_SourceText source;
    _PyTok_Cursor cursor;
    _PyTok_Reader reader;
    const _PyTok_Seam *seam;

    int type_comments;      /* Whether to look for type comments */

    _PyTok_Frame frames[_PYTOK_MAX_FRAMES];
    int frame_index;
    int extra_tokens;
    int comment_newline;
    int implicit_newline;
    _PyTok_SourceKind source_kind;
    int is_interactive;
    int is_prompting;
#ifdef Py_DEBUG
    int debug;
#endif
};

struct _PyTokenizer *_PyTok_StateNew(void);
struct _PyTokenizer *_PyTok_StateFromString(const char *, int, int);
struct _PyTokenizer *_PyTok_StateFromUTF8(const char *, int, int);
struct _PyTokenizer *_PyTok_StateFromReadline(PyObject *, const char *, int, int);
struct _PyTokenizer *_PyTok_StateFromFile(FILE *, const char *, const char *,
                                       const char *);
void _PyTok_StateFree(struct _PyTokenizer *);
#endif
