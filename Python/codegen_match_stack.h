/* Experimental sparse operand-stack placement for the same ordered DAG.
 * Node entries contain only live values, in virtual-value order. Edges drop
 * dead values and reconcile permutations. This deliberately shares the graph
 * and liveness calculation with the fixed-slot experiment for comparison.
 */
typedef struct {
    int *values;
    int size;
} dag_stack;

static int
stack_swap(match_dag *d, dag_stack *stack, int index, location loc)
{
    if (index != stack->size - 1) {
        compiler *c = d->c;
        ADDOP_I(c, loc, SWAP, stack->size - index);
        int value = stack->values[index];
        stack->values[index] = stack->values[stack->size - 1];
        stack->values[stack->size - 1] = value;
    }
    return SUCCESS;
}

static int
stack_use(match_dag *d, dag_stack *stack, int value, int keep, location loc)
{
    compiler *c = d->c;
    if (d->constants[value]) {
        ADDOP_LOAD_CONST(c, loc, d->constants[value]);
        stack->values[stack->size++] = -1;
        return SUCCESS;
    }
    int index = stack->size - 1;
    while (index >= 0 && stack->values[index] != value) {
        index--;
    }
    if (index < 0) {
        PyErr_Format(PyExc_SystemError, "missing DAG value %d", value);
        return ERROR;
    }
    /* Do not move a resident through operands already prepared for this op. */
    if (keep || (index != stack->size - 1 && stack->values[stack->size - 1] < 0)) {
        ADDOP_I(c, loc, COPY, stack->size - index);
        stack->values[stack->size++] = -1;
    }
    else {
        RETURN_IF_ERROR(stack_swap(d, stack, index, loc));
        stack->values[stack->size - 1] = -1;
    }
    return SUCCESS;
}

static int
stack_live_after(match_dag *d, dag_node node, int value)
{
    return (node.yes >= 0 && d->values_live[(size_t)node.yes * d->nvalues + value]) ||
           (node.no >= 0 && d->values_live[(size_t)node.no * d->nvalues + value]);
}

static int
stack_edge(match_dag *d, dag_stack *stack, int target, location loc)
{
    compiler *c = d->c;
    unsigned char *live = d->values_live + (size_t)target * d->nvalues;
    for (int i = stack->size; i--;) {
        int value = stack->values[i];
        if (value < 0 || !live[value]) {
            RETURN_IF_ERROR(stack_swap(d, stack, i, loc));
            ADDOP(c, loc, POP_TOP);
            stack->size--;
        }
    }
    int *layout = NULL;
    if (d->entry_layouts) {
        layout = d->entry_layouts[target];
        if (layout == NULL) {
            layout = PyMem_Malloc((stack->size + 1) * sizeof(int));
            if (layout == NULL) {
                PyErr_NoMemory();
                return ERROR;
            }
            layout[0] = stack->size;
            memcpy(layout + 1, stack->values, stack->size * sizeof(int));
            d->entry_layouts[target] = layout;
        }
        assert(layout[0] == stack->size);
    }
    int dest = 0;
    for (int v = 0; v < (layout ? layout[0] : d->nvalues); v++) {
        int value = layout ? layout[v + 1] : v;
        if (!live[value]) {
            continue;
        }
        int source = dest;
        while (source < stack->size && stack->values[source] != value) {
            source++;
        }
        if (source == stack->size) {
            PyErr_Format(PyExc_SystemError, "missing DAG edge value %d", value);
            return ERROR;
        }
        if (source != dest) {
            RETURN_IF_ERROR(stack_swap(d, stack, source, loc));
            RETURN_IF_ERROR(stack_swap(d, stack, dest, loc));
            if (source != stack->size - 1) {
                RETURN_IF_ERROR(stack_swap(d, stack, source, loc));
            }
        }
        dest++;
    }
    assert(dest == stack->size);
    ADDOP_JUMP(c, NO_LOCATION, JUMP, d->labels[target]);
    return SUCCESS;
}

