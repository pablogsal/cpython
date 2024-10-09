#include "Python.h"
#include "pycore_ast.h"           // expr_ty
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_runtime.h"       // _Py_ID()
#include <float.h>                // DBL_MAX_10_EXP
#include <stdbool.h>

typedef struct {
    // Add any necessary fields here in the future
} PyAssertRewriter;

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
    printf("BoolOp: %s\n", op == And ? "And" : "Or");
    return 0;
}

int visit_operator(PyAssertRewriter* rewriter, operator_ty op) {
    const char* op_str[] = {"Add", "Sub", "Mult", "MatMult", "Div", "Mod", "Pow",
                            "LShift", "RShift", "BitOr", "BitXor", "BitAnd", "FloorDiv"};
    printf("Operator: %s\n", op_str[op - 1]);
    return 0;
}

int visit_unaryop(PyAssertRewriter* rewriter, unaryop_ty op) {
    const char* op_str[] = {"Invert", "Not", "UAdd", "USub"};
    printf("UnaryOp: %s\n", op_str[op - 1]);
    return 0;
}

int visit_cmpop(PyAssertRewriter* rewriter, cmpop_ty op) {
    const char* op_str[] = {"Eq", "NotEq", "Lt", "LtE", "Gt", "GtE", "Is", "IsNot", "In", "NotIn"};
    printf("CmpOp: %s\n", op_str[op - 1]);
    return 0;
}

int visit_expr_context(PyAssertRewriter* rewriter, expr_context_ty ctx) {
    const char* ctx_str[] = {"Load", "Store", "Del"};
    printf("ExprContext: %s\n", ctx_str[ctx - 1]);
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
    printf("BoolOp expression\n");
    visit_boolop(rewriter, expr->v.BoolOp.op);
    visit_expr_seq(rewriter, expr->v.BoolOp.values);
    return 0;
}

int visit_named_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("NamedExpr expression\n");
    visit_expr(rewriter, expr->v.NamedExpr.target);
    visit_expr(rewriter, expr->v.NamedExpr.value);
    return 0;
}

int visit_binop_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("BinOp expression\n");
    visit_expr(rewriter, expr->v.BinOp.left);
    visit_operator(rewriter, expr->v.BinOp.op);
    visit_expr(rewriter, expr->v.BinOp.right);
    return 0;
}

int visit_unaryop_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("UnaryOp expression\n");
    visit_unaryop(rewriter, expr->v.UnaryOp.op);
    visit_expr(rewriter, expr->v.UnaryOp.operand);
    return 0;
}

int visit_lambda_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Lambda expression\n");
    visit_arguments(rewriter, expr->v.Lambda.args);
    visit_expr(rewriter, expr->v.Lambda.body);
    return 0;
}

int visit_ifexp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("IfExp expression\n");
    visit_expr(rewriter, expr->v.IfExp.test);
    visit_expr(rewriter, expr->v.IfExp.body);
    visit_expr(rewriter, expr->v.IfExp.orelse);
    return 0;
}

int visit_dict_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Dict expression\n");
    visit_expr_seq(rewriter, expr->v.Dict.keys);
    visit_expr_seq(rewriter, expr->v.Dict.values);
    return 0;
}

int visit_set_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Set expression\n");
    visit_expr_seq(rewriter, expr->v.Set.elts);
    return 0;
}

int visit_listcomp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("ListComp expression\n");
    visit_expr(rewriter, expr->v.ListComp.elt);
    for (int i = 0; i < asdl_seq_LEN(expr->v.ListComp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.ListComp.generators, i));
    }
    return 0;
}

int visit_setcomp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("SetComp expression\n");
    visit_expr(rewriter, expr->v.SetComp.elt);
    for (int i = 0; i < asdl_seq_LEN(expr->v.SetComp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.SetComp.generators, i));
    }
    return 0;
}

int visit_generatorexp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("GeneratorExp expression\n");
    visit_expr(rewriter, expr->v.GeneratorExp.elt);
    for (int i = 0; i < asdl_seq_LEN(expr->v.GeneratorExp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.GeneratorExp.generators, i));
    }
    return 0;
}

int visit_dictcomp_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("DictComp expression\n");
    visit_expr(rewriter, expr->v.DictComp.key);
    visit_expr(rewriter, expr->v.DictComp.value);
    for (int i = 0; i < asdl_seq_LEN(expr->v.DictComp.generators); i++) {
        visit_comprehension(rewriter, asdl_seq_GET(expr->v.DictComp.generators, i));
    }
    return 0;
}

int visit_await_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Await expression\n");
    visit_expr(rewriter, expr->v.Await.value);
    return 0;
}

int visit_yield_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Yield expression\n");
    if (expr->v.Yield.value) {
        visit_expr(rewriter, expr->v.Yield.value);
    }
    return 0;
}

