#include "Python.h"
#include "pycore_ast.h"           // expr_ty
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_runtime.h"       // _Py_ID()
#include "internal/pycore_ast.h"
#include <float.h>                // DBL_MAX_10_EXP
#include <stdbool.h>

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

typedef struct {
    PyArena *arena;
    int tmp_name_counter;
    expr_ty result;
} PyAssertRewriter;

#define POSITION(n) (n)->lineno, (n)->col_offset, (n)->end_lineno, (n)->end_col_offset
#define ARENA(r) (r)->arena

// Forward declarations
int visit_expr(PyAssertRewriter* rewriter, expr_ty expr);
int visit_boolop(PyAssertRewriter* rewriter, boolop_ty op);
int visit_operator(PyAssertRewriter* rewriter, operator_ty op);
int visit_unaryop(PyAssertRewriter* rewriter, unaryop_ty op);
int visit_cmpop(PyAssertRewriter* rewriter, cmpop_ty op);
int visit_expr_context(PyAssertRewriter* rewriter, expr_context_ty ctx);
int visit_expr_seq(PyAssertRewriter* rewriter, asdl_expr_seq* seq);
int visit_int_seq(PyAssertRewriter* rewriter, asdl_int_seq* seq);
int visit_arguments(PyAssertRewriter* rewriter, arguments_ty args);
int visit_arg(PyAssertRewriter* rewriter, arg_ty arg);
int visit_keyword(PyAssertRewriter* rewriter, keyword_ty keyword);
int visit_comprehension(PyAssertRewriter* rewriter, struct _comprehension *comp);
int visit_alias(PyAssertRewriter* rewriter, alias_ty alias);
int visit_arg_seq(PyAssertRewriter* rewriter, asdl_arg_seq* seq);

// Helper functions for specific node types
int visit_boolop(PyAssertRewriter* rewriter, boolop_ty op) {
    DEBUG_PRINT("BoolOp: %s\n", op == And ? "And" : "Or");
    return 0;
}

int visit_operator(PyAssertRewriter* rewriter, operator_ty op) {
    const char* op_str[] = {"Add", "Sub", "Mult", "MatMult", "Div", "Mod", "Pow",
                            "LShift", "RShift", "BitOr", "BitXor", "BitAnd", "FloorDiv"};
    DEBUG_PRINT("Operator: %s\n", op_str[op - 1]);
    return 0;
}

int visit_unaryop(PyAssertRewriter* rewriter, unaryop_ty op) {
    const char* op_str[] = {"Invert", "Not", "UAdd", "USub"};
    DEBUG_PRINT("UnaryOp: %s\n", op_str[op - 1]);
    return 0;
}

int visit_cmpop(PyAssertRewriter* rewriter, cmpop_ty op) {
    const char* op_str[] = {"Eq", "NotEq", "Lt", "LtE", "Gt", "GtE", "Is", "IsNot", "In", "NotIn"};
    DEBUG_PRINT("CmpOp: %s\n", op_str[op - 1]);
    return 0;
}

int visit_expr_context(PyAssertRewriter* rewriter, expr_context_ty ctx) {
    const char* ctx_str[] = {"Load", "Store", "Del"};
    DEBUG_PRINT("ExprContext: %s\n", ctx_str[ctx - 1]);
    return 0;
}

// Helper function to visit sequences of expressions
int visit_expr_seq(PyAssertRewriter* rewriter, asdl_expr_seq* seq) {
    if (seq == NULL) {
        return 0;
    }
    for (int i = 0; i < asdl_seq_LEN(seq); i++) {
        visit_expr(rewriter, asdl_seq_GET(seq, i));
    }
    return 0;
}

// Helper function to visit sequences of integers (for Compare ops)
int visit_int_seq(PyAssertRewriter* rewriter, asdl_int_seq* seq) {
    if (seq == NULL) {
        return 0;
    }
    for (int i = 0; i < asdl_seq_LEN(seq); i++) {
        visit_cmpop(rewriter, (cmpop_ty)asdl_seq_GET(seq, i));
    }
    return 0;
}

