#include "Python.h"
#include "pycore_ast.h"
#include "pycore_pystate.h"
#include "pycore_runtime.h"
#include "internal/pycore_ast.h"
#include <float.h>
#include <stdbool.h>

#define POSITION(n) (n)->lineno, (n)->col_offset, (n)->end_lineno, (n)->end_col_offset
#define ARENA(r) (r)->arena

// Helper to create a Python object and register it with the arena
static PyObject* arena_pyobject(PyArena *arena, PyObject *obj) {
    if (obj == NULL) {
        return NULL;
    }
    if (_PyArena_AddPyObject(arena, obj) < 0) {
        Py_DECREF(obj);
        return NULL;
    }
    return obj;
}

// Result of visiting an expression: the expr itself and its explanation string
typedef struct {
    expr_ty expr;
    PyObject *expl;  // Python string object for the explanation
} VisitResult;

// Stack entry for format context
typedef struct FormatContext {
    PyObject *specifiers;  // dict mapping specifier names to expressions
    struct FormatContext *next;
} FormatContext;

typedef struct {
    PyArena *arena;
    int variable_counter;
    asdl_stmt_seq *statements;      // Statements being built
    PyObject *variables;             // List of variable names (Python list)
    asdl_stmt_seq *expl_stmts;      // Explanation statements
    FormatContext *stack;            // Stack of format contexts
    PyObject *explanation_specifiers; // Current format context dict
    expr_ty assert_node;             // Original assert for position info
} PyAssertRewriter;

// Forward declarations
static VisitResult visit_expr(PyAssertRewriter *rewriter, expr_ty expr);
static int init_rewriter(PyAssertRewriter *rewriter, PyArena *arena, expr_ty assert_node);
static void free_rewriter(PyAssertRewriter *rewriter);
static PyObject* variable(PyAssertRewriter *rewriter);
static expr_ty assign(PyAssertRewriter *rewriter, expr_ty expr);
static expr_ty display(PyAssertRewriter *rewriter, expr_ty expr);
static expr_ty helper(PyAssertRewriter *rewriter, const char *name, int nargs, ...);
static expr_ty builtin_attr(PyAssertRewriter *rewriter, const char *name);
static PyObject* explanation_param(PyAssertRewriter *rewriter, expr_ty expr);
static void push_format_context(PyAssertRewriter *rewriter);
static expr_ty pop_format_context(PyAssertRewriter *rewriter, expr_ty expl_expr);

// Helper to get cmpop as string
static const char* cmpop_str(cmpop_ty op) {
    switch (op) {
        case Eq: return "==";
        case NotEq: return "!=";
        case Lt: return "<";
        case LtE: return "<=";
        case Gt: return ">";
        case GtE: return ">=";
        case Is: return "is";
        case IsNot: return "is not";
        case In: return "in";
        case NotIn: return "not in";
        default: return "?";
    }
}

// Helper to get operator as string
static const char* binop_str(operator_ty op) {
    switch (op) {
        case Add: return "+";
        case Sub: return "-";
        case Mult: return "*";
        case Div: return "/";
        case FloorDiv: return "//";
        case Mod: return "%%";  // escaped for % formatting
        case Pow: return "**";
        case LShift: return "<<";
        case RShift: return ">>";
        case BitOr: return "|";
        case BitXor: return "^";
        case BitAnd: return "&";
        case MatMult: return "@";
        default: return "?";
    }
}

// Helper to get unary op format string
static const char* unaryop_str(unaryop_ty op) {
    switch (op) {
        case Not: return "not %s";
        case Invert: return "~%s";
        case USub: return "-%s";
        case UAdd: return "+%s";
        default: return "%s";
    }
}

// Initialize the rewriter
static int init_rewriter(PyAssertRewriter *rewriter, PyArena *arena, expr_ty assert_node) {
    rewriter->arena = arena;
    rewriter->variable_counter = 0;
    rewriter->statements = _Py_asdl_stmt_seq_new(0, arena);
    rewriter->expl_stmts = _Py_asdl_stmt_seq_new(0, arena);
    rewriter->stack = NULL;
    rewriter->assert_node = assert_node;

    // Create Python list for variables
    rewriter->variables = PyList_New(0);
    if (rewriter->variables == NULL) {
        return -1;
    }

    // Create empty dict for explanation_specifiers
    rewriter->explanation_specifiers = PyDict_New();
    if (rewriter->explanation_specifiers == NULL) {
        Py_DECREF(rewriter->variables);
        return -1;
    }

    return 0;
}

