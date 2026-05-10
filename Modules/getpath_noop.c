/* Implements the getpath API for compiling with no functionality */

#include "Python.h"
#include "pycore_initconfig.h"
#include "pycore_pathconfig.h"

PyStatus
_PyConfig_InitPathConfig(PyConfig *config, int compute_path_config)
{
    return PyStatus_Error("path configuration is unsupported");
}

PyObject *
_Py_Get_Getpath_CodeObject(void)
{
    PyErr_SetString(PyExc_RuntimeError, "getpath code object is unsupported");
    return NULL;
}
