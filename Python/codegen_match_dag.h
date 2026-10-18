/* Experimental ordered match DAG. Included by codegen.c.
 *
 * Operations name their input values, independently of operand-stack positions.
 * Specialization remembers both outcomes of a test. The stack emitter keeps
 * extracted values and pending captures until their last use.
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
    int *values;
    int size;
} dag_stack;

typedef struct dag_failure {
    struct dag_failure *next;
    jump_target_label label;
    location loc;
    int size, pop_none;
    int values[];
} dag_failure;

typedef struct {
    dag_stack entry;
    jump_target_label label;
    dag_failure *failures;
    int pending;
    bool reachable, single_predecessor, inlined;
} dag_block;

typedef struct {
    compiler *c;
    _Py_c_array_t operations, nodes;
    int nops, nnodes, nvalues, original, states, removed;
    PyObject *op_ids, *node_ids, *state_ids, *captures;
    PyObject **stores;
    PyObject **constants;
    unsigned char *facts_live, *values_live;
    dag_block *blocks;
    jump_target_label *bodies;
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
    PyObject *key = Py_BuildValue("(iiiiiON)", kind, a, b, arg,
                                 fresh ? d->nops + 1 : 0,
                                 data ? data : Py_None, expr_key);
    if (key == NULL) {
        return ERROR;
    }
    int id = (int)_PyCompile_DictAddObj(d->op_ids, key);
    Py_DECREF(key);
    if (id < 0 || id < d->nops) {
        return id;
    }
    RETURN_IF_ERROR(_Py_CArray_EnsureCapacity(&d->operations, id));
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
    int id = (int)_PyCompile_DictAddObj(d->node_ids, key);
    Py_DECREF(key);
    if (id < 0 || id < d->nnodes) {
        return id;
    }
    RETURN_IF_ERROR(_Py_CArray_EnsureCapacity(&d->nodes, id));
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
    int result = ERROR;
    unsigned char *after = NULL;
    PyObject *key = PyBytes_FromStringAndSize(NULL, sizeof(int) + d->nops);
    if (key == NULL) {
        goto done;
    }
    char *bytes = PyBytes_AS_STRING(key);
    memcpy(bytes, &source, sizeof(int));
    unsigned char *facts = (unsigned char *)bytes + sizeof(int);
    for (int i = 0; i < d->nops; i++) {
        facts[i] = d->facts_live[(size_t)source * d->nops + i] ? context[i] : 0;
    }
    PyObject *found = PyDict_GetItemWithError(d->state_ids, key);
    if (found) {
        result = (int)PyLong_AsLong(found);
        goto done;
    }
    if (PyErr_Occurred()) {
        goto done;
    }
    d->states++;
    dag_operation op = DAG_OP(d, node.op);
    int known = op.reusable ? facts[node.op] : 0;
    if (known) {
        d->removed++;
        result = dag_specialize(d, known == 1 ? node.yes : node.no, facts);
    }
    else {
        after = PyMem_Malloc(d->nops);
        if (after == NULL) {
            PyErr_NoMemory();
            goto done;
        }
        int targets[2] = {node.yes, node.no};
        for (int edge = 0; edge < 2; edge++) {
            if (targets[edge] < 0) {
                continue;
            }
            if (op.kind == DAG_GUARD) {
                memset(after, 0, d->nops);
            }
            else {
                memcpy(after, facts, d->nops);
            }
            if (op.reusable) {
                dag_record_fact(d, after, node.op, edge + 1);
            }
            targets[edge] = dag_specialize(d, targets[edge], after);
            if (targets[edge] < 0) {
                goto done;
            }
        }
        result = dag_node_new(d, node.op, targets[0], targets[1], node.loc);
    }
    if (result >= 0) {
        PyObject *number = PyLong_FromLong(result);
        if (number == NULL || PyDict_SetItem(d->state_ids, key, number) < 0) {
            result = ERROR;
        }
        Py_XDECREF(number);
    }
done:
    PyMem_Free(after);
    Py_XDECREF(key);
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
dag_analyze_values(match_dag *d, int root)
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
    d->blocks = PyMem_Calloc(d->nnodes, sizeof(dag_block));
    d->values_live = PyMem_Calloc(d->nnodes, nv);
    if (!d->blocks || !d->values_live) {
        PyErr_NoMemory();
        return ERROR;
    }
    d->blocks[root].reachable = true;
    for (int i = root; i >= 0; i--) {
        if (!d->blocks[i].reachable) {
            continue;
        }
        dag_node node = DAG_NODE(d, i);
        int targets[2] = {node.yes, node.no};
        for (int edge = 0; edge < 2; edge++) {
            if (targets[edge] >= 0) {
                dag_block *target = &d->blocks[targets[edge]];
                target->reachable = true;
                target->pending++;
                target->single_predecessor = target->pending == 1;
            }
        }
    }
    for (int i = 0; i < d->nnodes; i++) {
        if (!d->blocks[i].reachable) {
            continue;
        }
        dag_node node = DAG_NODE(d, i);
        dag_operation *op = &DAG_OP(d, node.op);
        unsigned char *live = d->values_live + i * nv;
        if (node.yes >= 0) {
            memcpy(live, d->values_live + node.yes * nv, nv);
        }
        if (node.no >= 0) {
            for (size_t v = 0; v < nv; v++) {
                live[v] |= d->values_live[node.no * nv + v];
            }
        }
        if (op->kind == DAG_GUARD) {
            memset(live + 1, 0, nv - 1);
        }
        else {
            memset(live + op->output, 0, op->count);
        }
        dag_inputs(d, op, live);
    }
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
        if ((op->kind == DAG_LOAD || op->kind == DAG_TUPLE) &&
            d->constants[op->output]) {
            target = node.yes;
            continue;
        }
        return node.loc;
    }
}

#include "codegen_match_stack.h"

static int
dag_build_patterns(match_dag *d, stmt_ty s)
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
    return tail;
}

static int
dag_optimize(match_dag *d, int root)
{
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
    int result = dag_specialize(d, root, context);
    PyMem_Free(context);
    return result;
}

/* Return 1 when no work was shared, so codegen_match can use ordinary
 * bytecode without the cost of graph temporaries. Otherwise return 0 or -1. */
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
        result = dag_build_patterns(&d, s);
        if (result >= 0) {
            result = dag_optimize(&d, result);
        }
        if (result >= 0) {
            result = d.removed ? dag_emit_match(&d, s, result) : 1;
        }
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
    if (d.blocks) {
        for (int i = 0; i < d.nnodes; i++) {
            PyMem_Free(d.blocks[i].entry.values);
            dag_failure *failure = d.blocks[i].failures;
            while (failure) {
                dag_failure *next = failure->next;
                PyMem_Free(failure);
                failure = next;
            }
        }
        PyMem_Free(d.blocks);
    }
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
