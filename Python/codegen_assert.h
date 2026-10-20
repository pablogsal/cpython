/* Compiler support for opt-in assertion diagnostics. */

typedef struct {
    PyObject *records;
    PyObject *nodes;
    PyObject *node_ids;
    int parent;
} assertion_context;

static int codegen_jump_if(compiler *, location, expr_ty,
                           jump_target_label, int);

static int
assertion_node_id(assertion_context *ctx, expr_ty e)
{
    PyObject *key = PyLong_FromVoidPtr(e);
    if (key == NULL) {
        return -1;
    }
    PyObject *value = PyDict_GetItemWithError(ctx->node_ids, key);
    Py_DECREF(key);
    return value == NULL ? -1 : (int)PyLong_AsLong(value);
}

static int
codegen_assert_enter(compiler *c, expr_ty e)
{
    assertion_context *ctx = METADATA(c)->u_assert;
    if (ctx == NULL) {
        return SUCCESS;
    }
    int id = assertion_node_id(ctx, e);
    if (id < 0) {
        if (PyErr_Occurred()) {
            return ERROR;
        }
        const char *kind = "expression";
        switch (e->kind) {
            case BoolOp_kind: kind = e->v.BoolOp.op == And ? "and" : "or"; break;
            case UnaryOp_kind: kind = e->v.UnaryOp.op == Not ? "not" : "unary"; break;
            case IfExp_kind: kind = "conditional"; break;
            case Compare_kind: kind = "compare"; break;
            case Call_kind: kind = "call"; break;
            case Name_kind: kind = "name"; break;
            case Constant_kind: kind = "constant"; break;
            case Attribute_kind: kind = "attribute"; break;
            case Subscript_kind: kind = "subscript"; break;
            case BinOp_kind: kind = "binary"; break;
            default: break;
        }
        PyObject *text = _PyAST_ExprAsUnicode(e);
        PyObject *children = PyList_New(0);
        PyObject *key = PyLong_FromVoidPtr(e);
        PyObject *node = NULL;
        id = (int)PyList_GET_SIZE(ctx->nodes);
        if (text && children && key) {
            node = Py_BuildValue("(Os(iiii)O)", text, kind,
                                 e->lineno, e->col_offset,
                                 e->end_lineno, e->end_col_offset, children);
        }
        PyObject *index = PyLong_FromLong(id);
        int res = ERROR;
        if (node && index && PyList_Append(ctx->nodes, node) == 0 &&
            PyDict_SetItem(ctx->node_ids, key, index) == 0) {
            res = SUCCESS;
        }
        Py_XDECREF(text);
        Py_XDECREF(children);
        Py_XDECREF(key);
        Py_XDECREF(node);
        Py_XDECREF(index);
        RETURN_IF_ERROR(res);
    }
    if (ctx->parent >= 0 && ctx->parent != id) {
        PyObject *parent = PyList_GET_ITEM(ctx->nodes, ctx->parent);
        PyObject *children = PyTuple_GET_ITEM(parent, 3);
        PyObject *index = PyLong_FromLong(id);
        if (index == NULL) {
            return ERROR;
        }
        int contains = PySequence_Contains(children, index);
        int res = contains < 0 ? ERROR :
            contains ? SUCCESS : PyList_Append(children, index);
        Py_DECREF(index);
        RETURN_IF_ERROR(res);
    }
    ctx->parent = id;
    return SUCCESS;
}