static int
stack_codegen_node(match_dag *d, stmt_ty s, int index, int *workspace, int capacity)
{
    compiler *c = d->c;
    dag_node node = DAG_NODE(d, index);
    dag_operation op = DAG_OP(d, node.op);
    location loc = node.loc;
    dag_stack stack = {.values = workspace};
    dag_stack failed = {.values = workspace + capacity};
    unsigned char *live = d->values_live + (size_t)index * d->nvalues;
    if (d->entry_layouts) {
        int *layout = d->entry_layouts[index];
        assert(layout != NULL);
        stack.size = layout[0];
        memcpy(stack.values, layout + 1, stack.size * sizeof(int));
    }
    else {
        for (int value = 0; value < d->nvalues; value++) {
            if (live[value]) {
                stack.values[stack.size++] = value;
            }
        }
    }
    USE_LABEL(c, d->labels[index]);
    if ((op.kind == DAG_LOAD || op.kind == DAG_TUPLE) && d->constants[op.output]) {
        return stack_edge(d, &stack, node.yes, loc);
    }
    if (op.kind == DAG_ACCEPT || op.kind == DAG_END) {
        assert(stack.size == 0);
        ADDOP_JUMP(c, NO_LOCATION, JUMP,
                   op.kind == DAG_END ? d->end : d->bodies[op.arg]);
        return SUCCESS;
    }
    if (op.kind == DAG_GUARD) {
        /* Liveness removes every cached value before executing the guard. */
        assert(stack.size <= 1 && (!stack.size || stack.values[0] == 0));
        failed.size = stack.size;
        memcpy(failed.values, stack.values, stack.size * sizeof(int));
        NEW_JUMP_TARGET_LABEL(c, fail);
        match_case_ty m = asdl_seq_GET(s->v.Match.cases, op.arg);
        RETURN_IF_ERROR(codegen_jump_if(c, loc, m->guard, fail, 0));
        RETURN_IF_ERROR(stack_edge(d, &stack, node.yes, loc));
        USE_LABEL(c, fail);
        return stack_edge(d, &failed, node.no, loc);
    }
    int inline_equal = -1;
    if (d->ordered_stack && op.kind == DAG_ATTRIBUTE && d->predecessors[node.yes] == 1) {
        dag_node next = DAG_NODE(d, node.yes);
        dag_operation comparison = DAG_OP(d, next.op);
        if (comparison.kind == DAG_EQUAL && comparison.b == op.output &&
            !stack_live_after(d, next, op.output)) {
            /* Prepare the other operand before the single-use attribute read.
             * The existing order is already ideal when both operands die. */
            int ready = stack.size >= 2 &&
                stack.values[stack.size - 2] == comparison.a &&
                stack.values[stack.size - 1] == op.a &&
                !stack_live_after(d, next, comparison.a) &&
                !stack_live_after(d, next, op.a);
            if (!ready) {
                RETURN_IF_ERROR(stack_use(d, &stack, comparison.a,
                    stack_live_after(d, next, comparison.a) || comparison.a == op.a, loc));
            }
            inline_equal = node.yes;
            d->inlined[inline_equal] = 1;
        }
    }
    if (d->ordered_stack && op.kind == DAG_BIND &&
        !d->constants[op.a] && !stack_live_after(d, node, op.a)) {
        for (int i = 0; i < stack.size; i++) {
            if (stack.values[i] == op.a) {
                stack.values[i] = op.output;
                return stack_edge(d, &stack, node.yes, loc);
            }
        }
        Py_UNREACHABLE();
    }
    if (op.kind == DAG_COMMIT) {
        PyObject *stores = d->stores[op.arg];
        for (int i = 0; i < PyList_GET_SIZE(stores); i++) {
            PyObject *name = PyList_GET_ITEM(stores, i);
            int value = (int)PyLong_AsLong(PyDict_GetItem(d->captures, name));
            RETURN_IF_ERROR(stack_use(d, &stack, value,
                                      stack_live_after(d, node, value), loc));
            RETURN_IF_ERROR(codegen_nameop(c, loc, name, Store));
            stack.size--;
        }
    }
    else if (op.kind == DAG_ENTER) {
        ADDOP(c, loc, NOP);
    }
    else if (op.kind == DAG_SEQUENCE || op.kind == DAG_MAPPING || op.kind == DAG_LENGTH) {
        /* These instructions already retain their input. */
        RETURN_IF_ERROR(stack_use(d, &stack, op.a, 0, loc));
        stack.values[stack.size - 1] = op.a;
        ADDOP(c, loc, op.kind == DAG_SEQUENCE ? MATCH_SEQUENCE :
              op.kind == DAG_MAPPING ? MATCH_MAPPING : GET_LEN);
    }
    else {
        int inputs = 0;
        int pair_ready = d->ordered_stack && op.a >= 0 && op.b >= 0 &&
            (op.kind != DAG_INDEX || op.arg < 0) && stack.size >= 2 &&
            stack.values[stack.size - 2] == op.a && stack.values[stack.size - 1] == op.b &&
            !stack_live_after(d, node, op.a) && !stack_live_after(d, node, op.b);
        if (pair_ready) {
            inputs = 2;
        }
        if (op.a >= 0 && !pair_ready) {
            RETURN_IF_ERROR(stack_use(d, &stack, op.a,
                                      stack_live_after(d, node, op.a) || op.a == op.b, loc));
            inputs++;
        }
        if (op.b >= 0 && op.kind != DAG_INDEX && !pair_ready) {
            RETURN_IF_ERROR(stack_use(d, &stack, op.b,
                                      stack_live_after(d, node, op.b), loc));
            inputs++;
        }
        switch (op.kind) {
            case DAG_BIND:
                break;
            case DAG_LOAD: {
                int start = INSTR_SEQUENCE(c)->s_used;
                VISIT(c, expr, op.expr);
                for (int i = start; i < INSTR_SEQUENCE(c)->s_used; i++) {
                    INSTR_SEQUENCE(c)->s_instrs[i].i_loc = loc;
                }
                break;
            }
            case DAG_ATTRIBUTE:
                ADDOP_NAME(c, loc, LOAD_ATTR, op.data, names);
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
                    if (!pair_ready) {
                        RETURN_IF_ERROR(stack_use(d, &stack, op.b,
                                                  stack_live_after(d, node, op.b), loc));
                        inputs++;
                    }
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
                    int keep = stack_live_after(d, node, value);
                    for (int j = i + 1; j < op.arg; j++) {
                        keep |= value == PyLong_AsLong(PyTuple_GET_ITEM(op.data, j));
                    }
                    RETURN_IF_ERROR(stack_use(d, &stack, value, keep, loc));
                    inputs++;
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
        stack.size -= inputs;
    }
    if (inline_equal >= 0) {
        node = DAG_NODE(d, inline_equal);
        op = DAG_OP(d, node.op);
        loc = node.loc;
        ADDOP_COMPARE(c, loc, Eq);
        ADDOP(c, loc, TO_BOOL);
        stack.size--;
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
        failed.size = stack.size;
        memcpy(failed.values, stack.values, stack.size * sizeof(int));
    }
    for (int i = op.count; i--;) {
        stack.values[stack.size++] = op.output + i;
    }
    RETURN_IF_ERROR(stack_edge(d, &stack, node.yes,
                              dag_destination_location(d, s, node.yes)));
    if (node.no >= 0) {
        USE_LABEL(c, fail);
        loc = dag_destination_location(d, s, node.no);
        if (optional) {
            ADDOP(c, loc, POP_TOP);
        }
        RETURN_IF_ERROR(stack_edge(d, &failed, node.no, loc));
    }
    return SUCCESS;
}