// Helper function to visit sequences of arguments
int visit_arg_seq(PyAssertRewriter* rewriter, asdl_arg_seq* seq) {
    if (seq == NULL) {
        return 0;
    }
    for (int i = 0; i < asdl_seq_LEN(seq); i++) {
        visit_arg(rewriter, asdl_seq_GET(seq, i));
    }
    return 0;
}

// Functions for specific expression types
int visit_boolop_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("BoolOp expression\n");
    visit_boolop(rewriter, expr->v.BoolOp.op);
    visit_expr_seq(rewriter, expr->v.BoolOp.values);
    return 0;
}

int visit_named_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("NamedExpr expression\n");
    visit_expr(rewriter, expr->v.NamedExpr.target);
    visit_expr(rewriter, expr->v.NamedExpr.value);
    return 0;
}

int visit_binop_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("BinOp expression\n");
    visit_expr(rewriter, expr->v.BinOp.left);
    visit_operator(rewriter, expr->v.BinOp.op);
    visit_expr(rewriter, expr->v.BinOp.right);
    return 0;
}

int visit_unaryop_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("UnaryOp expression\n");
    visit_unaryop(rewriter, expr->v.UnaryOp.op);
    visit_expr(rewriter, expr->v.UnaryOp.operand);
    return 0;
}

int visit_lambda_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Lambda expression\n");
    visit_arguments(rewriter, expr->v.Lambda.args);
    visit_expr(rewriter, expr->v.Lambda.body);
    return 0;
}

int visit_ifexp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("IfExp expression\n");
    visit_expr(rewriter, expr->v.IfExp.test);
    visit_expr(rewriter, expr->v.IfExp.body);
    visit_expr(rewriter, expr->v.IfExp.orelse);
    return 0;
}

int visit_dict_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Dict expression\n");
    visit_expr_seq(rewriter, expr->v.Dict.keys);
    visit_expr_seq(rewriter, expr->v.Dict.values);
    return 0;
}

int visit_set_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Set expression\n");
    visit_expr_seq(rewriter, expr->v.Set.elts);
    return 0;
}

int visit_listcomp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("ListComp expression\n");
    visit_expr(rewriter, expr->v.ListComp.elt);
    for (int i = 0; i < asdl_seq_LEN(expr->v.ListComp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.ListComp.generators, i));
    }
    return 0;
}

int visit_setcomp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("SetComp expression\n");
    visit_expr(rewriter, expr->v.SetComp.elt);
    for (int i = 0; i < asdl_seq_LEN(expr->v.SetComp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.SetComp.generators, i));
    }
    return 0;
}

int visit_generatorexp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("GeneratorExp expression\n");
    visit_expr(rewriter, expr->v.GeneratorExp.elt);
    for (int i = 0; i < asdl_seq_LEN(expr->v.GeneratorExp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.GeneratorExp.generators, i));
    }
    return 0;
}

int visit_dictcomp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("DictComp expression\n");
    visit_expr(rewriter, expr->v.DictComp.key);
    visit_expr(rewriter, expr->v.DictComp.value);
    for (int i = 0; i < asdl_seq_LEN(expr->v.DictComp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.DictComp.generators, i));
    }
    return 0;
}

int visit_await_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Await expression\n");
    visit_expr(rewriter, expr->v.Await.value);
    return 0;
}

int visit_yield_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Yield expression\n");
    if (expr->v.Yield.value) {
        visit_expr(rewriter, expr->v.Yield.value);
    }
    return 0;
}

int visit_yieldfrom_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("YieldFrom expression\n");
    visit_expr(rewriter, expr->v.YieldFrom.value);
    return 0;
}

