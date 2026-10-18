/* Emit the ordered DAG with only live values on the operand stack.
 * The first incoming edge chooses each block's stack layout. Later edges
 * reconcile their values with that layout.
 */
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
    return dag_value_live(d, node.yes, value) || dag_value_live(d, node.no, value);
}

/* Failure edges retain the current value order. Success edges call this
 * after dead-value cleanup, which can change that order through SWAP. */
static int
stack_record_entry(match_dag *d, dag_stack *stack, int target)
{
    dag_stack *entry = &d->blocks[target].entry;
    if (entry->values != NULL) {
        return SUCCESS;
    }
    entry->values = PyMem_Malloc(stack->size * sizeof(int));
    if (entry->values == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    for (int i = 0; i < stack->size; i++) {
        int value = stack->values[i];
        if (value >= 0 && dag_value_live(d, target, value)) {
            entry->values[entry->size++] = value;
        }
    }
    return SUCCESS;
}

static int
stack_edge(match_dag *d, dag_stack *stack, int target, location loc)
{
    compiler *c = d->c;
    for (int i = stack->size; i--;) {
        int value = stack->values[i];
        if (value < 0 || !dag_value_live(d, target, value)) {
            RETURN_IF_ERROR(stack_swap(d, stack, i, loc));
            ADDOP(c, loc, POP_TOP);
            stack->size--;
        }
    }
    RETURN_IF_ERROR(stack_record_entry(d, stack, target));
    dag_stack *entry = &d->blocks[target].entry;
    assert(entry->size == stack->size);
    for (int dest = 0; dest < stack->size; dest++) {
        int value = entry->values[dest];
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
    }
    ADDOP_JUMP(c, NO_LOCATION, JUMP, d->blocks[target].label);
    return SUCCESS;
}

static int
stack_queue_failure(match_dag *d, dag_stack *stack, int target,
                    jump_target_label label, int pop_none, location loc)
{
    dag_failure *failure = PyMem_Malloc(sizeof(*failure) + stack->size * sizeof(int));
    if (failure == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    *failure = (dag_failure){.next = d->blocks[target].failures,
        .label = label, .loc = loc, .size = stack->size, .pop_none = pop_none};
    memcpy(failure->values, stack->values, stack->size * sizeof(int));
    d->blocks[target].failures = failure;
    return stack_record_entry(d, stack, target);
}

static int
stack_codegen_node(match_dag *d, stmt_ty s, int index, int *workspace)
{
    compiler *c = d->c;
    dag_node node = DAG_NODE(d, index);
    dag_operation op = DAG_OP(d, node.op);
    location loc = node.loc;
    dag_stack stack = {.values = workspace};
    dag_stack *entry = &d->blocks[index].entry;
    assert(entry->values != NULL);
    stack.size = entry->size;
    memcpy(stack.values, entry->values, stack.size * sizeof(int));
    USE_LABEL(c, d->blocks[index].label);
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
        NEW_JUMP_TARGET_LABEL(c, fail);
        match_case_ty m = asdl_seq_GET(s->v.Match.cases, op.arg);
        RETURN_IF_ERROR(codegen_jump_if(c, loc, m->guard, fail, 0));
        RETURN_IF_ERROR(stack_queue_failure(d, &stack, node.no, fail, 0, loc));
        return stack_edge(d, &stack, node.yes, loc);
    }
    int inline_equal = -1;
    if (op.kind == DAG_ATTRIBUTE && d->blocks[node.yes].single_predecessor) {
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
            d->blocks[inline_equal].inlined = true;
        }
    }
    if (op.kind == DAG_BIND &&
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
            int value = (int)PyLong_AsLong(PyDict_GetItem(d->captures, name)) + 1;
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
        int pair_ready = op.a >= 0 && op.b >= 0 &&
            stack.size >= 2 &&
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
        if (op.b >= 0 && !pair_ready) {
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
                    int value = dag_tuple_value(d, &op, i);
                    int keep = stack_live_after(d, node, value);
                    for (int j = i + 1; j < op.arg; j++) {
                        keep |= value == dag_tuple_value(d, &op, j);
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
    int pop_none = op.kind == DAG_CLASS || op.kind == DAG_KEYS;
    if (node.no >= 0) {
        if (pop_none) {
            ADDOP_I(c, loc, COPY, 1);
            ADDOP_JUMP(c, loc, POP_JUMP_IF_NONE, fail);
        }
        else {
            ADDOP_JUMP(c, loc, POP_JUMP_IF_FALSE, fail);
        }
        RETURN_IF_ERROR(stack_queue_failure(d, &stack, node.no, fail, pop_none,
                                           dag_destination_location(d, s, node.no)));
    }
    for (int i = op.count; i--;) {
        stack.values[stack.size++] = op.output + i;
    }
    return stack_edge(d, &stack, node.yes,
                      dag_destination_location(d, s, node.yes));
}

static int
stack_emit_failure(match_dag *d, dag_failure *failure, int target)
{
    compiler *c = d->c;
    dag_stack stack = {failure->values, failure->size};
    USE_LABEL(c, failure->label);
    if (failure->pop_none) {
        ADDOP(c, failure->loc, POP_TOP);
    }
    return stack_edge(d, &stack, target, failure->loc);
}

/* A topological order keeps all graph edges forward. Prefer a success edge
 * when its other predecessors are ready. Place cleanup before its destination,
 * so failure paths do not need backward jumps or extra trace events. */
static int
stack_codegen_graph(match_dag *d, stmt_ty s, int root, int *workspace)
{
    int *pending = PyMem_Malloc(d->nnodes * sizeof(int));
    if (pending == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    int count = 1, result = SUCCESS;
    pending[0] = root;
    while (count) {
        int index = pending[--count];
        dag_block *block = &d->blocks[index];
        while (block->failures) {
            dag_failure *failure = block->failures;
            block->failures = failure->next;
            result = stack_emit_failure(d, failure, index);
            PyMem_Free(failure);
            if (result < 0) {
                break;
            }
        }
        if (result < 0) {
            break;
        }
        if (!block->inlined) {
            result = stack_codegen_node(d, s, index, workspace);
            if (result < 0) {
                break;
            }
        }
        dag_node node = DAG_NODE(d, index);
        if (node.no >= 0 && --d->blocks[node.no].pending == 0) {
            pending[count++] = node.no;
        }
        if (node.yes >= 0 && --d->blocks[node.yes].pending == 0) {
            pending[count++] = node.yes;
        }
    }
    PyMem_Free(pending);
    return result;
}

static int
dag_emit_match(match_dag *d, stmt_ty s, int root)
{
    compiler *c = d->c;
    int ncases = asdl_seq_LEN(s->v.Match.cases);
    RETURN_IF_ERROR(dag_analyze_values(d, root));
    for (int i = 0; i < d->nnodes; i++) {
        d->blocks[i].label = _PyInstructionSequence_NewLabel(INSTR_SEQUENCE(c));
    }
    for (int i = 0; i < ncases; i++) {
        d->bodies[i] = _PyInstructionSequence_NewLabel(INSTR_SEQUENCE(c));
    }
    d->end = _PyInstructionSequence_NewLabel(INSTR_SEQUENCE(c));
    dag_stack *entry = &d->blocks[root].entry;
    entry->values = PyMem_Calloc(1, sizeof(int));
    if (entry->values == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    entry->size = 1;
    VISIT(c, expr, s->v.Match.subject);
    ADDOP_JUMP(c, NO_LOCATION, JUMP, d->blocks[root].label);
    /* Mapping keys also reserve one unpacked value per key. */
    int capacity = 2 * d->nvalues + 4;
    int *workspace = PyMem_Malloc(capacity * sizeof(int));
    if (workspace == NULL) {
        PyErr_NoMemory();
        return ERROR;
    }
    int result = stack_codegen_graph(d, s, root, workspace);
    PyMem_Free(workspace);
    RETURN_IF_ERROR(result);
    for (int i = 0; i < ncases; i++) {
        match_case_ty m = asdl_seq_GET(s->v.Match.cases, i);
        USE_LABEL(c, d->bodies[i]);
        VISIT_SEQ(c, stmt, m->body);
        ADDOP_JUMP(c, NO_LOCATION, JUMP, d->end);
    }
    USE_LABEL(c, d->end);
    return SUCCESS;
}