// Free rewriter resources
static void free_rewriter(PyAssertRewriter *rewriter) {
    Py_XDECREF(rewriter->variables);
    Py_XDECREF(rewriter->explanation_specifiers);

    // Free format context stack
    FormatContext *ctx = rewriter->stack;
    while (ctx != NULL) {
        FormatContext *next = ctx->next;
        Py_XDECREF(ctx->specifiers);
        PyMem_Free(ctx);
        ctx = next;
    }
}

// Get a new variable name
static PyObject* variable(PyAssertRewriter *rewriter) {
    PyObject *name = arena_pyobject(ARENA(rewriter), PyUnicode_FromFormat("@py_assert%d", rewriter->variable_counter++));
    if (name == NULL) {
        return NULL;
    }
    if (PyList_Append(rewriter->variables, name) < 0) {
        return NULL;
    }
    return name;
}

// Assign expression to a new variable
static expr_ty assign(PyAssertRewriter *rewriter, expr_ty expr) {
    PyObject *name_str = variable(rewriter);
    if (name_str == NULL) {
        return NULL;
    }

    identifier name = name_str;

    // Create: name = expr
    asdl_expr_seq *targets = _Py_asdl_expr_seq_new(1, ARENA(rewriter));
    if (targets == NULL) {
        return NULL;
    }

    expr_ty store_name = _PyAST_Name(name, Store, POSITION(rewriter->assert_node), ARENA(rewriter));
    asdl_seq_SET(targets, 0, store_name);

    stmt_ty assign_stmt = _PyAST_Assign(targets, expr, NULL,
                                        POSITION(rewriter->assert_node), ARENA(rewriter));

    // Append to statements
    asdl_stmt_seq *new_stmts = _Py_asdl_stmt_seq_new(
        asdl_seq_LEN(rewriter->statements) + 1, ARENA(rewriter));
    if (new_stmts == NULL) {
        return NULL;
    }

    for (Py_ssize_t i = 0; i < asdl_seq_LEN(rewriter->statements); i++) {
        asdl_seq_SET(new_stmts, i, asdl_seq_GET(rewriter->statements, i));
    }
    asdl_seq_SET(new_stmts, asdl_seq_LEN(rewriter->statements), assign_stmt);
    rewriter->statements = new_stmts;

    // Return Name node for loading the variable
    return _PyAST_Name(name, Load, POSITION(rewriter->assert_node), ARENA(rewriter));
}

// Call saferepr on the expression
static expr_ty display(PyAssertRewriter *rewriter, expr_ty expr) {
    return helper(rewriter, "_saferepr", 1, expr);
}

// Call a helper function from @sys (_ast_rewrite module)
static expr_ty helper(PyAssertRewriter *rewriter, const char *name, int nargs, ...) {
    // @sys = Name("@sys", Load)
    identifier sys_name = arena_pyobject(ARENA(rewriter), PyUnicode_FromString("@sys"));
    expr_ty py_name = _PyAST_Name(sys_name, Load, POSITION(rewriter->assert_node), ARENA(rewriter));

    // @sys.name
    identifier attr_name = arena_pyobject(ARENA(rewriter), PyUnicode_FromString(name));
    expr_ty attr = _PyAST_Attribute(py_name, attr_name, Load,
                                    POSITION(rewriter->assert_node), ARENA(rewriter));

    // Build arguments
    asdl_expr_seq *args = _Py_asdl_expr_seq_new(nargs, ARENA(rewriter));
    if (args == NULL) {
        return NULL;
    }

    va_list ap;
    va_start(ap, nargs);
    for (int i = 0; i < nargs; i++) {
        expr_ty arg = va_arg(ap, expr_ty);
        asdl_seq_SET(args, i, arg);
    }
    va_end(ap);

    return _PyAST_Call(attr, args, NULL, POSITION(rewriter->assert_node), ARENA(rewriter));
}

// Return @py_builtins.name
static expr_ty builtin_attr(PyAssertRewriter *rewriter, const char *name) {
    identifier builtin_name = arena_pyobject(ARENA(rewriter), PyUnicode_FromString("@py_builtins"));
    expr_ty builtin_obj = _PyAST_Name(builtin_name, Load,
                                      POSITION(rewriter->assert_node), ARENA(rewriter));

    identifier attr_name = arena_pyobject(ARENA(rewriter), PyUnicode_FromString(name));
    return _PyAST_Attribute(builtin_obj, attr_name, Load,
                           POSITION(rewriter->assert_node), ARENA(rewriter));
}