static expr_ty create_compare_call(PyAssertRewriter *rewriter, expr_ty left, const char* op, expr_ty right, expr_ty orig) {
    expr_ty func = _PyAST_Name(PyUnicode_FromString("compare"), Load, POSITION(orig), ARENA(rewriter));
    if (func == NULL) return NULL;

    asdl_expr_seq *args = _Py_asdl_expr_seq_new(3, rewriter->arena);
    if (args == NULL) return NULL;
    asdl_seq_SET(args, 0, left);
    asdl_seq_SET(args, 1, _PyAST_Constant(PyUnicode_FromString(op), NULL, POSITION(orig), ARENA(rewriter)));
    asdl_seq_SET(args, 2, right);

    return _PyAST_Call(func, args, NULL, POSITION(orig), ARENA(rewriter));
}


static expr_ty create_named_expr(PyAssertRewriter *rewriter, identifier name, expr_ty value, expr_ty orig) {
    expr_ty target = _PyAST_Name(name, Store, POSITION(orig), ARENA(rewriter));
    if (target == NULL) return NULL;

    return _PyAST_NamedExpr(target, value, POSITION(orig), ARENA(rewriter));
}

static expr_ty create_bool_op(PyAssertRewriter *rewriter, boolop_ty op, expr_ty left, expr_ty right, expr_ty orig) {
    asdl_expr_seq *values = _Py_asdl_expr_seq_new(2, rewriter->arena);
    if (values == NULL) return NULL;
    asdl_seq_SET(values, 0, left);
    asdl_seq_SET(values, 1, right);

    return _PyAST_BoolOp(op, values, POSITION(orig), ARENA(rewriter));
}

int visit_compare_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    assert(expr != NULL);
    DEBUG_PRINT("Compare expression\n");

    expr_ty result = NULL;
    expr_ty left = expr->v.Compare.left;

    for (int i = 0; i < asdl_seq_LEN(expr->v.Compare.ops); i++) {
        cmpop_ty op = (cmpop_ty)asdl_seq_GET(expr->v.Compare.ops, i);
        expr_ty right = asdl_seq_GET(expr->v.Compare.comparators, i);

        const char* op_str = (op == Eq) ? "==" :
                             (op == NotEq) ? "!=" :
                             (op == Lt) ? "<" :
                             (op == LtE) ? "<=" :
                             (op == Gt) ? ">" :
                             (op == GtE) ? ">=" :
                             (op == Is) ? "is" :
                             (op == IsNot) ? "is not" :
                             (op == In) ? "in" :
                             (op == NotIn) ? "not in" : "unknown";

        identifier tmp_name = PyUnicode_FromFormat("_tmp%d", rewriter->tmp_name_counter++);
        if (tmp_name == NULL) return -1;

        expr_ty compare_call = create_compare_call(rewriter, left, op_str, right, expr);
        if (compare_call == NULL) return -1;

        expr_ty named_expr = create_named_expr(rewriter, tmp_name, compare_call, expr);
        if (named_expr == NULL) return -1;

        if (result == NULL) {
            result = named_expr;
        } else {
            result = create_bool_op(rewriter, And, result, named_expr, expr);
            if (result == NULL) return -1;
        }

        // For the next iteration, the right operand becomes the left operand
        left = right;
    }

    rewriter->result = result;
    return 0;
}


int visit_call_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Call expression\n");
    visit_expr(rewriter, expr->v.Call.func);
    visit_expr_seq(rewriter, expr->v.Call.args);
    for (int i = 0; i < asdl_seq_LEN(expr->v.Call.keywords); i++) {
        visit_keyword(rewriter, asdl_seq_GET(expr->v.Call.keywords, i));
    }
    return 0;
}

int visit_formatted_value_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("FormattedValue expression\n");
    visit_expr(rewriter, expr->v.FormattedValue.value);
    if (expr->v.FormattedValue.format_spec) {
        visit_expr(rewriter, expr->v.FormattedValue.format_spec);
    }
    return 0;
}

int visit_joinedstr_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("JoinedStr expression\n");
    visit_expr_seq(rewriter, expr->v.JoinedStr.values);
    return 0;
}