static int
codegen_assert_capture(compiler *c, expr_ty e, const char *kind, int link)
{
    assertion_context *ctx = METADATA(c)->u_assert;
    if (ctx == NULL) {
        return SUCCESS;
    }
    if ((e->kind == Name_kind && e->v.Name.ctx != Load) ||
        (e->kind == Attribute_kind && e->v.Attribute.ctx != Load) ||
        (e->kind == Subscript_kind && e->v.Subscript.ctx != Load) ||
        (e->kind == List_kind && e->v.List.ctx != Load) ||
        (e->kind == Tuple_kind && e->v.Tuple.ctx != Load)) {
        return SUCCESS;
    }
    /* Comparison nodes use their last evaluated link as their value. */
    if (e->kind == Compare_kind && strcmp(kind, "value") == 0) {
        return SUCCESS;
    }
    PyObject *extra = NULL, *name = NULL, *record = NULL;
    int ret = ERROR;
    if (link >= 0) {
        static const char *operators[] = {
            "", "==", "!=", "<", "<=", ">", ">=", "is", "is not", "in", "not in"
        };
        expr_ty left = link == 0 ? e->v.Compare.left :
            asdl_seq_GET(e->v.Compare.comparators, link - 1);
        expr_ty right = asdl_seq_GET(e->v.Compare.comparators, link);
        const char *op = operators[asdl_seq_GET(e->v.Compare.ops, link)];
        int left_id = assertion_node_id(ctx, left);
        int right_id = assertion_node_id(ctx, right);
        if (left_id < 0 || right_id < 0) {
            goto done;
        }
        extra = Py_BuildValue("(sii)", op, left_id, right_id);
    }
    else {
        extra = Py_NewRef(Py_None);
    }
    if (extra == NULL) {
        goto done;
    }
    name = PyUnicode_FromFormat(".assertion.%zd", PyList_GET_SIZE(ctx->records));
    if (name == NULL) {
        goto done;
    }
    Py_ssize_t slot = _PyCompile_DictAddObj(METADATA(c)->u_varnames, name);
    if (slot < 0) {
        goto done;
    }
    if (METADATA(c)->u_internal == NULL) {
        METADATA(c)->u_internal = PySet_New(NULL);
        if (METADATA(c)->u_internal == NULL) {
            goto done;
        }
    }
    if (PySet_Add(METADATA(c)->u_internal, name) < 0) {
        goto done;
    }
    int node_id = assertion_node_id(ctx, e);
    if (node_id < 0) {
        goto done;
    }
    record = Py_BuildValue("(nsiO)", slot, kind, node_id, extra);
    if (record == NULL || PyList_Append(ctx->records, record) < 0) {
        goto done;
    }
    if (codegen_addop_i(INSTR_SEQUENCE(c), STORE_FAST, slot, NO_LOCATION) < 0 ||
        codegen_addop_i(INSTR_SEQUENCE(c), LOAD_FAST, slot, NO_LOCATION) < 0) {
        goto done;
    }
    ret = SUCCESS;
done:
    Py_XDECREF(extra);
    Py_XDECREF(name);
    Py_XDECREF(record);
    return ret;
}

static int
codegen_assert_clear(compiler *c, assertion_context *ctx)
{
    for (Py_ssize_t i = 0; i < PyList_GET_SIZE(ctx->records); i++) {
        PyObject *record = PyList_GET_ITEM(ctx->records, i);
        Py_ssize_t slot = PyLong_AsSsize_t(PyTuple_GET_ITEM(record, 0));
        RETURN_IF_ERROR(slot);
        ADDOP_I(c, NO_LOCATION, LOAD_FAST_AND_CLEAR, slot);
        ADDOP(c, NO_LOCATION, POP_TOP);
    }
    return SUCCESS;
}