// Create a new formatting placeholder and return it
static PyObject* explanation_param(PyAssertRewriter *rewriter, expr_ty expr) {
    // Don't use arena_pyobject here - specifier is temporary and used as dict key
    PyObject *specifier = PyUnicode_FromFormat("py%d", rewriter->variable_counter++);
    if (specifier == NULL) {
        return NULL;
    }

    // Wrap expr_ty in a PyCapsule so we can store it in a Python dict
    PyObject *expr_capsule = PyCapsule_New((void*)expr, "expr_ty", NULL);
    if (expr_capsule == NULL) {
        Py_DECREF(specifier);
        return NULL;
    }

    // Store in current format context
    if (PyDict_SetItem(rewriter->explanation_specifiers, specifier, expr_capsule) < 0) {
        Py_DECREF(specifier);
        Py_DECREF(expr_capsule);
        return NULL;
    }

    Py_DECREF(expr_capsule);

    // Return "%(pyN)s" - this is the explanation string, doesn't need arena
    PyObject *result = PyUnicode_FromFormat("%%(%S)s", specifier);
    Py_DECREF(specifier);
    return result;
}

// Push a new format context
static void push_format_context(PyAssertRewriter *rewriter) {
    FormatContext *new_ctx = PyMem_Malloc(sizeof(FormatContext));
    if (new_ctx == NULL) {
        PyErr_NoMemory();
        return;
    }

    new_ctx->specifiers = PyDict_New();
    new_ctx->next = rewriter->stack;
    rewriter->stack = new_ctx;

    // Update current specifiers
    Py_XDECREF(rewriter->explanation_specifiers);
    rewriter->explanation_specifiers = new_ctx->specifiers;
    Py_INCREF(rewriter->explanation_specifiers);
}

// Pop format context and create formatted string
static expr_ty pop_format_context(PyAssertRewriter *rewriter, expr_ty expl_expr) {
    if (rewriter->stack == NULL) {
        return NULL;
    }

    FormatContext *current = rewriter->stack;
    rewriter->stack = current->next;

    // Restore previous context
    if (rewriter->stack != NULL) {
        Py_XDECREF(rewriter->explanation_specifiers);
        rewriter->explanation_specifiers = rewriter->stack->specifiers;
        Py_INCREF(rewriter->explanation_specifiers);
    }

    // Build format dict: {key1: val1, key2: val2, ...}
    PyObject *keys_list = PyDict_Keys(current->specifiers);
    PyObject *values_list = PyDict_Values(current->specifiers);

    if (!keys_list || !values_list) {
        Py_XDECREF(keys_list);
        Py_XDECREF(values_list);
        return NULL;
    }

    Py_ssize_t dict_size = PyList_Size(keys_list);
    asdl_expr_seq *keys = _Py_asdl_expr_seq_new(dict_size, ARENA(rewriter));
    asdl_expr_seq *values = _Py_asdl_expr_seq_new(dict_size, ARENA(rewriter));

    if (!keys || !values) {
        Py_DECREF(keys_list);
        Py_DECREF(values_list);
        return NULL;
    }

    for (Py_ssize_t i = 0; i < dict_size; i++) {
        PyObject *key = PyList_GetItem(keys_list, i);
        // PyList_GetItem returns borrowed reference, need to INCREF and add to arena
        Py_INCREF(key);
        if (_PyArena_AddPyObject(ARENA(rewriter), key) < 0) {
            Py_DECREF(key);
            Py_DECREF(keys_list);
            Py_DECREF(values_list);
            return NULL;
        }
        expr_ty key_expr = _PyAST_Constant(key, NULL, POSITION(rewriter->assert_node), ARENA(rewriter));
        if (!key_expr) {
            Py_DECREF(keys_list);
            Py_DECREF(values_list);
            return NULL;
        }
        asdl_seq_SET(keys, i, key_expr);

        PyObject *val_capsule = PyList_GetItem(values_list, i);
        // Extract expr_ty from capsule
        expr_ty val_expr = (expr_ty)PyCapsule_GetPointer(val_capsule, "expr_ty");
        if (!val_expr) {
            Py_DECREF(keys_list);
            Py_DECREF(values_list);
            return NULL;
        }
        asdl_seq_SET(values, i, val_expr);
    }

    Py_DECREF(keys_list);
    Py_DECREF(values_list);

    expr_ty format_dict = _PyAST_Dict(keys, values, POSITION(rewriter->assert_node), ARENA(rewriter));
    if (!format_dict) {
        return NULL;
    }

    // Create: expl_expr % format_dict
    expr_ty form = _PyAST_BinOp(expl_expr, Mod, format_dict,
                                POSITION(rewriter->assert_node), ARENA(rewriter));
    if (!form) {
        return NULL;
    }

    // Assign to @py_formatN
    PyObject *name_str = arena_pyobject(ARENA(rewriter),
                                        PyUnicode_FromFormat("@py_format%d", rewriter->variable_counter++));
    if (!name_str) {
        return NULL;
    }
    identifier name = name_str;

    asdl_expr_seq *targets = _Py_asdl_expr_seq_new(1, ARENA(rewriter));
    expr_ty store_name = _PyAST_Name(name, Store, POSITION(rewriter->assert_node), ARENA(rewriter));
    asdl_seq_SET(targets, 0, store_name);

    stmt_ty assign_stmt = _PyAST_Assign(targets, form, NULL,
                                        POSITION(rewriter->assert_node), ARENA(rewriter));

    // Append to expl_stmts
    asdl_stmt_seq *new_stmts = _Py_asdl_stmt_seq_new(
        asdl_seq_LEN(rewriter->expl_stmts) + 1, ARENA(rewriter));

    for (Py_ssize_t i = 0; i < asdl_seq_LEN(rewriter->expl_stmts); i++) {
        asdl_seq_SET(new_stmts, i, asdl_seq_GET(rewriter->expl_stmts, i));
    }
    asdl_seq_SET(new_stmts, asdl_seq_LEN(rewriter->expl_stmts), assign_stmt);
    rewriter->expl_stmts = new_stmts;

    // Clean up context
    Py_DECREF(current->specifiers);
    PyMem_Free(current);

    return _PyAST_Name(name, Load, POSITION(rewriter->assert_node), ARENA(rewriter));
}