int visit_constant_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    return 0;
}

int visit_attribute_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Attribute expression\n");
    visit_expr(rewriter, expr->v.Attribute.value);
    DEBUG_PRINT("Attribute: ");
    // _PyObject_Dump(expr->v.Attribute.attr);
    visit_expr_context(rewriter, expr->v.Attribute.ctx);
    return 0;
}

int visit_subscript_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Subscript expression\n");
    visit_expr(rewriter, expr->v.Subscript.value);
    visit_expr(rewriter, expr->v.Subscript.slice);
    visit_expr_context(rewriter, expr->v.Subscript.ctx);
    return 0;
}

int visit_starred_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Starred expression\n");
    visit_expr(rewriter, expr->v.Starred.value);
    visit_expr_context(rewriter, expr->v.Starred.ctx);
    return 0;
}

int visit_name_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Name expression\n");
    DEBUG_PRINT("Identifier: ");
    // _PyObject_Dump(expr->v.Name.id);
    visit_expr_context(rewriter, expr->v.Name.ctx);
    return 0;
}

int visit_list_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("List expression\n");
    visit_expr_seq(rewriter, expr->v.List.elts);
    visit_expr_context(rewriter, expr->v.List.ctx);
    return 0;
}

int visit_tuple_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Tuple expression\n");
    visit_expr_seq(rewriter, expr->v.Tuple.elts);
    visit_expr_context(rewriter, expr->v.Tuple.ctx);
    return 0;
}

int visit_slice_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    DEBUG_PRINT("Slice expression\n");
    if (expr->v.Slice.lower) visit_expr(rewriter, expr->v.Slice.lower);
    if (expr->v.Slice.upper) visit_expr(rewriter, expr->v.Slice.upper);
    if (expr->v.Slice.step) visit_expr(rewriter, expr->v.Slice.step);
    return 0;
}

// Main visitor function
int visit_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    if (expr == NULL) {
        PyErr_SetString(PyExc_SystemError, "Expression is NULL");
        return -1;
    }

    DEBUG_PRINT("Expression at line %d, col %d\n", expr->lineno, expr->col_offset);

    switch (expr->kind) {
        case BoolOp_kind: visit_boolop_expr(rewriter, expr); break;
        case NamedExpr_kind: visit_named_expr(rewriter, expr); break;
        case BinOp_kind: visit_binop_expr(rewriter, expr); break;
        case UnaryOp_kind: visit_unaryop_expr(rewriter, expr); break;
        case Lambda_kind: visit_lambda_expr(rewriter, expr); break;
        case IfExp_kind: visit_ifexp_expr(rewriter, expr); break;
        case Dict_kind: visit_dict_expr(rewriter, expr); break;
        case Set_kind: visit_set_expr(rewriter, expr); break;
        case ListComp_kind: visit_listcomp_expr(rewriter, expr); break;
        case SetComp_kind: visit_setcomp_expr(rewriter, expr); break;
        case DictComp_kind: visit_dictcomp_expr(rewriter, expr); break;
        case GeneratorExp_kind: visit_generatorexp_expr(rewriter, expr); break;
        case Await_kind: visit_await_expr(rewriter, expr); break;
        case Yield_kind: visit_yield_expr(rewriter, expr); break;
        case YieldFrom_kind: visit_yieldfrom_expr(rewriter, expr); break;
        case Compare_kind: visit_compare_expr(rewriter, expr); break;
        case Call_kind: visit_call_expr(rewriter, expr); break;
        case FormattedValue_kind: visit_formatted_value_expr(rewriter, expr); break;
        case JoinedStr_kind: visit_joinedstr_expr(rewriter, expr); break;
        case Constant_kind: visit_constant_expr(rewriter, expr); break;
        case Attribute_kind: visit_attribute_expr(rewriter, expr); break;
        case Subscript_kind: visit_subscript_expr(rewriter, expr); break;
        case Starred_kind: visit_starred_expr(rewriter, expr); break;
        case Name_kind: visit_name_expr(rewriter, expr); break;
        case List_kind: visit_list_expr(rewriter, expr); break;
        case Tuple_kind: visit_tuple_expr(rewriter, expr); break;
        case Slice_kind: visit_slice_expr(rewriter, expr); break;
        default: {
            PyErr_Format(PyExc_SystemError, "Unknown expression kind %d", expr->kind);
            return -1;
        }
    }
    return 0;
}