static int
codegen_assert_details_body(compiler *c, stmt_ty s, assertion_context *ctx)
{
    NEW_JUMP_TARGET_LABEL(c, passed);
    NEW_JUMP_TARGET_LABEL(c, cleanup);
    NEW_JUMP_TARGET_LABEL(c, end);
    location loc = LOC(s);
    /* There are no return/break/continue statements inside an expression,
       but the handler must count against the compiler's block limit. */
    RETURN_IF_ERROR(_PyCompile_PushFBlock(c, loc, COMPILE_FBLOCK_TRY_EXCEPT,
                                         cleanup, NO_LABEL, NULL));
    ADDOP_JUMP(c, NO_LOCATION, SETUP_FINALLY, cleanup);
    METADATA(c)->u_assert = ctx;
    int res = codegen_jump_if(c, loc, s->v.Assert.test, passed, 1);
    METADATA(c)->u_assert = NULL;
    RETURN_IF_ERROR(res);

    ADDOP_I(c, loc, LOAD_COMMON_CONSTANT, CONSTANT_ASSERTIONERROR);
    if (s->v.Assert.msg) {
        VISIT(c, expr, s->v.Assert.msg);
    }
    else {
        ADDOP(c, NO_LOCATION, PUSH_NULL);
    }
    ADDOP_I(c, loc, CALL, 0);

    PyObject *records = PyList_AsTuple(ctx->records);
    PyObject *nodes = PyTuple_New(PyList_GET_SIZE(ctx->nodes));
    if (nodes != NULL) {
        for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(nodes); i++) {
            PyObject *node = PyList_GET_ITEM(ctx->nodes, i);
            PyObject *children = PyList_AsTuple(PyTuple_GET_ITEM(node, 3));
            PyObject *frozen = children == NULL ? NULL : PyTuple_Pack(
                4, PyTuple_GET_ITEM(node, 0), PyTuple_GET_ITEM(node, 1),
                PyTuple_GET_ITEM(node, 2), children);
            Py_XDECREF(children);
            if (frozen == NULL) {
                Py_CLEAR(nodes);
                break;
            }
            PyTuple_SET_ITEM(nodes, i, frozen);
        }
    }
    PyObject *plan = NULL;
    if (records && nodes) {
        plan = PyTuple_Pack(2, records, nodes);
    }
    Py_XDECREF(records);
    Py_XDECREF(nodes);
    if (plan == NULL) {
        return ERROR;
    }
    Py_ssize_t plan_index = _PyCompile_AddConst(c, plan);
    Py_DECREF(plan);
    RETURN_IF_ERROR(plan_index);
    ADDOP_I(c, NO_LOCATION, LOAD_CONST, plan_index);
    ADDOP_I(c, NO_LOCATION, CALL_INTRINSIC_2, INTRINSIC_ASSERT_DETAILS);
    ADDOP_I(c, LOC(s->v.Assert.test), RAISE_VARARGS, 1);

    USE_LABEL(c, passed);
    ADDOP(c, NO_LOCATION, POP_BLOCK);
    _PyCompile_PopFBlock(c, COMPILE_FBLOCK_TRY_EXCEPT, cleanup);
    RETURN_IF_ERROR(codegen_assert_clear(c, ctx));
    ADDOP_JUMP(c, NO_LOCATION, JUMP_NO_INTERRUPT, end);

    USE_LABEL(c, cleanup);
    RETURN_IF_ERROR(codegen_assert_clear(c, ctx));
    ADDOP_I(c, NO_LOCATION, RERAISE, 0);
    USE_LABEL(c, end);
    return SUCCESS;
}

static int
codegen_assert_details(compiler *c, stmt_ty s)
{
    if (s->v.Assert.test->kind == Constant_kind &&
        PyObject_IsTrue(s->v.Assert.test->v.Constant.value) == 1) {
        ADDOP(c, LOC(s), NOP);
        return SUCCESS;
    }
    assertion_context ctx = {
        .records = PyList_New(0),
        .nodes = PyList_New(0), .node_ids = PyDict_New(), .parent = -1,
    };
    int ret = ERROR;
    if (ctx.records && ctx.nodes && ctx.node_ids) {
        ret = codegen_assert_details_body(c, s, &ctx);
    }
    METADATA(c)->u_assert = NULL;
    Py_XDECREF(ctx.records);
    Py_XDECREF(ctx.nodes);
    Py_XDECREF(ctx.node_ids);
    return ret;
}