// Visit Name expression
static VisitResult visit_Name(PyAssertRewriter *rewriter, expr_ty name) {
    VisitResult result;
    result.expr = name;

    // Build: 'name' in locals()
    expr_ty locs = _PyAST_Call(builtin_attr(rewriter, "locals"),
                               _Py_asdl_expr_seq_new(0, ARENA(rewriter)), NULL,
                               POSITION(rewriter->assert_node), ARENA(rewriter));

    // Name.id is already in the arena (it came from the parser), so we don't need to register it again
    expr_ty name_const = _PyAST_Constant(name->v.Name.id, NULL,
                                        POSITION(rewriter->assert_node), ARENA(rewriter));

    asdl_int_seq *ops = _Py_asdl_int_seq_new(1, ARENA(rewriter));
    asdl_seq_SET(ops, 0, In);
    asdl_expr_seq *comparators = _Py_asdl_expr_seq_new(1, ARENA(rewriter));
    asdl_seq_SET(comparators, 0, locs);

    expr_ty inlocs = _PyAST_Compare(name_const, ops, comparators,
                                    POSITION(rewriter->assert_node), ARENA(rewriter));

    // Build: _should_repr_global_name(name)
    asdl_expr_seq *args = _Py_asdl_expr_seq_new(1, ARENA(rewriter));
    asdl_seq_SET(args, 0, name);
    expr_ty dorepr = helper(rewriter, "_should_repr_global_name", 1, name);

    // Build: inlocs or dorepr
    asdl_expr_seq *or_values = _Py_asdl_expr_seq_new(2, ARENA(rewriter));
    asdl_seq_SET(or_values, 0, inlocs);
    asdl_seq_SET(or_values, 1, dorepr);
    expr_ty test = _PyAST_BoolOp(Or, or_values, POSITION(rewriter->assert_node), ARENA(rewriter));

    // Build: display(name) if test else name.id
    expr_ty if_expr = _PyAST_IfExp(test, display(rewriter, name), name_const,
                                  POSITION(rewriter->assert_node), ARENA(rewriter));

    result.expl = explanation_param(rewriter, if_expr);
    return result;
}