int visit_comprehension(PyAssertRewriter* rewriter, struct _comprehension *comp) {
    assert(comp != NULL);
    DEBUG_PRINT("Comprehension\n");
    visit_expr(rewriter, comp->target);
    visit_expr(rewriter, comp->iter);
    visit_expr_seq(rewriter, comp->ifs);
    DEBUG_PRINT("Is async: %d\n", comp->is_async);
    return 0;
}

int visit_arguments(PyAssertRewriter* rewriter, arguments_ty args) {
    assert(args != NULL);
    DEBUG_PRINT("Arguments\n");
    visit_arg_seq(rewriter, args->posonlyargs);
    visit_arg_seq(rewriter, args->args);
    if (args->vararg) visit_arg(rewriter, args->vararg);
    visit_arg_seq(rewriter, args->kwonlyargs);
    visit_expr_seq(rewriter, args->kw_defaults);
    if (args->kwarg) visit_arg(rewriter, args->kwarg);
    visit_expr_seq(rewriter, args->defaults);
    return 0;
}

int visit_arg(PyAssertRewriter* rewriter, arg_ty arg) {
    assert(arg != NULL);
    DEBUG_PRINT("Arg: ");
    // _PyObject_Dump(arg->arg);
    if (arg->annotation) visit_expr(rewriter, arg->annotation);
    if (arg->type_comment) {
        DEBUG_PRINT("Type comment: ");
        // _PyObject_Dump(arg->type_comment);
    }
    DEBUG_PRINT("Line: %d, Col: %d, End Line: %d, End Col: %d\n",
           arg->lineno, arg->col_offset, arg->end_lineno, arg->end_col_offset);
    return 0;
}

int visit_keyword(PyAssertRewriter* rewriter, keyword_ty keyword) {
    assert(keyword != NULL);
    DEBUG_PRINT("Keyword\n");
    if (keyword->arg) {
        DEBUG_PRINT("Arg: \n");
        // _PyObject_Dump(keyword->arg);
    }
    visit_expr(rewriter, keyword->value);
    DEBUG_PRINT("Line: %d, Col: %d, End Line: %d, End Col: %d\n",
           keyword->lineno, keyword->col_offset, keyword->end_lineno, keyword->end_col_offset);
    return 0;
}

static void PyAssertRewriter_free(PyAssertRewriter *rewriter) {
    return;
}

static int PyAssertRewriter_init(PyAssertRewriter *rewriter, PyArena *arena) {
    rewriter->arena = arena;
    return 0;
}

asdl_stmt_seq*
_PyAST_ExpandAssert(stmt_ty assert, PyArena *arena)
{
    PyAssertRewriter rewriter = {0};
    if (PyAssertRewriter_init(&rewriter, arena) < 0) {
        return NULL;
    }

    if(visit_expr(&rewriter, assert->v.Assert.test) != 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_SystemError, "Failed to visit expression");
        }
        return NULL;
    }

    PyAssertRewriter_free(&rewriter);

    if (rewriter.result == NULL) {
        return _Py_asdl_stmt_seq_new(0, arena);
    }

    stmt_ty expr = _PyAST_Expr(rewriter.result, POSITION(assert), arena);

    asdl_stmt_seq *wrapper = _Py_asdl_stmt_seq_new(1, arena);
    if (wrapper == NULL) {
        return NULL;
    }
    asdl_seq_SET(wrapper, 0, expr);
    return wrapper;
}
