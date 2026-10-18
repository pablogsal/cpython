/* Experimental ordered match DAG. Included by codegen.c.
 *
 * Operations name their input values, independently of operand-stack positions.
 * Specialization remembers both outcomes of a test. The stack emitter keeps
 * extracted values and pending captures until their last use.
 */

#include "pycore_setobject.h"    // _PySet_NextEntry(), _PySet_AddTakeRef()

enum dag_kind {
    DAG_ENTER,      /* Keep the source location of a case entry. */
    DAG_BIND,       /* Copy a value to a pending capture. */
    DAG_COMMIT,     /* Store pending captures in Python names. */
    DAG_GUARD,      /* Evaluate a guard and clear cached facts and values. */
    DAG_ACCEPT,     /* Jump to the selected case body. */
    DAG_END,        /* No case matched. */
    /* Kinds from DAG_LOAD onward can be shared unless marked fresh. */
    DAG_LOAD,      /* Evaluate a pattern expression. */
    DAG_ATTRIBUTE, /* Read an attribute from a pattern expression. */
    DAG_SEQUENCE, DAG_MAPPING, /* Check the subject protocol. */
    DAG_LENGTH,    /* Read a length for subsequent integer comparisons. */
    DAG_LENGTH_EQ, DAG_LENGTH_GE, /* Exact and minimum lengths. */
    DAG_EQUAL, DAG_SINGLETON,  /* Equality and identity comparisons. */
    DAG_UNPACK,    /* Unpack a sequence or an extraction tuple. */
    DAG_INDEX,     /* Read an item before or after a wildcard star. */
    DAG_TUPLE,     /* Build the tuple of mapping keys. */
    DAG_KEYS, DAG_CLASS, /* Extract mapping values or class attributes. */
    DAG_REST       /* Build a fresh mapping for a rest capture. */
};

typedef struct {
    enum dag_kind kind;
    int a, b;           /* Input value IDs, or -1 for absent inputs. */
    int arg;            /* Kind-specific index, count, or comparison bound. */
    int output, count;  /* Consecutive output IDs; BIND uses a capture ID. */
    int first;          /* Smallest original node that can use this fact. */
    bool reusable;      /* Reuse is limited to paths with no executed guard. */
    PyObject *data;     /* Owned constant, names, or tuple of producer IDs. */
    expr_ty expr;       /* Borrowed AST for LOAD. */
} dag_operation;

typedef struct {
    int op;             /* Index in operations. */
    int yes, no;        /* Successor node IDs, or -1; successors come first. */
    location loc;
    bool uses_facts;    /* A reusable operation is reachable before a guard. */
} dag_node;

typedef struct {
    int *values;        /* Value IDs; -1 marks a prepared stack operand. */
    int size;
} dag_stack;

typedef struct dag_failure {
    struct dag_failure *next;
    jump_target_label label;
    location loc;
    int size, pop_none;  /* Snapshot excludes the None result on failure. */
    int values[];
} dag_failure;

typedef struct {
    dag_stack entry;     /* Layout fixed by the first incoming edge. */
    dag_stack live;      /* Sorted IDs of values needed at block entry. */
    jump_target_label label;
    dag_failure *failures;
    int pending;        /* Incoming graph edges not yet processed. */
    bool single_predecessor, inlined;
} dag_block;

typedef struct {
    compiler *c;
    _Py_c_array_t operations, nodes;
    int nops, nnodes, nvalues, original, states, removed;
    PyObject *op_ids, *node_ids, *state_ids, *captures;
    PyObject *stores;    /* Owned tuple of capture-name lists, one per case. */
    PyObject **constants;
    int *length_first;   /* Last possible use of a bound on each value. */
    dag_block *blocks;
    jump_target_label *bodies;
    jump_target_label end;
} match_dag;

#define DAG_OP(d, i) (((dag_operation *)(d)->operations.array)[i])
#define DAG_NODE(d, i) (((dag_node *)(d)->nodes.array)[i])
/* Bound per-state facts, live values, and operation outputs. The state budget
 * then also bounds the total DAG analysis and stack-layout work. */
#define DAG_MAX_WIDTH 64

