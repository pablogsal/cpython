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

static int dag_pattern(match_dag *, pattern_ty, int, int, int);

static int
dag_pattern_impl(match_dag *d, pattern_ty p, int value, int yes, int no)
{
    location loc = LOC(p);
    switch (p->kind) {
        case MatchStar_kind:
            return dag_bind(d, p->v.MatchStar.name, value, yes, loc);
        case MatchAs_kind:
            yes = dag_bind(d, p->v.MatchAs.name, value, yes, loc);
            RETURN_IF_ERROR(yes);
            return p->v.MatchAs.pattern ?
                dag_pattern(d, p->v.MatchAs.pattern, value, yes, no) : yes;
        case MatchOr_kind:
            for (int i = asdl_seq_LEN(p->v.MatchOr.patterns); i--;) {
                no = dag_pattern(d, asdl_seq_GET(p->v.MatchOr.patterns, i),
                                 value, yes, no);
                RETURN_IF_ERROR(no);
            }
            return no;
        case MatchSingleton_kind:
            return dag_emit(d, DAG_SINGLETON, value, -1, 0,
                            p->v.MatchSingleton.value, NULL, 0, 0, yes, no, loc);
        case MatchValue_kind: {
            int load = dag_operation_new(d, DAG_LOAD, -1, -1, 0, NULL,
                                         p->v.MatchValue.value, 1, 0);
            RETURN_IF_ERROR(load);
            yes = dag_emit(d, DAG_EQUAL, value, DAG_OP(d, load).output,
                           0, NULL, NULL, 0, 0, yes, no, loc);
            return dag_node_new(d, load, yes, -1, loc);
        }
        case MatchSequence_kind: {
            asdl_pattern_seq *items = p->v.MatchSequence.patterns;
            int n = asdl_seq_LEN(items), star = -1, wild = 1;
            for (int i = 0; i < n; i++) {
                pattern_ty child = asdl_seq_GET(items, i);
                if (child->kind == MatchStar_kind) {
                    star = i;
                    wild &= WILDCARD_STAR_CHECK(child);
                }
                else {
                    wild &= WILDCARD_CHECK(child);
                }
            }
            int length = dag_operation_new(d, DAG_LENGTH, value, -1, 0,
                                           NULL, NULL, 1, 0);
            RETURN_IF_ERROR(length);
            int length_value = DAG_OP(d, length).output;
            if (!wild && star >= 0 &&
                WILDCARD_STAR_CHECK((pattern_ty)asdl_seq_GET(items, star))) {
                for (int i = n; i--;) {
                    pattern_ty child = asdl_seq_GET(items, i);
                    if (i == star || WILDCARD_CHECK(child)) {
                        continue;
                    }
                    int index = i < star ? i : i - n;
                    int op = dag_operation_new(d, DAG_INDEX, value,
                                               index < 0 ? length_value : -1,
                                               index, NULL, NULL, 1, 0);
                    RETURN_IF_ERROR(op);
                    yes = dag_pattern(d, child, DAG_OP(d, op).output, yes, no);
                    yes = dag_node_new(d, op, yes, -1, loc);
                    RETURN_IF_ERROR(yes);
                }
            }
            else if (!wild) {
                int arg = star < 0 ? n : star + ((n - star - 1) << 8);
                int op = dag_operation_new(d, DAG_UNPACK, value, -1, arg,
                                           star < 0 ? Py_False : Py_True,
                                           NULL, n, star >= 0);
                RETURN_IF_ERROR(op);
                int output = DAG_OP(d, op).output;
                for (int i = n; i--;) {
                    yes = dag_pattern(d, asdl_seq_GET(items, i), output + i, yes, no);
                    RETURN_IF_ERROR(yes);
                }
                yes = dag_node_new(d, op, yes, -1, loc);
            }
            if (star < 0 || n > 1) {
                yes = dag_emit(d, star < 0 ? DAG_LENGTH_EQ : DAG_LENGTH_GE,
                               length_value, -1, n - (star >= 0), NULL, NULL,
                               0, 0, yes, no, loc);
                yes = dag_node_new(d, length, yes, -1, loc);
            }
            return dag_emit(d, DAG_SEQUENCE, value, -1, 0, NULL, NULL,
                            0, 0, yes, no, loc);
        }
        case MatchMapping_kind: {
            int n = asdl_seq_LEN(p->v.MatchMapping.keys);
            PyObject *loads = PyTuple_New(n);
            if (loads == NULL) {
                return ERROR;
            }
            for (int i = 0; i < n; i++) {
                int op = dag_operation_new(d, DAG_LOAD, -1, -1, 0, NULL,
                    asdl_seq_GET(p->v.MatchMapping.keys, i), 1, 0);
                if (op < 0) {
                    Py_DECREF(loads);
                    return ERROR;
                }
                PyObject *number = PyLong_FromLong(op);
                if (number == NULL) {
                    Py_DECREF(loads);
                    return ERROR;
                }
                PyTuple_SET_ITEM(loads, i, number);
            }
            PyObject *inputs = PyTuple_New(n);
            if (inputs == NULL) {
                Py_DECREF(loads);
                return ERROR;
            }
            for (int i = 0; i < n; i++) {
                int op = (int)PyLong_AsLong(PyTuple_GET_ITEM(loads, i));
                PyObject *number = PyLong_FromLong(DAG_OP(d, op).output);
                if (number == NULL) {
                    Py_DECREF(inputs);
                    Py_DECREF(loads);
                    return ERROR;
                }
                PyTuple_SET_ITEM(inputs, i, number);
            }
            int tuple = dag_operation_new(d, DAG_TUPLE, -1, -1, n, inputs, NULL, 1, 0);
            Py_DECREF(inputs);
            if (tuple < 0) {
                Py_DECREF(loads);
                return ERROR;
            }
            int keys_value = DAG_OP(d, tuple).output;
            if (p->v.MatchMapping.rest) {
                int rest = dag_operation_new(d, DAG_REST, value, keys_value,
                                             n, NULL, NULL, 1, 1);
                if (rest >= 0) {
                    yes = dag_bind(d, p->v.MatchMapping.rest,
                                   DAG_OP(d, rest).output, yes, loc);
                    yes = dag_node_new(d, rest, yes, -1, loc);
                }
            }
            int keys = dag_operation_new(d, DAG_KEYS, value, keys_value, 0, NULL, NULL, 1, 0);
            if (keys >= 0) {
                int unpack = dag_operation_new(d, DAG_UNPACK, DAG_OP(d, keys).output,
                                               -1, n, Py_False, NULL, n, 0);
                if (unpack >= 0) {
                    int output = DAG_OP(d, unpack).output;
                    for (int i = n; i-- && !PyErr_Occurred();) {
                        yes = dag_pattern(d, asdl_seq_GET(p->v.MatchMapping.patterns, i),
                                          output + i, yes, no);
                    }
                    yes = dag_node_new(d, unpack, yes, -1, loc);
                    yes = dag_node_new(d, keys, yes, no, loc);
                }
            }
            yes = dag_node_new(d, tuple, yes, -1, loc);
            for (int i = n; i-- && !PyErr_Occurred();) {
                int load = (int)PyLong_AsLong(PyTuple_GET_ITEM(loads, i));
                yes = dag_node_new(d, load, yes, -1, loc);
            }
            Py_DECREF(loads);
            if (n) {
                int length = dag_operation_new(d, DAG_LENGTH, value, -1, 0, NULL, NULL, 1, 0);
                RETURN_IF_ERROR(length);
                yes = dag_emit(d, DAG_LENGTH_GE, DAG_OP(d, length).output, -1,
                               n, NULL, NULL, 0, 0, yes, no, loc);
                yes = dag_node_new(d, length, yes, -1, loc);
            }
            return dag_emit(d, DAG_MAPPING, value, -1, 0, NULL, NULL,
                            0, 0, yes, no, loc);
        }
        case MatchClass_kind: {
            int nargs = asdl_seq_LEN(p->v.MatchClass.patterns);
            int nattrs = asdl_seq_LEN(p->v.MatchClass.kwd_attrs);
            int n = nargs + nattrs;
            PyObject *names = PyTuple_New(nattrs);
            if (names == NULL) {
                return ERROR;
            }
            for (int i = 0; i < nattrs; i++) {
                PyTuple_SET_ITEM(names, i, Py_NewRef(asdl_seq_GET(p->v.MatchClass.kwd_attrs, i)));
            }
            int load = dag_operation_new(d, DAG_LOAD, -1, -1, 0, NULL,
                                         p->v.MatchClass.cls, 1, 0);
            if (load < 0) {
                Py_DECREF(names);
                return ERROR;
            }
            int cls = dag_operation_new(d, DAG_CLASS, value, DAG_OP(d, load).output,
                                        nargs, names, NULL, 1, 0);
            Py_DECREF(names);
            RETURN_IF_ERROR(cls);
            int unpack = dag_operation_new(d, DAG_UNPACK, DAG_OP(d, cls).output,
                                           -1, n, Py_False, NULL, n, 0);
            RETURN_IF_ERROR(unpack);
            int output = DAG_OP(d, unpack).output;
            for (int i = n; i--;) {
                pattern_ty child = i < nargs ? asdl_seq_GET(p->v.MatchClass.patterns, i) :
                    asdl_seq_GET(p->v.MatchClass.kwd_patterns, i - nargs);
                yes = dag_pattern(d, child, output + i, yes, no);
                RETURN_IF_ERROR(yes);
            }
            yes = dag_node_new(d, unpack, yes, -1, loc);
            yes = dag_node_new(d, cls, yes, no, loc);
            return dag_node_new(d, load, yes, -1, loc);
        }
    }
    Py_UNREACHABLE();
}

static int
dag_pattern(match_dag *d, pattern_ty p, int value, int yes, int no)
{
    if (Py_EnterRecursiveCall(" during match DAG construction")) {
        return ERROR;
    }
    int result = dag_pattern_impl(d, p, value, yes, no);
    Py_LeaveRecursiveCall();
    return result;
}

#undef DAG_OP
#undef DAG_NODE