// Visit BinOp expression
static VisitResult visit_BinOp(PyAssertRewriter *rewriter, expr_ty binop) {
    VisitResult result = {0};

    const char *symbol = binop_str(binop->v.BinOp.op);
    VisitResult left_result = visit_expr(rewriter, binop->v.BinOp.left);
    VisitResult right_result = visit_expr(rewriter, binop->v.BinOp.right);

    PyObject *explanation = arena_pyobject(ARENA(rewriter),
        PyUnicode_FromFormat("(%S %s %S)", left_result.expl, symbol, right_result.expl));

    expr_ty res_expr = _PyAST_BinOp(left_result.expr, binop->v.BinOp.op, right_result.expr,
                                     POSITION(rewriter->assert_node), ARENA(rewriter));
    result.expr = assign(rewriter, res_expr);
    result.expl = explanation;

    return result;
}

// Visit UnaryOp expression
static VisitResult visit_UnaryOp(PyAssertRewriter *rewriter, expr_ty unary) {
    VisitResult result = {0};

    const char *pattern = unaryop_str(unary->v.UnaryOp.op);
    VisitResult operand_result = visit_expr(rewriter, unary->v.UnaryOp.operand);

    expr_ty res_expr = _PyAST_UnaryOp(unary->v.UnaryOp.op, operand_result.expr,
                                       POSITION(rewriter->assert_node), ARENA(rewriter));
    result.expr = assign(rewriter, res_expr);
    result.expl = arena_pyobject(ARENA(rewriter),
        PyUnicode_FromFormat(pattern, operand_result.expl));

    return result;
}

