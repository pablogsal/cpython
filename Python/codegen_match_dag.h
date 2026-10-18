/* Experimental ordered match DAG. Included by codegen.c.
 *
 * Operations name their input values, independently of operand-stack positions.
 * Specialization remembers both outcomes of a test. Liveness then assigns the
 * same kind of temporary slot to extracted values and pending captures.
 */

enum dag_kind {
    DAG_ENTER, DAG_BIND, DAG_COMMIT, DAG_GUARD, DAG_ACCEPT, DAG_END,
    DAG_LOAD, DAG_ATTRIBUTE, DAG_SEQUENCE, DAG_MAPPING, DAG_LENGTH, DAG_LENGTH_EQ,
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
dag_expression(match_dag *d, expr_ty expr)
{
    if (expr->kind == Attribute_kind) {
        if (Py_EnterRecursiveCall(" during match DAG expression construction")) {
            return ERROR;
        }
        int base = dag_expression(d, expr->v.Attribute.value);
        Py_LeaveRecursiveCall();
        RETURN_IF_ERROR(base);
        return dag_operation_new(d, DAG_ATTRIBUTE, DAG_OP(d, base).output,
                                 -1, base, expr->v.Attribute.attr, NULL, 1, 0);
    }
    return dag_operation_new(d, DAG_LOAD, -1, -1, 0, NULL, expr, 1, 0);
}

static int
dag_expression_nodes(match_dag *d, int op, int yes, location loc)
{
    while (op >= 0 && DAG_OP(d, op).kind == DAG_ATTRIBUTE) {
        yes = dag_node_new(d, op, yes, -1, loc);
        RETURN_IF_ERROR(yes);
        op = DAG_OP(d, op).arg;
    }
    return dag_node_new(d, op, yes, -1, loc);
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
            int load = dag_expression(d, p->v.MatchValue.value);
            RETURN_IF_ERROR(load);
            yes = dag_emit(d, DAG_EQUAL, value, DAG_OP(d, load).output,
                           0, NULL, NULL, 0, 0, yes, no, loc);
            return dag_expression_nodes(d, load, yes, loc);
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
                int op = dag_expression(d, asdl_seq_GET(p->v.MatchMapping.keys, i));
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
                yes = dag_expression_nodes(d, load, yes, loc);
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
            int load = dag_expression(d, p->v.MatchClass.cls);
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
            return dag_expression_nodes(d, load, yes, loc);
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

/* Length comparisons use integer bounds, so their implications do not depend
 * on the pattern that introduced them. Record these while the tested fact is
 * still available, before the continuation drops facts it no longer needs. */
static void
dag_record_fact(match_dag *d, unsigned char *facts, int tested, int outcome)
{
    facts[tested] = outcome;
    dag_operation op = DAG_OP(d, tested);
    Py_ssize_t lower = 0, upper = PY_SSIZE_T_MAX;
    if (op.kind == DAG_LENGTH_EQ && outcome == 1) {
        lower = upper = op.arg;
    }
    else if (op.kind == DAG_LENGTH_GE) {
        if (outcome == 1) {
            lower = op.arg;
        }
        else {
            upper = (Py_ssize_t)op.arg - 1;
        }
    }
    else {
        return;
    }
    for (int i = 0; i < d->nops; i++) {
        dag_operation other = DAG_OP(d, i);
        if (other.a != op.a) {
            continue;
        }
        if (other.kind == DAG_LENGTH_EQ) {
            if (lower == upper && lower == other.arg) {
                facts[i] = 1;
            }
            else if (other.arg < lower || other.arg > upper) {
                facts[i] = 2;
            }
        }
        else if (other.kind == DAG_LENGTH_GE) {
            if (lower >= other.arg) {
                facts[i] = 1;
            }
            else if (upper < other.arg) {
                facts[i] = 2;
            }
        }
    }
}

/* The source is acyclic and each node is created after its successors. Facts
 * irrelevant to the remaining tests are removed before memoizing a state. */
static int
dag_specialize(match_dag *d, int source, const unsigned char *context)
{
    dag_node node = DAG_NODE(d, source);
    if (node.yes < 0 || d->states >= 4 * d->original) {
        return source;
    }
    if (Py_EnterRecursiveCall(" during match DAG specialization")) {
        return ERROR;
    }
    PyObject *key = PyBytes_FromStringAndSize(NULL, sizeof(int) + d->nops);
    if (key == NULL) {
        Py_LeaveRecursiveCall();
        return ERROR;
    }
    char *bytes = PyBytes_AS_STRING(key);
    memcpy(bytes, &source, sizeof(int));
    unsigned char *facts = (unsigned char *)bytes + sizeof(int);
    for (int i = 0; i < d->nops; i++) {
        facts[i] = d->facts_live[(size_t)source * d->nops + i] ? context[i] : 0;
    }
    PyObject *found = PyDict_GetItemWithError(d->state_ids, key);
    int result = ERROR;
    if (found) {
        result = (int)PyLong_AsLong(found);
    }
    else if (!PyErr_Occurred()) {
        d->states++;
        dag_operation op = DAG_OP(d, node.op);
        int known = op.reusable ? facts[node.op] : 0;
        if (known) {
            d->removed++;
            result = dag_specialize(d, known == 1 ? node.yes : node.no, facts);
        }
        else {
            unsigned char *after = PyMem_Malloc(d->nops);
            if (after == NULL) {
                PyErr_NoMemory();
            }
            else {
                if (op.kind == DAG_GUARD) {
                    memset(after, 0, d->nops);
                }
                else {
                    memcpy(after, facts, d->nops);
                }
                if (op.reusable) {
                    dag_record_fact(d, after, node.op, 1);
                }
                int yes = dag_specialize(d, node.yes, after);
                int no = -1;
                if (yes >= 0 && node.no >= 0) {
                    if (op.kind == DAG_GUARD) {
                        memset(after, 0, d->nops);
                    }
                    else {
                        memcpy(after, facts, d->nops);
                    }
                    if (op.reusable) {
                        dag_record_fact(d, after, node.op, 2);
                    }
                    no = dag_specialize(d, node.no, after);
                }
                PyMem_Free(after);
                if (yes >= 0 && !PyErr_Occurred()) {
                    result = dag_node_new(d, node.op, yes, no, node.loc);
                }
            }
        }
        if (result >= 0) {
            PyObject *number = PyLong_FromLong(result);
            if (number == NULL || PyDict_SetItem(d->state_ids, key, number) < 0) {
                result = ERROR;
            }
            Py_XDECREF(number);
        }
    }
    Py_DECREF(key);
    Py_LeaveRecursiveCall();
    return result;
}

static void
dag_inputs(match_dag *d, dag_operation *op, unsigned char *live)
{
    if (op->a >= 0 && !d->constants[op->a]) {
        live[op->a] = 1;
    }
    if (op->b >= 0 && !d->constants[op->b]) {
        live[op->b] = 1;
    }
    if (op->kind == DAG_TUPLE) {
        for (int i = 0; i < PyTuple_GET_SIZE(op->data); i++) {
            int value = (int)PyLong_AsLong(PyTuple_GET_ITEM(op->data, i));
            if (!d->constants[value]) {
                live[value] = 1;
            }
        }
    }
    if (op->kind == DAG_COMMIT) {
        PyObject *stores = d->stores[op->arg];
        for (int i = 0; i < PyList_GET_SIZE(stores); i++) {
            PyObject *id = PyDict_GetItem(d->captures, PyList_GET_ITEM(stores, i));
            live[PyLong_AsLong(id)] = 1;
        }
    }
}

static int
dag_allocate(match_dag *d, int root)
{
    size_t nv = d->nvalues;
    d->constants = PyMem_Calloc(nv, sizeof(PyObject *));
    if (d->constants == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    /* Constants need no temporary storage. Recreate them at their uses. */
    for (int i = 0; i < d->nops; i++) {
        dag_operation *op = &DAG_OP(d, i);
        if (op->kind == DAG_LOAD && op->expr->kind == Constant_kind) {
            d->constants[op->output] = Py_NewRef(op->expr->v.Constant.value);
        }
        else if (op->kind == DAG_TUPLE) {
            int constant = 1;
            for (int j = 0; j < op->arg; j++) {
                int value = (int)PyLong_AsLong(PyTuple_GET_ITEM(op->data, j));
                constant &= d->constants[value] != NULL;
            }
            if (constant) {
                PyObject *tuple = PyTuple_New(op->arg);
                if (tuple == NULL) {
                    return ERROR;
                }
                for (int j = 0; j < op->arg; j++) {
                    int value = (int)PyLong_AsLong(PyTuple_GET_ITEM(op->data, j));
                    PyTuple_SET_ITEM(tuple, j, Py_NewRef(d->constants[value]));
                }
                d->constants[op->output] = tuple;
            }
        }
    }
    d->reachable = PyMem_Calloc(d->nnodes, 1);
    d->values_live = PyMem_Calloc(d->nnodes, nv);
    d->colors = PyMem_Malloc(nv * sizeof(int));
    unsigned char *interference = PyMem_Calloc(nv, nv);
    unsigned char *after = PyMem_Calloc(nv, 1);
    int *active = PyMem_Malloc(nv * sizeof(int));
    if (!d->reachable || !d->values_live || !d->colors || !interference ||
        !after || !active) {
        PyMem_Free(interference);
        PyMem_Free(after);
        PyMem_Free(active);
        PyErr_NoMemory();
        return ERROR;
    }
    d->reachable[root] = 1;
    for (int i = root; i >= 0; i--) {
        if (!d->reachable[i]) {
            continue;
        }
        dag_node node = DAG_NODE(d, i);
        if (node.yes >= 0) {
            d->reachable[node.yes] = 1;
        }
        if (node.no >= 0) {
            d->reachable[node.no] = 1;
        }
    }
    for (int i = 0; i < d->nnodes; i++) {
        if (!d->reachable[i]) {
            continue;
        }
        dag_node node = DAG_NODE(d, i);
        dag_operation *op = &DAG_OP(d, node.op);
        unsigned char *live = d->values_live + i * nv;
        memset(after, 0, nv);
        if (node.yes >= 0) {
            memcpy(after, d->values_live + node.yes * nv, nv);
        }
        if (node.no >= 0) {
            for (size_t v = 0; v < nv; v++) {
                after[v] |= d->values_live[node.no * nv + v];
            }
        }
        memcpy(live, after, nv);
        if (op->kind == DAG_GUARD) {
            memset(live + 1, 0, nv - 1);
        }
        else {
            memset(live + op->output, 0, op->count);
        }
        dag_inputs(d, op, live);
        int count = 0;
        for (size_t v = 0; v < nv; v++) {
            if (live[v]) {
                active[count++] = (int)v;
            }
        }
        for (int a = 0; a < count; a++) {
            for (int b = 0; b < a; b++) {
                interference[active[a] * nv + active[b]] = 1;
                interference[active[b] * nv + active[a]] = 1;
            }
        }
        for (int a = op->output; a < op->output + op->count; a++) {
            if (!after[a]) {
                continue;
            }
            for (size_t b = 0; b < nv; b++) {
                if (after[b] && b != (size_t)a) {
                    interference[a * nv + b] = interference[b * nv + a] = 1;
                }
            }
        }
    }
    for (size_t a = 0; a < nv; a++) {
        memset(after, 0, nv);
        for (size_t b = 0; b < a; b++) {
            if (interference[a * nv + b]) {
                after[d->colors[b]] = 1;
            }
        }
        int color = 0;
        while (after[color]) {
            color++;
        }
        /* Prefer the same slot for a move's input and output when their live
         * ranges do not overlap. This also handles OR capture permutations. */
        for (int i = 0; i < d->nops; i++) {
            dag_operation *op = &DAG_OP(d, i);
            if (op->kind == DAG_BIND && op->a == (int)a &&
                op->output < (int)a && !after[d->colors[op->output]]) {
                color = d->colors[op->output];
                break;
            }
        }
        d->colors[a] = color;
        d->slots = Py_MAX(d->slots, color + 1);
    }
    PyMem_Free(interference);
    PyMem_Free(after);
    PyMem_Free(active);
    return SUCCESS;
}

static int
dag_copy(match_dag *d, int value, int extra, location loc)
{
    if (d->constants[value]) {
        return codegen_addop_load_const(d->c, loc, d->constants[value]);
    }
    return codegen_addop_i(INSTR_SEQUENCE(d->c), COPY,
                          d->slots + extra - d->colors[value], loc);
}

static int
dag_store(match_dag *d, int value, int extra, location loc)
{
    compiler *c = d->c;
    ADDOP_I(c, loc, SWAP, d->slots + extra - d->colors[value]);
    ADDOP(c, loc, POP_TOP);
    return SUCCESS;
}

static location
dag_destination_location(match_dag *d, stmt_ty s, int target)
{
    for (;;) {
        dag_node node = DAG_NODE(d, target);
        dag_operation *op = &DAG_OP(d, node.op);
        if (op->kind == DAG_ACCEPT) {
            match_case_ty m = asdl_seq_GET(s->v.Match.cases, op->arg);
            return LOC((stmt_ty)asdl_seq_GET(m->body, 0));
        }
        if ((op->kind == DAG_COMMIT && !PyList_GET_SIZE(d->stores[op->arg])) ||
            ((op->kind == DAG_LOAD || op->kind == DAG_TUPLE) &&
             d->constants[op->output])) {
            target = node.yes;
            continue;
        }
        return node.loc;
    }
}

/* All edges use one allocated layout. Clear values at their last use; the
 * normal exception unwinder also owns and releases these operand-stack slots. */
static int
dag_edge(match_dag *d, int source, int target, int define, location loc)
{
    compiler *c = d->c;
    int kind = DAG_OP(d, DAG_NODE(d, target).op).kind;
    if (kind == DAG_ACCEPT || kind == DAG_END) {
        /* The destination releases every slot. Clearing them first would
         * add a load and swap for each value immediately before its pop. */
        ADDOP_JUMP(c, NO_LOCATION, JUMP, d->labels[target]);
        return SUCCESS;
    }
    dag_operation *op = &DAG_OP(d, DAG_NODE(d, source).op);
    unsigned char *before = d->values_live + (size_t)source * d->nvalues;
    unsigned char *after = d->values_live + (size_t)target * d->nvalues;
    for (int slot = 0; slot < d->slots; slot++) {
        if (kind == DAG_GUARD && slot != d->colors[0]) {
            /* The guard clears these before evaluating its expression. */
            continue;
        }
        int was_live = 0, remains = 0;
        for (int v = 0; v < d->nvalues; v++) {
            if (d->colors[v] == slot) {
                was_live |= before[v] || (define && v >= op->output && v < op->output + op->count);
                remains |= after[v];
            }
        }
        if (was_live && !remains) {
            ADDOP_LOAD_CONST(c, loc, Py_None);
            ADDOP_I(c, loc, SWAP, d->slots + 1 - slot);
            ADDOP(c, loc, POP_TOP);
        }
    }
    ADDOP_JUMP(c, NO_LOCATION, JUMP, d->labels[target]);
    return SUCCESS;
}

static int
dag_codegen_node(match_dag *d, stmt_ty s, int index)
{
    compiler *c = d->c;
    dag_node node = DAG_NODE(d, index);
    dag_operation op = DAG_OP(d, node.op);
    location loc = node.loc;
    USE_LABEL(c, d->labels[index]);
    if ((op.kind == DAG_LOAD || op.kind == DAG_TUPLE) &&
        d->constants[op.output]) {
        ADDOP_JUMP(c, NO_LOCATION, JUMP, d->labels[node.yes]);
        return SUCCESS;
    }
    if (op.kind == DAG_BIND && d->colors[op.a] == d->colors[op.output]) {
        return dag_edge(d, index, node.yes, 1, loc);
    }
    if (op.kind == DAG_ACCEPT || op.kind == DAG_END) {
        loc = dag_destination_location(d, s, index);
        for (int i = 0; i < d->slots; i++) {
            ADDOP(c, loc, POP_TOP);
        }
        ADDOP_JUMP(c, NO_LOCATION, JUMP,
                   op.kind == DAG_END ? d->end : d->bodies[op.arg]);
        return SUCCESS;
    }
    if (op.kind == DAG_GUARD) {
        /* Captures have been committed. A guard can mutate any value examined
         * by a pattern, even when it eventually returns true. */
        for (int slot = 0; slot < d->slots; slot++) {
            if (slot != d->colors[0]) {
                ADDOP_LOAD_CONST(c, NO_LOCATION, Py_None);
                ADDOP_I(c, NO_LOCATION, SWAP, d->slots + 1 - slot);
                ADDOP(c, NO_LOCATION, POP_TOP);
            }
        }
        NEW_JUMP_TARGET_LABEL(c, fail);
        match_case_ty m = asdl_seq_GET(s->v.Match.cases, op.arg);
        RETURN_IF_ERROR(codegen_jump_if(c, loc, m->guard, fail, 0));
        ADDOP_JUMP(c, NO_LOCATION, JUMP, d->labels[node.yes]);
        USE_LABEL(c, fail);
        ADDOP_JUMP(c, NO_LOCATION, JUMP, d->labels[node.no]);
        return SUCCESS;
    }
    if (op.kind == DAG_COMMIT) {
        PyObject *stores = d->stores[op.arg];
        for (int i = 0; i < PyList_GET_SIZE(stores); i++) {
            PyObject *name = PyList_GET_ITEM(stores, i);
            int value = (int)PyLong_AsLong(PyDict_GetItem(d->captures, name));
            RETURN_IF_ERROR(dag_copy(d, value, 0, loc));
            RETURN_IF_ERROR(codegen_nameop(c, loc, name, Store));
        }
    }
    else if (op.kind == DAG_ENTER) {
        ADDOP(c, loc, NOP);
    }
    else {
        if (op.a >= 0) {
            RETURN_IF_ERROR(dag_copy(d, op.a, 0, loc));
        }
        if (op.b >= 0 && op.kind != DAG_INDEX) {
            RETURN_IF_ERROR(dag_copy(d, op.b, 1, loc));
        }
        switch (op.kind) {
            case DAG_BIND:
                break;
            case DAG_ATTRIBUTE:
                ADDOP_NAME(c, loc, LOAD_ATTR, op.data, names);
                break;
            case DAG_LOAD: {
                int start = INSTR_SEQUENCE(c)->s_used;
                VISIT(c, expr, op.expr);
                /* The operation can originate at a different occurrence of
                 * the same expression. Locations belong to graph nodes. */
                for (int i = start; i < INSTR_SEQUENCE(c)->s_used; i++) {
                    INSTR_SEQUENCE(c)->s_instrs[i].i_loc = loc;
                }
                break;
            }
            case DAG_SEQUENCE:
            case DAG_MAPPING:
            case DAG_LENGTH:
                ADDOP(c, loc, op.kind == DAG_SEQUENCE ? MATCH_SEQUENCE :
                      op.kind == DAG_MAPPING ? MATCH_MAPPING : GET_LEN);
                ADDOP_I(c, loc, SWAP, 2);
                ADDOP(c, loc, POP_TOP);
                break;
            case DAG_LENGTH_EQ:
            case DAG_LENGTH_GE:
                ADDOP_LOAD_CONST_NEW(c, loc, PyLong_FromLong(op.arg));
                ADDOP_COMPARE(c, loc, op.kind == DAG_LENGTH_EQ ? Eq : GtE);
                break;
            case DAG_EQUAL:
                ADDOP_COMPARE(c, loc, Eq);
                ADDOP(c, loc, TO_BOOL);
                break;
            case DAG_SINGLETON:
                ADDOP_LOAD_CONST(c, loc, op.data);
                ADDOP_I(c, loc, IS_OP, 0);
                break;
            case DAG_INDEX:
                if (op.arg < 0) {
                    RETURN_IF_ERROR(dag_copy(d, op.b, 1, loc));
                    ADDOP_LOAD_CONST_NEW(c, loc, PyLong_FromLong(-op.arg));
                    ADDOP_BINARY(c, loc, Sub);
                }
                else {
                    ADDOP_LOAD_CONST_NEW(c, loc, PyLong_FromLong(op.arg));
                }
                ADDOP_I(c, loc, BINARY_OP, NB_SUBSCR);
                break;
            case DAG_TUPLE:
                for (int i = 0; i < op.arg; i++) {
                    int value = (int)PyLong_AsLong(PyTuple_GET_ITEM(op.data, i));
                    RETURN_IF_ERROR(dag_copy(d, value, i, loc));
                }
                ADDOP_I(c, loc, BUILD_TUPLE, op.arg);
                break;
            case DAG_UNPACK:
                ADDOP_I(c, loc, op.data == Py_True ? UNPACK_EX : UNPACK_SEQUENCE, op.arg);
                break;
            case DAG_KEYS:
                ADDOP(c, loc, MATCH_KEYS);
                ADDOP_I(c, loc, SWAP, 2);
                ADDOP(c, loc, POP_TOP);
                ADDOP_I(c, loc, SWAP, 2);
                ADDOP(c, loc, POP_TOP);
                break;
            case DAG_CLASS:
                ADDOP_LOAD_CONST(c, loc, op.data);
                ADDOP_I(c, loc, MATCH_CLASS, op.arg);
                break;
            case DAG_REST:
                ADDOP_I(c, loc, BUILD_MAP, 0);
                ADDOP_I(c, loc, SWAP, 3);
                ADDOP_I(c, loc, DICT_UPDATE, 2);
                ADDOP_I(c, loc, UNPACK_SEQUENCE, op.arg);
                for (int i = op.arg; i > 0; i--) {
                    ADDOP_I(c, loc, COPY, 1 + i);
                    ADDOP_I(c, loc, SWAP, 2);
                    ADDOP(c, loc, DELETE_SUBSCR);
                }
                break;
            default:
                Py_UNREACHABLE();
        }
    }
    NEW_JUMP_TARGET_LABEL(c, fail);
    int optional = op.kind == DAG_CLASS || op.kind == DAG_KEYS;
    if (node.no >= 0) {
        if (optional) {
            ADDOP_I(c, loc, COPY, 1);
            ADDOP_JUMP(c, loc, POP_JUMP_IF_NONE, fail);
        }
        else {
            ADDOP_JUMP(c, loc, POP_JUMP_IF_FALSE, fail);
        }
    }
    for (int i = 0; i < op.count; i++) {
        int value = op.output + i;
        if (d->values_live[(size_t)node.yes * d->nvalues + value]) {
            RETURN_IF_ERROR(dag_store(d, value, op.count - i, loc));
        }
        else {
            ADDOP(c, loc, POP_TOP);
        }
    }
    RETURN_IF_ERROR(dag_edge(d, index, node.yes, 1,
                            dag_destination_location(d, s, node.yes)));
    if (node.no >= 0) {
        USE_LABEL(c, fail);
        loc = dag_destination_location(d, s, node.no);
        if (optional) {
            ADDOP(c, loc, POP_TOP);
        }
        RETURN_IF_ERROR(dag_edge(d, index, node.no, 0, loc));
    }
    return SUCCESS;
}

static int
dag_build(match_dag *d, stmt_ty s)
{
    compiler *c = d->c;
    int ncases = asdl_seq_LEN(s->v.Match.cases);
    /* Keep CPython's existing diagnostics and capture-order validation. The
     * temporary instructions are discarded before graph construction. */
    for (int i = 0; i < ncases; i++) {
        match_case_ty m = asdl_seq_GET(s->v.Match.cases, i);
        pattern_context pc = {.stores = PyList_New(0),
                             .allow_irrefutable = m->guard != NULL || i == ncases - 1};
        if (pc.stores == NULL) {
            return ERROR;
        }
        int start = INSTR_SEQUENCE(c)->s_used;
        int labels = INSTR_SEQUENCE(c)->s_next_free_label;
        int result = codegen_pattern(c, m->pattern, &pc);
        INSTR_SEQUENCE(c)->s_used = start;
        INSTR_SEQUENCE(c)->s_next_free_label = labels;
        PyMem_Free(pc.fail_pop);
        if (result < 0) {
            Py_DECREF(pc.stores);
            return ERROR;
        }
        d->stores[i] = pc.stores;
        for (int j = 0; j < PyList_GET_SIZE(pc.stores); j++) {
            RETURN_IF_ERROR(dag_capture(d, PyList_GET_ITEM(pc.stores, j)));
        }
    }
    int tail = dag_emit(d, DAG_END, -1, -1, 0, NULL, NULL, 0, 0,
                        -1, -1, NO_LOCATION);
    for (int i = ncases; i--;) {
        match_case_ty m = asdl_seq_GET(s->v.Match.cases, i);
        int yes = dag_emit(d, DAG_ACCEPT, -1, -1, i, NULL, NULL, 0, 0,
                           -1, -1, NO_LOCATION);
        if (m->guard) {
            yes = dag_emit(d, DAG_GUARD, -1, -1, i, NULL, NULL, 0, 0,
                           yes, tail, LOC(m->pattern));
        }
        if (PyList_GET_SIZE(d->stores[i])) {
            yes = dag_emit(d, DAG_COMMIT, -1, -1, i, NULL, NULL, 0, 0,
                           yes, -1, LOC(m->pattern));
        }
        yes = dag_pattern(d, m->pattern, 0, yes, tail);
        tail = dag_emit(d, DAG_ENTER, -1, -1, i, NULL, NULL, 0, 0,
                        yes, -1, LOC(m->pattern));
        RETURN_IF_ERROR(tail);
    }
    d->original = d->nnodes;
    d->facts_live = PyMem_Calloc(d->original, d->nops);
    unsigned char *context = PyMem_Calloc(d->nops, 1);
    if (d->facts_live == NULL || context == NULL) {
        PyMem_Free(context);
        PyErr_NoMemory();
        return ERROR;
    }
    for (int i = 0; i < d->original; i++) {
        dag_node node = DAG_NODE(d, i);
        dag_operation op = DAG_OP(d, node.op);
        unsigned char *live = d->facts_live + (size_t)i * d->nops;
        if (op.kind != DAG_GUARD) {
            for (int j = 0; j < d->nops; j++) {
                live[j] = (node.yes >= 0 && d->facts_live[(size_t)node.yes * d->nops + j]) ||
                          (node.no >= 0 && d->facts_live[(size_t)node.no * d->nops + j]);
            }
        }
        if (op.reusable) {
            live[node.op] = 1;
        }
    }
    int root = dag_specialize(d, tail, context);
    PyMem_Free(context);
    RETURN_IF_ERROR(root);
    if (!d->removed) {
        /* Nothing was shared. Preserve the ordinary compiler's stack layout
         * instead of paying for graph temporaries with no possible saving. */
        return 1;
    }
    RETURN_IF_ERROR(dag_allocate(d, root));
    for (int i = 0; i < d->nnodes; i++) {
        dag_operation *op = &DAG_OP(d, DAG_NODE(d, i).op);
        if (op->kind == DAG_ACCEPT && !d->reachable[i]) {
            match_case_ty m = asdl_seq_GET(s->v.Match.cases, op->arg);
            RETURN_IF_ERROR(_PyCompile_Warn(c, LOC(m->pattern),
                                           "pattern is unreachable"));
        }
    }
    d->labels = PyMem_Malloc(d->nnodes * sizeof(jump_target_label));
    if (d->labels == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    for (int i = 0; i < d->nnodes; i++) {
        d->labels[i] = _PyInstructionSequence_NewLabel(INSTR_SEQUENCE(c));
    }
    for (int i = 0; i < ncases; i++) {
        d->bodies[i] = _PyInstructionSequence_NewLabel(INSTR_SEQUENCE(c));
    }
    d->end = _PyInstructionSequence_NewLabel(INSTR_SEQUENCE(c));
    VISIT(c, expr, s->v.Match.subject);
    for (int i = 1; i < d->slots; i++) {
        ADDOP_LOAD_CONST(c, NO_LOCATION, Py_None);
    }
    assert(d->colors[0] == 0);
    ADDOP_JUMP(c, NO_LOCATION, JUMP, d->labels[root]);
    for (int i = d->nnodes; i--;) {
        if (d->reachable[i]) {
            RETURN_IF_ERROR(dag_codegen_node(d, s, i));
        }
    }
    for (int i = 0; i < ncases; i++) {
        match_case_ty m = asdl_seq_GET(s->v.Match.cases, i);
        USE_LABEL(c, d->bodies[i]);
        VISIT_SEQ(c, stmt, m->body);
        ADDOP_JUMP(c, NO_LOCATION, JUMP, d->end);
    }
    USE_LABEL(c, d->end);
    return SUCCESS;
}

static int
codegen_match_dag(compiler *c, stmt_ty s)
{
    match_dag d = {.c = c, .nvalues = 1};
    _Py_CArray_Init(&d.operations, sizeof(dag_operation), 32);
    _Py_CArray_Init(&d.nodes, sizeof(dag_node), 64);
    d.op_ids = PyDict_New();
    d.node_ids = PyDict_New();
    d.state_ids = PyDict_New();
    d.captures = PyDict_New();
    int ncases = asdl_seq_LEN(s->v.Match.cases);
    d.stores = PyMem_Calloc(ncases, sizeof(PyObject *));
    d.bodies = PyMem_Malloc(ncases * sizeof(jump_target_label));
    int result = ERROR;
    if (d.op_ids && d.node_ids && d.state_ids && d.captures && d.stores && d.bodies) {
        result = dag_build(&d, s);
    }
    else {
        PyErr_NoMemory();
    }
    for (int i = 0; i < d.nops; i++) {
        Py_XDECREF(DAG_OP(&d, i).data);
    }
    if (d.stores) {
        for (int i = 0; i < ncases; i++) {
            Py_XDECREF(d.stores[i]);
        }
    }
    Py_XDECREF(d.op_ids);
    Py_XDECREF(d.node_ids);
    Py_XDECREF(d.state_ids);
    Py_XDECREF(d.captures);
    _Py_CArray_Fini(&d.operations);
    _Py_CArray_Fini(&d.nodes);
    PyMem_Free(d.stores);
    PyMem_Free(d.bodies);
    PyMem_Free(d.facts_live);
    PyMem_Free(d.values_live);
    PyMem_Free(d.reachable);
    PyMem_Free(d.colors);
    PyMem_Free(d.labels);
    if (d.constants) {
        for (int i = 0; i < d.nvalues; i++) {
            Py_XDECREF(d.constants[i]);
        }
    }
    PyMem_Free(d.constants);
    return result;
}

#undef DAG_OP
#undef DAG_NODE
