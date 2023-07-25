#include "Python.h"

#include "tokenizer.h"
#include "pegen.h"

mod_ty
_PyParser_ASTFromString(const char *str, PyObject* filename, int mode,
                        PyCompilerFlags *flags, PyArena *arena)
{
    if (PySys_Audit("compile", "yO", str, filename) < 0) {
        return NULL;
    }

    mod_ty result = _PyPegen_run_parser_from_string(str, mode, filename, flags, arena);
    return result;
}

mod_ty
_PyParser_ASTFromFile(FILE *fp, PyObject *filename_ob, const char *enc,
                      int mode, PyCompilerFlags *flags, int *errcode, PyArena *arena)
{
    if (PySys_Audit("compile", "OO", Py_None, filename_ob) < 0) {
        return NULL;
    }
    return _PyPegen_run_parser_from_file_pointer(fp, mode, filename_ob, enc,
                                                flags, errcode, arena);
}