// Visit Compare expression
static VisitResult visit_Compare(PyAssertRewriter *rewriter, expr_ty comp) {
    VisitResult result = {0};

    push_format_context(rewriter);

    // Visit left operand
    VisitResult left_result = visit_expr(rewriter, comp->v.Compare.left);
    if (left_result.expr == NULL) {
        return result;
    }
    PyObject *left_expl = left_result.expl;
    expr_ty left_res = left_result.expr;

    // Wrap in parens if Compare or BoolOp
    if (comp->v.Compare.left->kind == Compare_kind ||
        comp->v.Compare.left->kind == BoolOp_kind) {
        PyObject *wrapped = PyUnicode_FromFormat("(%S)", left_expl);
        Py_DECREF(left_expl);
        left_expl = wrapped;
    }

    Py_ssize_t num_ops = asdl_seq_LEN(comp->v.Compare.ops);

    // Create variables for each comparison result
    PyObject **res_variables = PyMem_Malloc(sizeof(PyObject*) * num_ops);

    asdl_expr_seq *load_names = _Py_asdl_expr_seq_new(num_ops, ARENA(rewriter));
    asdl_expr_seq *store_names = _Py_asdl_expr_seq_new(num_ops, ARENA(rewriter));

    asdl_expr_seq *syms = _Py_asdl_expr_seq_new(num_ops, ARENA(rewriter));
    asdl_expr_seq *expls = _Py_asdl_expr_seq_new(num_ops, ARENA(rewriter));
    asdl_expr_seq *results = _Py_asdl_expr_seq_new(num_ops + 1, ARENA(rewriter));

    asdl_seq_SET(results, 0, left_res);

    for (Py_ssize_t i = 0; i < num_ops; i++) {
        PyObject *var_name = variable(rewriter);
        res_variables[i] = var_name;

        expr_ty load_var = _PyAST_Name(var_name, Load, POSITION(rewriter->assert_node), ARENA(rewriter));
        expr_ty store_var = _PyAST_Name(var_name, Store, POSITION(rewriter->assert_node), ARENA(rewriter));

        asdl_seq_SET(load_names, i, load_var);
        asdl_seq_SET(store_names, i, store_var);

        cmpop_ty op = asdl_seq_GET(comp->v.Compare.ops, i);
        expr_ty next_operand = asdl_seq_GET(comp->v.Compare.comparators, i);

        VisitResult next_result = visit_expr(rewriter, next_operand);
        PyObject *next_expl = next_result.expl;
        expr_ty next_res = next_result.expr;

        // Wrap in parens if needed
        if (next_operand->kind == Compare_kind || next_operand->kind == BoolOp_kind) {
            PyObject *wrapped = PyUnicode_FromFormat("(%S)", next_expl);
            Py_DECREF(next_expl);
            next_expl = wrapped;
        }

        const char *sym = cmpop_str(op);
        PyObject *sym_str = arena_pyobject(ARENA(rewriter), PyUnicode_FromString(sym));
        asdl_seq_SET(syms, i, _PyAST_Constant(sym_str, NULL,
                                              POSITION(rewriter->assert_node), ARENA(rewriter)));

        PyObject *expl = arena_pyobject(ARENA(rewriter), PyUnicode_FromFormat("%S %s %S", left_expl, sym, next_expl));
        asdl_seq_SET(expls, i, _PyAST_Constant(expl, NULL,
                                               POSITION(rewriter->assert_node), ARENA(rewriter)));

        // Create comparison: left_res op next_res
        asdl_int_seq *single_op = _Py_asdl_int_seq_new(1, ARENA(rewriter));
        asdl_seq_SET(single_op, 0, op);
        asdl_expr_seq *single_comp = _Py_asdl_expr_seq_new(1, ARENA(rewriter));
        asdl_seq_SET(single_comp, 0, next_res);

        expr_ty res_expr = _PyAST_Compare(left_res, single_op, single_comp,
                                         POSITION(rewriter->assert_node), ARENA(rewriter));

        // Assign to variable
        asdl_expr_seq *targets = _Py_asdl_expr_seq_new(1, ARENA(rewriter));
        asdl_seq_SET(targets, 0, store_var);
        stmt_ty assign_stmt = _PyAST_Assign(targets, res_expr, NULL,
                                           POSITION(rewriter->assert_node), ARENA(rewriter));

        // Add to statements
        asdl_stmt_seq *new_stmts = _Py_asdl_stmt_seq_new(
            asdl_seq_LEN(rewriter->statements) + 1, ARENA(rewriter));
        for (Py_ssize_t j = 0; j < asdl_seq_LEN(rewriter->statements); j++) {
            asdl_seq_SET(new_stmts, j, asdl_seq_GET(rewriter->statements, j));
        }
        asdl_seq_SET(new_stmts, asdl_seq_LEN(rewriter->statements), assign_stmt);
        rewriter->statements = new_stmts;

        asdl_seq_SET(results, i + 1, next_res);

        Py_DECREF(next_expl);
        left_res = next_res;
        Py_DECREF(left_expl);
        left_expl = PyUnicode_FromString("");  // Will be replaced
    }

    Py_DECREF(left_expl);
    PyMem_Free(res_variables);

    // Build call to _call_reprcompare
    expr_ty syms_tuple = _PyAST_Tuple(syms, Load, POSITION(rewriter->assert_node), ARENA(rewriter));
    expr_ty results_tuple = _PyAST_Tuple(results, Load, POSITION(rewriter->assert_node), ARENA(rewriter));
    expr_ty expls_tuple = _PyAST_Tuple(expls, Load, POSITION(rewriter->assert_node), ARENA(rewriter));
    expr_ty load_names_tuple = _PyAST_Tuple(load_names, Load, POSITION(rewriter->assert_node), ARENA(rewriter));

    // Create args array
    asdl_expr_seq *call_args = _Py_asdl_expr_seq_new(4, ARENA(rewriter));
    asdl_seq_SET(call_args, 0, syms_tuple);
    asdl_seq_SET(call_args, 1, load_names_tuple);
    asdl_seq_SET(call_args, 2, expls_tuple);
    asdl_seq_SET(call_args, 3, results_tuple);

    // @sys._call_reprcompare
    identifier sys_name = arena_pyobject(ARENA(rewriter), PyUnicode_FromString("@sys"));
    expr_ty sys_obj = _PyAST_Name(sys_name, Load, POSITION(rewriter->assert_node), ARENA(rewriter));
    identifier func_name = arena_pyobject(ARENA(rewriter), PyUnicode_FromString("_call_reprcompare"));
    expr_ty func = _PyAST_Attribute(sys_obj, func_name, Load, POSITION(rewriter->assert_node), ARENA(rewriter));

    expr_ty expl_call = _PyAST_Call(func, call_args, NULL,
                                    POSITION(rewriter->assert_node), ARENA(rewriter));

    // Create result expression
    if (num_ops > 1) {
        result.expr = _PyAST_BoolOp(And, load_names, POSITION(rewriter->assert_node), ARENA(rewriter));
    } else {
        result.expr = asdl_seq_GET(load_names, 0);
    }

    expr_ty formatted = pop_format_context(rewriter, expl_call);
    result.expl = explanation_param(rewriter, formatted);

    return result;
}