int visit_yieldfrom_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("YieldFrom expression\n");
    visit_expr(rewriter, expr->v.YieldFrom.value);
    return 0;
}

int visit_compare_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Compare expression\n");
    visit_expr(rewriter, expr->v.Compare.left);
    visit_int_seq(rewriter, expr->v.Compare.ops);
    visit_expr_seq(rewriter, expr->v.Compare.comparators);
    return 0;
}

int visit_call_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Call expression\n");
    visit_expr(rewriter, expr->v.Call.func);
    visit_expr_seq(rewriter, expr->v.Call.args);
    for (int i = 0; i < asdl_seq_LEN(expr->v.Call.keywords); i++) {
        visit_keyword(rewriter, asdl_seq_GET(expr->v.Call.keywords, i));
    }
    return 0;
}

int visit_formatted_value_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("FormattedValue expression\n");
    visit_expr(rewriter, expr->v.FormattedValue.value);
    if (expr->v.FormattedValue.format_spec) {
        visit_expr(rewriter, expr->v.FormattedValue.format_spec);
    }
    return 0;
}

int visit_joinedstr_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("JoinedStr expression\n");
    visit_expr_seq(rewriter, expr->v.JoinedStr.values);
    return 0;
}

int visit_constant_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Constant expression\n");
    return 0;
    // Note: You might want to add logic to print the constant value based on its type
}

int visit_attribute_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Attribute expression\n");
    visit_expr(rewriter, expr->v.Attribute.value);
    printf("Attribute: ");
    _PyObject_Dump(expr->v.Attribute.attr);
    visit_expr_context(rewriter, expr->v.Attribute.ctx);
    return 0;
}

int visit_subscript_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Subscript expression\n");
    visit_expr(rewriter, expr->v.Subscript.value);
    visit_expr(rewriter, expr->v.Subscript.slice);
    visit_expr_context(rewriter, expr->v.Subscript.ctx);
    return 0;
}

int visit_starred_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Starred expression\n");
    visit_expr(rewriter, expr->v.Starred.value);
    visit_expr_context(rewriter, expr->v.Starred.ctx);
    return 0;
}

int visit_name_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Name expression\n");
    printf("Identifier: ");
    _PyObject_Dump(expr->v.Name.id);
    visit_expr_context(rewriter, expr->v.Name.ctx);
    return 0;
}

int visit_list_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("List expression\n");
    visit_expr_seq(rewriter, expr->v.List.elts);
    visit_expr_context(rewriter, expr->v.List.ctx);
    return 0;
}

int visit_tuple_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Tuple expression\n");
    visit_expr_seq(rewriter, expr->v.Tuple.elts);
    visit_expr_context(rewriter, expr->v.Tuple.ctx);
    return 0;
}

int visit_slice_expr(PyAssertRewriter* rewriter, expr_ty expr) {
    printf("Slice expression\n");
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

    printf("Expression at line %d, col %d\n", expr->lineno, expr->col_offset);

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
    printf("Comprehension\n");
    visit_expr(rewriter, comp->target);
    visit_expr(rewriter, comp->iter);
    visit_expr_seq(rewriter, comp->ifs);
    printf("Is async: %d\n", comp->is_async);
    return 0;
}

int visit_arguments(PyAssertRewriter* rewriter, arguments_ty args) {
    assert(args != NULL);
    printf("Arguments\n");
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
    printf("Arg: ");
    _PyObject_Dump(arg->arg);
    if (arg->annotation) visit_expr(rewriter, arg->annotation);
    if (arg->type_comment) {
        printf("Type comment: ");
        _PyObject_Dump(arg->type_comment);
    }
    printf("Line: %d, Col: %d, End Line: %d, End Col: %d\n",
           arg->lineno, arg->col_offset, arg->end_lineno, arg->end_col_offset);
    return 0;
}

int visit_keyword(PyAssertRewriter* rewriter, keyword_ty keyword) {
    assert(keyword != NULL);
    printf("Keyword\n");
    if (keyword->arg) {
        printf("Arg: \n");
        _PyObject_Dump(keyword->arg);
    }
    visit_expr(rewriter, keyword->value);
    printf("Line: %d, Col: %d, End Line: %d, End Col: %d\n",
           keyword->lineno, keyword->col_offset, keyword->end_lineno, keyword->end_col_offset);
    return 0;
}

PyObject *
_PyAST_ExpandAssert(stmt_ty assert, PyArena *arena)
{
    PyAssertRewriter rewriter = {0};
    if(visit_expr(&rewriter, assert->v.Assert.test) != 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_SystemError, "Failed to visit expression");
        }
        return NULL;
    }
    Py_RETURN_NONE;
}
