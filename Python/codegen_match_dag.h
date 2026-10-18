/* Experimental ordered match DAG. Included by codegen.c.
 *
 * Operations name their input values, independently of operand-stack positions.
 * Specialization remembers both outcomes of a test. Liveness then assigns the
 * same kind of temporary slot to extracted values and pending captures.
 */

enum dag_kind {
    DAG_ENTER, DAG_BIND, DAG_COMMIT, DAG_GUARD, DAG_ACCEPT, DAG_END,
    DAG_LOAD, DAG_SEQUENCE, DAG_MAPPING, DAG_LENGTH, DAG_LENGTH_EQ,
    DAG_LENGTH_GE, DAG_EQUAL, DAG_SINGLETON, DAG_UNPACK, DAG_INDEX,
    DAG_TUPLE, DAG_KEYS, DAG_CLASS, DAG_REST
};

typedef struct {
    int kind, a, b, arg, output, count, reusable;
    PyObject *data;
    expr_ty expr;
} dag_operation;

typedef struct {
    int op, yes, no;
    location loc;
} dag_node;

typedef struct {
    compiler *c;
    _Py_c_array_t operations, nodes;
    int nops, nnodes, nvalues, original, states, slots, removed;
    PyObject *op_ids, *node_ids, *state_ids, *captures;
    PyObject **stores;
    PyObject **constants;
    unsigned char *facts_live, *values_live, *reachable;
    int *colors;
    jump_target_label *labels, *bodies;
    jump_target_label end;
} match_dag;

#define DAG_OP(d, i) (((dag_operation *)(d)->operations.array)[i])
#define DAG_NODE(d, i) (((dag_node *)(d)->nodes.array)[i])

static int
dag_capture(match_dag *d, PyObject *name)
{
    PyObject *found = PyDict_GetItemWithError(d->captures, name);
    if (found) {
        return (int)PyLong_AsLong(found);
    }
    RETURN_IF_ERROR(PyErr_Occurred() ? -1 : 0);
    int id = d->nvalues++;
    PyObject *number = PyLong_FromLong(id);
    if (number == NULL) {
        return ERROR;
    }
    int err = PyDict_SetItem(d->captures, name, number);
    Py_DECREF(number);
    return err < 0 ? ERROR : id;
}

/* Pattern expressions are names, attribute chains, or folded constants. Use
 * their AST structure, not source positions or generated instruction strings. */
static PyObject *
dag_expression_key(expr_ty expr)
{
    switch (expr->kind) {
        case Name_kind:
            return Py_BuildValue("(iO)", Name_kind, expr->v.Name.id);
        case Attribute_kind: {
            PyObject *base = dag_expression_key(expr->v.Attribute.value);
            if (base == NULL) {
                return NULL;
            }
            return Py_BuildValue("(iNO)", Attribute_kind, base, expr->v.Attribute.attr);
        }
        case Constant_kind: {
            PyObject *key = _PyCode_ConstantKey(expr->v.Constant.value);
            return key ? Py_BuildValue("(iN)", Constant_kind, key) : NULL;
        }
        default:
            /* Retain ordinary expression code generation for validated ASTs
             * whose expression has not been folded, without claiming reuse. */
            return PyLong_FromVoidPtr(expr);
    }
}

static int
dag_operation_new(match_dag *d, int kind, int a, int b, int arg,
                  PyObject *data, expr_ty expr, int count, int fresh)
{
    if (a < -1 || b < -1 || PyErr_Occurred()) {
        return ERROR;
    }
    PyObject *expr_key = expr ? dag_expression_key(expr) : Py_NewRef(Py_None);
    if (expr_key == NULL) {
        return ERROR;
    }
    PyObject *key = Py_BuildValue("(iiiiiOON)", kind, a, b, arg,
                                 fresh ? d->nops + 1 : 0,
                                 data ? data : Py_None, Py_None, expr_key);
    if (key == NULL) {
        return ERROR;
    }
    PyObject *found = PyDict_GetItemWithError(d->op_ids, key);
    if (found) {
        int id = (int)PyLong_AsLong(found);
        Py_DECREF(key);
        return id;
    }
    int id = d->nops;
    PyObject *number = PyLong_FromLong(id);
    if (number == NULL || PyErr_Occurred() ||
        _Py_CArray_EnsureCapacity(&d->operations, id) < 0 ||
        PyDict_SetItem(d->op_ids, key, number) < 0) {
        Py_DECREF(key);
        Py_XDECREF(number);
        return ERROR;
    }
    Py_DECREF(number);
    Py_DECREF(key);
    DAG_OP(d, id) = (dag_operation){
        .kind = kind, .a = a, .b = b, .arg = arg,
        .output = d->nvalues, .count = count,
        .reusable = kind >= DAG_LOAD && !fresh,
        .data = Py_XNewRef(data), .expr = expr
    };
    if (kind == DAG_BIND) {
        DAG_OP(d, id).output = arg;
    }
    else {
        d->nvalues += count;
    }
    d->nops++;
    return id;
}

static int
dag_node_new(match_dag *d, int op, int yes, int no, location loc)
{
    if (op < 0 || PyErr_Occurred()) {
        return ERROR;
    }
    PyObject *key = Py_BuildValue("(iiiiiii)", op, yes, no, loc.lineno,
                                 loc.end_lineno, loc.col_offset, loc.end_col_offset);
    if (key == NULL) {
        return ERROR;
    }
    PyObject *found = PyDict_GetItemWithError(d->node_ids, key);
    if (found) {
        int id = (int)PyLong_AsLong(found);
        Py_DECREF(key);
        return id;
    }
    int id = d->nnodes;
    PyObject *number = PyLong_FromLong(id);
    if (number == NULL || PyErr_Occurred() ||
        _Py_CArray_EnsureCapacity(&d->nodes, id) < 0 ||
        PyDict_SetItem(d->node_ids, key, number) < 0) {
        Py_DECREF(key);
        Py_XDECREF(number);
        return ERROR;
    }
    Py_DECREF(number);
    Py_DECREF(key);
    DAG_NODE(d, id) = (dag_node){op, yes, no, loc};
    d->nnodes++;
    return id;
}

static int
dag_emit(match_dag *d, int kind, int a, int b, int arg, PyObject *data,
         expr_ty expr, int count, int fresh, int yes, int no, location loc)
{
    int op = dag_operation_new(d, kind, a, b, arg, data, expr, count, fresh);
    return dag_node_new(d, op, yes, no, loc);
}

static int
dag_bind(match_dag *d, identifier name, int value, int yes, location loc)
{
    if (name == NULL) {
        return yes;
    }
    int capture = dag_capture(d, name);
    RETURN_IF_ERROR(capture);
    return dag_emit(d, DAG_BIND, value, -1, capture, NULL, NULL, 1, 0,
                    yes, -1, loc);
}

#undef DAG_OP
#undef DAG_NODE