// Main visit function
static VisitResult visit_expr(PyAssertRewriter *rewriter, expr_ty expr) {
    VisitResult result = {0};

    if (expr == NULL) {
        PyErr_SetString(PyExc_SystemError, "Expression is NULL");
        return result;
    }

    switch (expr->kind) {
        case Name_kind:
            return visit_Name(rewriter, expr);

        case Compare_kind:
            return visit_Compare(rewriter, expr);

        case BinOp_kind:
            return visit_BinOp(rewriter, expr);

        case UnaryOp_kind:
            return visit_UnaryOp(rewriter, expr);

        case Constant_kind:
            // For constants, just assign and display
            result.expr = assign(rewriter, expr);
            if (result.expr == NULL) return result;
            result.expl = explanation_param(rewriter, display(rewriter, result.expr));
            return result;

        // For all other expression types, use generic visit
        default:
            // Generic visit: assign and display
            result.expr = assign(rewriter, expr);
            if (result.expr == NULL) return result;
            result.expl = explanation_param(rewriter, display(rewriter, result.expr));
            return result;
    }
}

// Main entry point
asdl_stmt_seq*
_PyAST_ExpandAssert(stmt_ty assert, PyArena *arena)
{
    if (assert == NULL || assert->kind != Assert_kind) {
        PyErr_SetString(PyExc_SystemError, "Expected Assert statement");
        return NULL;
    }

    PyAssertRewriter rewriter = {0};
    if (init_rewriter(&rewriter, arena, assert->v.Assert.test) < 0) {
        return NULL;
    }

    // Create import statement:
    // import _ast_rewrite as @sys, builtins as @py_builtins
    asdl_alias_seq *names = _Py_asdl_alias_seq_new(2, arena);
    if (names == NULL) {
        free_rewriter(&rewriter);
        return NULL;
    }

    PyObject *mod1 = arena_pyobject(arena, PyUnicode_FromString("_ast_rewrite"));
    PyObject *as1 = arena_pyobject(arena, PyUnicode_FromString("@sys"));
    PyObject *mod2 = arena_pyobject(arena, PyUnicode_FromString("builtins"));
    PyObject *as2 = arena_pyobject(arena, PyUnicode_FromString("@py_builtins"));

    if (!mod1 || !as1 || !mod2 || !as2) {
        free_rewriter(&rewriter);
        return NULL;
    }

    alias_ty alias1 = _PyAST_alias(mod1, as1, POSITION(assert), arena);
    alias_ty alias2 = _PyAST_alias(mod2, as2, POSITION(assert), arena);

    if (!alias1 || !alias2) {
        free_rewriter(&rewriter);
        return NULL;
    }

    asdl_seq_SET(names, 0, alias1);
    asdl_seq_SET(names, 1, alias2);

    stmt_ty import_stmt = _PyAST_Import(names, POSITION(assert), arena);
    if (!import_stmt) {
        free_rewriter(&rewriter);
        return NULL;
    }

    // Add import to statements
    asdl_stmt_seq *new_stmts = _Py_asdl_stmt_seq_new(1, arena);
    if (!new_stmts) {
        free_rewriter(&rewriter);
        return NULL;
    }
    asdl_seq_SET(new_stmts, 0, import_stmt);
    rewriter.statements = new_stmts;

    // Push format context for the whole assertion
    push_format_context(&rewriter);

    // Visit the test expression
    VisitResult top_result = visit_expr(&rewriter, assert->v.Assert.test);
    if (top_result.expr == NULL) {
        free_rewriter(&rewriter);
        return NULL;
    }

    // Create negation: not top_condition
    expr_ty negation = _PyAST_UnaryOp(Not, top_result.expr, POSITION(assert), arena);
    if (!negation) {
        free_rewriter(&rewriter);
        return NULL;
    }

    // Build the explanation message
    PyObject *expl_str;
    expr_ty assertmsg;

    if (assert->v.Assert.msg) {
        assertmsg = helper(&rewriter, "_format_assertmsg", 1, assert->v.Assert.msg);
        if (!assertmsg) {
            free_rewriter(&rewriter);
            return NULL;
        }
        expl_str = arena_pyobject(arena, PyUnicode_FromFormat("\n>assert %S", top_result.expl));
    } else {
        PyObject *empty_str = arena_pyobject(arena, PyUnicode_FromString(""));
        if (!empty_str) {
            free_rewriter(&rewriter);
            return NULL;
        }
        assertmsg = _PyAST_Constant(empty_str, NULL, POSITION(assert), arena);
        if (!assertmsg) {
            free_rewriter(&rewriter);
            return NULL;
        }
        expl_str = arena_pyobject(arena, PyUnicode_FromFormat("assert %S", top_result.expl));
    }

    if (!expl_str) {
        free_rewriter(&rewriter);
        return NULL;
    }

    expr_ty expl_const = _PyAST_Constant(expl_str, NULL, POSITION(assert), arena);
    if (!expl_const) {
        free_rewriter(&rewriter);
        return NULL;
    }

    expr_ty template = _PyAST_BinOp(assertmsg, Add, expl_const, POSITION(assert), arena);
    if (!template) {
        free_rewriter(&rewriter);
        return NULL;
    }

    expr_ty msg = pop_format_context(&rewriter, template);
    if (!msg) {
        free_rewriter(&rewriter);
        return NULL;
    }

    expr_ty fmt = helper(&rewriter, "_format_explanation", 1, msg);
    if (!fmt) {
        free_rewriter(&rewriter);
        return NULL;
    }

    // Create AssertionError
    expr_ty err_name = builtin_attr(&rewriter, "AssertionError");
    if (!err_name) {
        free_rewriter(&rewriter);
        return NULL;
    }

    asdl_expr_seq *exc_args = _Py_asdl_expr_seq_new(1, arena);
    if (!exc_args) {
        free_rewriter(&rewriter);
        return NULL;
    }
    asdl_seq_SET(exc_args, 0, fmt);

    expr_ty exc = _PyAST_Call(err_name, exc_args, NULL, POSITION(assert), arena);
    if (!exc) {
        free_rewriter(&rewriter);
        return NULL;
    }

    stmt_ty raise_stmt = _PyAST_Raise(exc, NULL, POSITION(assert), arena);
    if (!raise_stmt) {
        free_rewriter(&rewriter);
        return NULL;
    }

    // Add raise to expl_stmts
    asdl_stmt_seq *new_expl = _Py_asdl_stmt_seq_new(
        asdl_seq_LEN(rewriter.expl_stmts) + 1, arena);
    for (Py_ssize_t i = 0; i < asdl_seq_LEN(rewriter.expl_stmts); i++) {
        asdl_seq_SET(new_expl, i, asdl_seq_GET(rewriter.expl_stmts, i));
    }
    asdl_seq_SET(new_expl, asdl_seq_LEN(rewriter.expl_stmts), raise_stmt);
    rewriter.expl_stmts = new_expl;

    // Create if statement
    stmt_ty if_stmt = _PyAST_If(negation, rewriter.expl_stmts, NULL, POSITION(assert), arena);

    // Add if statement to main statements
    new_stmts = _Py_asdl_stmt_seq_new(asdl_seq_LEN(rewriter.statements) + 1, arena);
    for (Py_ssize_t i = 0; i < asdl_seq_LEN(rewriter.statements); i++) {
        asdl_seq_SET(new_stmts, i, asdl_seq_GET(rewriter.statements, i));
    }
    asdl_seq_SET(new_stmts, asdl_seq_LEN(rewriter.statements), if_stmt);
    rewriter.statements = new_stmts;

    // Clear temporary variables
    Py_ssize_t num_vars = PyList_Size(rewriter.variables);
    if (num_vars > 0) {
        asdl_expr_seq *var_exprs = _Py_asdl_expr_seq_new(num_vars, arena);
        for (Py_ssize_t i = 0; i < num_vars; i++) {
            PyObject *var_name = PyList_GetItem(rewriter.variables, i);
            expr_ty var_expr = _PyAST_Name(var_name, Store, POSITION(assert), arena);
            asdl_seq_SET(var_exprs, i, var_expr);
        }

        expr_ty none_val = _PyAST_Constant(Py_None, NULL, POSITION(assert), arena);
        stmt_ty clear_stmt = _PyAST_Assign(var_exprs, none_val, NULL, POSITION(assert), arena);

        new_stmts = _Py_asdl_stmt_seq_new(asdl_seq_LEN(rewriter.statements) + 1, arena);
        for (Py_ssize_t i = 0; i < asdl_seq_LEN(rewriter.statements); i++) {
            asdl_seq_SET(new_stmts, i, asdl_seq_GET(rewriter.statements, i));
        }
        asdl_seq_SET(new_stmts, asdl_seq_LEN(rewriter.statements), clear_stmt);
        rewriter.statements = new_stmts;
    }

    asdl_stmt_seq *result = rewriter.statements;
    free_rewriter(&rewriter);

    return result;
}