static int
dag_capture(match_dag *d, PyObject *name)
{
    /* Capture indices start at zero; value zero is the match subject. */
    PyObject *index = PyDict_GetItem(d->captures, name);
    assert(index != NULL);
    return (int)PyLong_AsLong(index) + 1;
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
dag_operation_new(match_dag *d, enum dag_kind kind, int a, int b, int arg,
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
    if (id < d->nops) {
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
    const int fields[] = {op, yes, no, loc.lineno, loc.end_lineno,
                          loc.col_offset, loc.end_col_offset};
    PyObject *key = PyBytes_FromStringAndSize((const char *)fields, sizeof(fields));
    if (key == NULL) {
        return ERROR;
    }
    int id = (int)_PyCompile_DictAddObj(d->node_ids, key);
    Py_DECREF(key);
    if (id < d->nnodes) {
        return id;
    }
    RETURN_IF_ERROR(_Py_CArray_EnsureCapacity(&d->nodes, id));
    dag_operation *operation = &DAG_OP(d, op);
    bool uses_facts = operation->kind != DAG_GUARD &&
        (operation->reusable ||
         (yes >= 0 && DAG_NODE(d, yes).uses_facts) ||
         (no >= 0 && DAG_NODE(d, no).uses_facts));
    DAG_NODE(d, id) = (dag_node){op, yes, no, loc, uses_facts};
    d->nnodes++;
    return id;
}

static int
dag_emit(match_dag *d, enum dag_kind kind, int a, int b, int arg, PyObject *data,
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
    return dag_emit(d, DAG_BIND, value, -1, dag_capture(d, name), NULL, NULL, 1, 0,
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
            int tuple = dag_operation_new(d, DAG_TUPLE, -1, -1, n, loads, NULL, 1, 0);
            Py_DECREF(loads);
            RETURN_IF_ERROR(tuple);
            loads = DAG_OP(d, tuple).data;  /* The operation owns this tuple. */
            int keys_value = DAG_OP(d, tuple).output;
            if (p->v.MatchMapping.rest) {
                int rest = dag_operation_new(d, DAG_REST, value, keys_value,
                                             n, NULL, NULL, 1, 1);
                RETURN_IF_ERROR(rest);
                yes = dag_bind(d, p->v.MatchMapping.rest,
                               DAG_OP(d, rest).output, yes, loc);
                yes = dag_node_new(d, rest, yes, -1, loc);
                RETURN_IF_ERROR(yes);
            }
            int keys = dag_operation_new(d, DAG_KEYS, value, keys_value, 0, NULL, NULL, 1, 0);
            RETURN_IF_ERROR(keys);
            int unpack = dag_operation_new(d, DAG_UNPACK, DAG_OP(d, keys).output,
                                           -1, n, Py_False, NULL, n, 0);
            RETURN_IF_ERROR(unpack);
            int output = DAG_OP(d, unpack).output;
            for (int i = n; i--;) {
                yes = dag_pattern(d, asdl_seq_GET(p->v.MatchMapping.patterns, i),
                                  output + i, yes, no);
                RETURN_IF_ERROR(yes);
            }
            yes = dag_node_new(d, unpack, yes, -1, loc);
            yes = dag_node_new(d, keys, yes, no, loc);
            yes = dag_node_new(d, tuple, yes, -1, loc);
            RETURN_IF_ERROR(yes);
            for (int i = n; i--;) {
                int load = (int)PyLong_AsLong(PyTuple_GET_ITEM(loads, i));
                yes = dag_expression_nodes(d, load, yes, loc);
                RETURN_IF_ERROR(yes);
            }
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
            int load = dag_expression(d, p->v.MatchClass.cls);
            RETURN_IF_ERROR(load);
            int nargs = asdl_seq_LEN(p->v.MatchClass.patterns);
            asdl_identifier_seq *attrs = p->v.MatchClass.kwd_attrs;
            int nattrs = asdl_seq_LEN(attrs);
            int n = nargs + nattrs;
            PyObject *names = PyTuple_FromArray(attrs ? attrs->typed_elements : NULL, nattrs);
            if (names == NULL) {
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

/* A positive fact encodes a successful operation ID plus one; a negative
 * fact encodes failure. Length implications are queried when needed, rather
 * than stored as separate facts for every possible comparison. */
static int
dag_known(match_dag *d, int fact, int tested)
{
    int id = abs(fact) - 1, outcome = fact > 0 ? 1 : 2;
    if (id == tested) {
        return outcome;
    }
    dag_operation op = DAG_OP(d, id), target = DAG_OP(d, tested);
    if (op.a != target.a ||
        (target.kind != DAG_LENGTH_EQ && target.kind != DAG_LENGTH_GE)) {
        return 0;
    }
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
        return 0;
    }
    if (target.kind == DAG_LENGTH_EQ) {
        return lower == upper && lower == target.arg ? 1 :
               target.arg < lower || target.arg > upper ? 2 : 0;
    }
    return lower >= target.arg ? 1 : upper < target.arg ? 2 : 0;
}

/* Successors precede their source. A fact whose last possible use is behind
 * us cannot affect any continuation. Frozen sets make memo keys independent
 * of the order in which facts were learned. */
static int
dag_specialize(match_dag *d, int source, PyObject *context)
{
    dag_node node = DAG_NODE(d, source);
    if (node.yes < 0 || d->states >= 4 * d->original ||
        PySet_GET_SIZE(context) > DAG_MAX_WIDTH ||
        _Py_ReachedRecursionLimitWithMargin(_PyThreadState_GET(), 1)) {
        return source;
    }
    int result = ERROR, known = 0;
    PyObject *key = NULL, *facts = PyFrozenSet_New(NULL);
    if (facts == NULL) {
        return ERROR;
    }
    dag_operation op = DAG_OP(d, node.op);
    Py_ssize_t pos = 0;
    PyObject *item;
    Py_hash_t hash;
    while (node.uses_facts && _PySet_NextEntry(context, &pos, &item, &hash)) {
        int fact = (int)PyLong_AsLong(item);
        dag_operation previous = DAG_OP(d, abs(fact) - 1);
        int first = previous.first;
        if (previous.kind == DAG_LENGTH_GE ||
            (previous.kind == DAG_LENGTH_EQ && fact > 0)) {
            first = d->length_first[previous.a];
        }
        if (first <= source) {
            if (PySet_Add(facts, item) < 0) {
                goto done;
            }
            if (op.reusable && !known) {
                known = dag_known(d, fact, node.op);
            }
        }
    }
    key = Py_BuildValue("(iO)", source, facts);
    if (key == NULL) {
        goto done;
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
    if (known) {
        d->removed++;
        result = dag_specialize(d, known == 1 ? node.yes : node.no, facts);
    }
    else {
        int targets[2] = {node.yes, node.no};
        for (int edge = 0; edge < 2; edge++) {
            if (targets[edge] < 0) {
                continue;
            }
            PyObject *after = op.reusable ? PySet_New(facts) : Py_NewRef(facts);
            if (after == NULL) {
                goto done;
            }
            if (op.reusable) {
                int value = edge ? -node.op - 1 : node.op + 1;
                PyObject *fact = PyLong_FromLong(value);
                if (fact == NULL || _PySet_AddTakeRef((PySetObject *)after, fact) < 0) {
                    Py_DECREF(after);
                    goto done;
                }
            }
            targets[edge] = dag_specialize(d, targets[edge], after);
            Py_DECREF(after);
            if (targets[edge] < 0) {
                goto done;
            }
        }
        result = targets[0] == node.yes && targets[1] == node.no ? source :
                 dag_node_new(d, node.op, targets[0], targets[1], node.loc);
    }
    if (result >= 0) {
        PyObject *number = PyLong_FromLong(result);
        if (number == NULL || PyDict_SetItem(d->state_ids, key, number) < 0) {
            result = ERROR;
        }
        Py_XDECREF(number);
    }
done:
    Py_DECREF(facts);
    Py_XDECREF(key);
    return result;
}

static int
dag_tuple_value(match_dag *d, dag_operation *op, int index)
{
    int producer = (int)PyLong_AsLong(PyTuple_GET_ITEM(op->data, index));
    return DAG_OP(d, producer).output;
}

static int
dag_compare_values(const void *left, const void *right)
{
    return *(const int *)left - *(const int *)right;
}

static int
dag_value_live(match_dag *d, int target, int value)
{
    if (target < 0) {
        return 0;
    }
    dag_stack *live = &d->blocks[target].live;
    return live->size && bsearch(&value, live->values, live->size,
                                sizeof(int), dag_compare_values) != NULL;
}

static void
dag_live_add(match_dag *d, dag_stack *live, unsigned char *seen, int value)
{
    if (value >= 0 && !d->constants[value] && !seen[value]) {
        seen[value] = 1;
        live->values[live->size++] = value;
    }
}

static void
dag_inputs(match_dag *d, dag_operation *op, dag_stack *live, unsigned char *seen)
{
    dag_live_add(d, live, seen, op->a);
    dag_live_add(d, live, seen, op->b);
    if (op->kind == DAG_TUPLE) {
        for (int i = 0; i < PyTuple_GET_SIZE(op->data); i++) {
            dag_live_add(d, live, seen, dag_tuple_value(d, op, i));
        }
    }
    if (op->kind == DAG_COMMIT) {
        PyObject *stores = PyTuple_GET_ITEM(d->stores, op->arg);
        for (int i = 0; i < PyList_GET_SIZE(stores); i++) {
            int value = dag_capture(d, PyList_GET_ITEM(stores, i));
            dag_live_add(d, live, seen, value);
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
            /* The matching UNPACK bounds the number of keys. */
            assert(op->arg <= DAG_MAX_WIDTH);
            PyObject *items[DAG_MAX_WIDTH];
            int j = 0;
            for (; j < op->arg; j++) {
                items[j] = d->constants[dag_tuple_value(d, op, j)];
                if (items[j] == NULL) {
                    break;
                }
            }
            if (j == op->arg) {
                d->constants[op->output] = PyTuple_FromArray(items, op->arg);
                if (d->constants[op->output] == NULL) {
                    return ERROR;
                }
            }
        }
    }
    d->blocks = PyMem_Calloc(d->nnodes, sizeof(dag_block));
    unsigned char *seen = PyMem_Calloc(nv, 1);
    dag_stack work = {.values = PyMem_Malloc(nv * sizeof(int))};
    int result = ERROR;
    if (!d->blocks || !seen || !work.values) {
        PyErr_NoMemory();
        goto done;
    }
    d->blocks[root].pending = 1;  /* The entry edge supplies the subject. */
    for (int i = root; i >= 0; i--) {
        if (d->blocks[i].pending == 0) {
            continue;
        }
        dag_node node = DAG_NODE(d, i);
        int targets[2] = {node.yes, node.no};
        for (int edge = 0; edge < 2; edge++) {
            if (targets[edge] >= 0) {
                dag_block *target = &d->blocks[targets[edge]];
                target->pending++;
                target->single_predecessor = target->pending == 1;
            }
        }
    }
    for (int i = 0; i < d->nnodes; i++) {
        if (d->blocks[i].pending == 0) {
            continue;
        }
        dag_node node = DAG_NODE(d, i);
        dag_operation *op = &DAG_OP(d, node.op);
        work.size = 0;
        int targets[2] = {node.yes, node.no};
        for (int edge = 0; edge < 2; edge++) {
            if (targets[edge] < 0) {
                continue;
            }
            dag_stack *next = &d->blocks[targets[edge]].live;
            for (int j = 0; j < next->size; j++) {
                int value = next->values[j];
                if ((op->kind == DAG_GUARD && value != 0) ||
                    (value >= op->output && value - op->output < op->count)) {
                    continue;
                }
                dag_live_add(d, &work, seen, value);
            }
        }
        dag_inputs(d, op, &work, seen);
        if (work.size > DAG_MAX_WIDTH) {
            result = 1;  /* Use ordinary code generation before emission starts. */
            goto done;
        }
        qsort(work.values, work.size, sizeof(int), dag_compare_values);
        dag_stack *live = &d->blocks[i].live;
        live->values = PyMem_Malloc(work.size * sizeof(int));
        if (live->values == NULL) {
            PyErr_NoMemory();
            goto done;
        }
        live->size = work.size;
        memcpy(live->values, work.values, work.size * sizeof(int));
        /* Clear only the IDs used by this block, not the whole value space. */
        for (int j = 0; j < work.size; j++) {
            seen[work.values[j]] = 0;
        }
    }
    result = SUCCESS;
done:
    PyMem_Free(seen);
    PyMem_Free(work.values);
    return result;
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
        PyTuple_SET_ITEM(d->stores, i, pc.stores);
        RETURN_IF_ERROR(result);
        for (int j = 0; j < PyList_GET_SIZE(pc.stores); j++) {
            RETURN_IF_ERROR(_PyCompile_DictAddObj(d->captures, PyList_GET_ITEM(pc.stores, j)));
        }
    }
    /* Reserve every capture ID before operations allocate their outputs. */
    d->nvalues = (int)PyDict_GET_SIZE(d->captures) + 1;
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
        if (PyList_GET_SIZE(PyTuple_GET_ITEM(d->stores, i))) {
            yes = dag_emit(d, DAG_COMMIT, -1, -1, i, NULL, NULL, 0, 0,
                           yes, -1, LOC(m->pattern));
        }
        yes = dag_pattern(d, m->pattern, 0, yes, tail);
        tail = dag_emit(d, DAG_ENTER, -1, -1, 0, NULL, NULL, 0, 0,
                        yes, -1, LOC(m->pattern));
        RETURN_IF_ERROR(tail);
    }
    return tail;
}

static int
dag_optimize(match_dag *d, int root)
{
    d->original = d->nnodes;
    d->length_first = PyMem_Malloc(d->nvalues * sizeof(int));
    if (d->length_first == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    for (int i = 0; i < d->nvalues; i++) {
        d->length_first[i] = d->original;
    }
    for (int i = 0; i < d->nops; i++) {
        if (DAG_OP(d, i).count > DAG_MAX_WIDTH) {
            return root;
        }
        DAG_OP(d, i).first = d->original;
    }
    bool shared = false;
    for (int i = 0; i < d->original; i++) {
        dag_operation *op = &DAG_OP(d, DAG_NODE(d, i).op);
        shared |= op->reusable && op->first < i;
        op->first = Py_MIN(op->first, i);
    }
    for (int i = 0; i < d->nops; i++) {
        dag_operation *op = &DAG_OP(d, i);
        if (op->kind == DAG_LENGTH_EQ || op->kind == DAG_LENGTH_GE) {
            shared |= d->length_first[op->a] < d->original;
            d->length_first[op->a] = Py_MIN(d->length_first[op->a], op->first);
        }
    }
    if (!shared) {
        return root;
    }
    PyObject *context = PyFrozenSet_New(NULL);
    if (context == NULL) {
        return ERROR;
    }
    int result = dag_specialize(d, root, context);
    Py_DECREF(context);
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
    d.stores = PyTuple_New(ncases);
    d.bodies = PyMem_Malloc(ncases * sizeof(jump_target_label));
    int result = ERROR;
    if (d.op_ids && d.node_ids && d.state_ids && d.captures && d.stores && d.bodies) {
        result = dag_build_patterns(&d, s);
        if (result >= 0) {
            result = dag_optimize(&d, result);
        }
        Py_CLEAR(d.op_ids);
        Py_CLEAR(d.node_ids);
        Py_CLEAR(d.state_ids);
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
    Py_XDECREF(d.stores);
    Py_XDECREF(d.op_ids);
    Py_XDECREF(d.node_ids);
    Py_XDECREF(d.state_ids);
    Py_XDECREF(d.captures);
    _Py_CArray_Fini(&d.operations);
    _Py_CArray_Fini(&d.nodes);
    PyMem_Free(d.bodies);
    PyMem_Free(d.length_first);
    if (d.blocks) {
        for (int i = 0; i < d.nnodes; i++) {
            PyMem_Free(d.blocks[i].entry.values);
            PyMem_Free(d.blocks[i].live.values);
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
#undef DAG_MAX_WIDTH
