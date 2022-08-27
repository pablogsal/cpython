static const int n_keyword_lists = 9;
static KeywordToken *reserved_keywords[] = {
    NULL,
    NULL,
    (KeywordToken[]) {
        {"if", 603},
        {"in", 613},
        {"as", 615},
        {"is", 622},
        {"or", 624},
        {NULL, -1},
    },
    (KeywordToken[]) {
        {"del", 596},
        {"try", 604},
        {"def", 609},
        {"for", 612},
        {"not", 621},
        {"and", 625},
        {NULL, -1},
    },
    (KeywordToken[]) {
        {"pass", 595},
        {"from", 607},
        {"elif", 610},
        {"else", 611},
        {"with", 614},
        {"None", 618},
        {"True", 619},
        {NULL, -1},
    },
    (KeywordToken[]) {
        {"raise", 594},
        {"yield", 597},
        {"break", 599},
        {"while", 605},
        {"class", 608},
        {"False", 620},
        {NULL, -1},
    },
    (KeywordToken[]) {
        {"return", 593},
        {"assert", 598},
        {"global", 601},
        {"import", 606},
        {"except", 616},
        {"lambda", 623},
        {NULL, -1},
    },
    (KeywordToken[]) {
        {"finally", 617},
        {NULL, -1},
    },
    (KeywordToken[]) {
        {"continue", 600},
        {"nonlocal", 602},
        {NULL, -1},
    },
};

enum {
    SK_MATCH,
    SK_CASE,
    SK__,
};

static const char *soft_keywords[] = {
    "match",
    "case",
    "_",
};

enum {
    R_FILE,
    R_INTERACTIVE,
    R_EVAL,
    R_FUNC_TYPE,
    R_FSTRING,
    R_STATEMENTS,
    R_STATEMENT,
    R_STATEMENT_NEWLINE,
    R_SIMPLE_STMTS,
    R_SIMPLE_STMT,
    R_COMPOUND_STMT,
    R_ASSIGNMENT,
    R_ANNOTATED_RHS,
    R_AUGASSIGN,
    R_RETURN_STMT,
    R_RAISE_STMT,
    R_GLOBAL_STMT,
    R_NONLOCAL_STMT,
    R_DEL_STMT,
    R_YIELD_STMT,
    R_ASSERT_STMT,
    R_IMPORT_STMT,
    R_IMPORT_NAME,
    R_IMPORT_FROM,
    R_IMPORT_FROM_TARGETS,
    R_IMPORT_FROM_AS_NAMES,
    R_IMPORT_FROM_AS_NAME,
    R_DOTTED_AS_NAMES,
    R_DOTTED_AS_NAME,
    R_DOTTED_NAME,
    R_BLOCK,
    R_DECORATORS,
    R_CLASS_DEF,
    R_CLASS_DEF_RAW,
    R_FUNCTION_DEF,
    R_FUNCTION_DEF_RAW,
    R_PARAMS,
    R_PARAMETERS,
    R_SLASH_NO_DEFAULT,
    R_SLASH_WITH_DEFAULT,
    R_STAR_ETC,
    R_KWDS,
    R_PARAM_NO_DEFAULT,
    R_PARAM_NO_DEFAULT_STAR_ANNOTATION,
    R_PARAM_WITH_DEFAULT,
    R_PARAM_MAYBE_DEFAULT,
    R_PARAM,
    R_PARAM_STAR_ANNOTATION,
    R_ANNOTATION,
    R_STAR_ANNOTATION,
    R_DEFAULT,
    R_IF_STMT,
    R_ELIF_STMT,
    R_ELSE_BLOCK,
    R_WHILE_STMT,
    R_FOR_STMT,
    R_WITH_STMT,
    R_WITH_ITEM,
    R_TRY_STMT,
    R_EXCEPT_BLOCK,
    R_EXCEPT_STAR_BLOCK,
    R_FINALLY_BLOCK,
    R_MATCH_STMT,
    R_SUBJECT_EXPR,
    R_CASE_BLOCK,
    R_GUARD,
    R_PATTERNS,
    R_PATTERN,
    R_AS_PATTERN,
    R_OR_PATTERN,
    R_CLOSED_PATTERN,
    R_LITERAL_PATTERN,
    R_LITERAL_EXPR,
    R_COMPLEX_NUMBER,
    R_SIGNED_NUMBER,
    R_SIGNED_REAL_NUMBER,
    R_REAL_NUMBER,
    R_IMAGINARY_NUMBER,
    R_CAPTURE_PATTERN,
    R_PATTERN_CAPTURE_TARGET,
    R_WILDCARD_PATTERN,
    R_VALUE_PATTERN,
    R_ATTR,
    R_NAME_OR_ATTR,
    R_GROUP_PATTERN,
    R_SEQUENCE_PATTERN,
    R_OPEN_SEQUENCE_PATTERN,
    R_MAYBE_SEQUENCE_PATTERN,
    R_MAYBE_STAR_PATTERN,
    R_STAR_PATTERN,
    R_MAPPING_PATTERN,
    R_ITEMS_PATTERN,
    R_KEY_VALUE_PATTERN,
    R_DOUBLE_STAR_PATTERN,
    R_CLASS_PATTERN,
    R_POSITIONAL_PATTERNS,
    R_KEYWORD_PATTERNS,
    R_KEYWORD_PATTERN,
    R_EXPRESSIONS,
    R_EXPRESSION,
    R_YIELD_EXPR,
    R_STAR_EXPRESSIONS,
    R_STAR_EXPRESSION,
    R_STAR_NAMED_EXPRESSIONS,
    R_STAR_NAMED_EXPRESSION,
    R_ASSIGNMENT_EXPRESSION,
    R_NAMED_EXPRESSION,
    R_DISJUNCTION,
    R_CONJUNCTION,
    R_INVERSION,
    R_COMPARISON,
    R_COMPARE_OP_BITWISE_OR_PAIR,
    R_EQ_BITWISE_OR,
    R_NOTEQ_BITWISE_OR,
    R_LTE_BITWISE_OR,
    R_LT_BITWISE_OR,
    R_GTE_BITWISE_OR,
    R_GT_BITWISE_OR,
    R_NOTIN_BITWISE_OR,
    R_IN_BITWISE_OR,
    R_ISNOT_BITWISE_OR,
    R_IS_BITWISE_OR,
    R_BITWISE_OR,
    R_BITWISE_XOR,
    R_BITWISE_AND,
    R_SHIFT_EXPR,
    R_SUM,
    R_TERM,
    R_FACTOR,
    R_POWER,
    R_AWAIT_PRIMARY,
    R_PRIMARY,
    R_SLICES,
    R_SLICE,
    R_ATOM,
    R_GROUP,
    R_LAMBDEF,
    R_LAMBDA_PARAMS,
    R_LAMBDA_PARAMETERS,
    R_LAMBDA_SLASH_NO_DEFAULT,
    R_LAMBDA_SLASH_WITH_DEFAULT,
    R_LAMBDA_STAR_ETC,
    R_LAMBDA_KWDS,
    R_LAMBDA_PARAM_NO_DEFAULT,
    R_LAMBDA_PARAM_WITH_DEFAULT,
    R_LAMBDA_PARAM_MAYBE_DEFAULT,
    R_LAMBDA_PARAM,
    R_STRINGS,
    R_LIST,
    R_TUPLE,
    R_SET,
    R_DICT,
    R_DOUBLE_STARRED_KVPAIRS,
    R_DOUBLE_STARRED_KVPAIR,
    R_KVPAIR,
    R_FOR_IF_CLAUSES,
    R_FOR_IF_CLAUSE,
    R_LISTCOMP,
    R_SETCOMP,
    R_GENEXP,
    R_DICTCOMP,
    R_ARGUMENTS,
    R_ARGS,
    R_KWARGS,
    R_STARRED_EXPRESSION,
    R_KWARG_OR_STARRED,
    R_KWARG_OR_DOUBLE_STARRED,
    R_STAR_TARGETS,
    R_STAR_TARGETS_LIST_SEQ,
    R_STAR_TARGETS_TUPLE_SEQ,
    R_STAR_TARGET,
    R_TARGET_WITH_STAR_ATOM,
    R_STAR_ATOM,
    R_SINGLE_TARGET,
    R_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET,
    R_T_PRIMARY,
    R_T_LOOKAHEAD,
    R_DEL_TARGETS,
    R_DEL_TARGET,
    R_DEL_T_ATOM,
    R_TYPE_EXPRESSIONS,
    R_FUNC_TYPE_COMMENT,
    R_ROOT,
    R__LOOP0_1,
    R__LOOP0_2,
    R__LOOP1_3,
    R__LOOP0_5,
    R__GATHER_4,
    R__TMP_6,
    R__TMP_7,
    R__TMP_8,
    R__TMP_9,
    R__TMP_10,
    R__TMP_11,
    R__TMP_12,
    R__TMP_13,
    R__LOOP1_14,
    R__TMP_15,
    R__TMP_16,
    R__TMP_17,
    R__LOOP0_19,
    R__GATHER_18,
    R__LOOP0_21,
    R__GATHER_20,
    R__TMP_22,
    R__TMP_23,
    R__LOOP0_24,
    R__LOOP1_25,
    R__LOOP0_27,
    R__GATHER_26,
    R__TMP_28,
    R__LOOP0_30,
    R__GATHER_29,
    R__TMP_31,
    R__LOOP1_32,
    R__TMP_33,
    R__TMP_34,
    R__TMP_35,
    R__LOOP0_36,
    R__LOOP0_37,
    R__LOOP0_38,
    R__LOOP1_39,
    R__LOOP0_40,
    R__LOOP1_41,
    R__LOOP1_42,
    R__LOOP1_43,
    R__LOOP0_44,
    R__LOOP1_45,
    R__LOOP0_46,
    R__LOOP1_47,
    R__LOOP0_48,
    R__LOOP0_49,
    R__LOOP1_50,
    R__LOOP0_52,
    R__GATHER_51,
    R__LOOP0_54,
    R__GATHER_53,
    R__LOOP0_56,
    R__GATHER_55,
    R__LOOP0_58,
    R__GATHER_57,
    R__TMP_59,
    R__LOOP1_60,
    R__LOOP1_61,
    R__TMP_62,
    R__TMP_63,
    R__LOOP1_64,
    R__LOOP0_66,
    R__GATHER_65,
    R__TMP_67,
    R__TMP_68,
    R__TMP_69,
    R__TMP_70,
    R__LOOP0_72,
    R__GATHER_71,
    R__LOOP0_74,
    R__GATHER_73,
    R__TMP_75,
    R__LOOP0_77,
    R__GATHER_76,
    R__LOOP0_79,
    R__GATHER_78,
    R__LOOP1_80,
    R__LOOP1_81,
    R__LOOP0_83,
    R__GATHER_82,
    R__LOOP1_84,
    R__LOOP1_85,
    R__LOOP1_86,
    R__TMP_87,
    R__LOOP0_89,
    R__GATHER_88,
    R__TMP_90,
    R__TMP_91,
    R__TMP_92,
    R__TMP_93,
    R__TMP_94,
    R__LOOP0_95,
    R__LOOP0_96,
    R__LOOP0_97,
    R__LOOP1_98,
    R__LOOP0_99,
    R__LOOP1_100,
    R__LOOP1_101,
    R__LOOP1_102,
    R__LOOP0_103,
    R__LOOP1_104,
    R__LOOP0_105,
    R__LOOP1_106,
    R__LOOP0_107,
    R__LOOP1_108,
    R__LOOP1_109,
    R__TMP_110,
    R__LOOP0_112,
    R__GATHER_111,
    R__LOOP1_113,
    R__LOOP0_114,
    R__LOOP0_115,
    R__TMP_116,
    R__LOOP0_118,
    R__GATHER_117,
    R__TMP_119,
    R__LOOP0_121,
    R__GATHER_120,
    R__LOOP0_123,
    R__GATHER_122,
    R__LOOP0_125,
    R__GATHER_124,
    R__LOOP0_127,
    R__GATHER_126,
    R__LOOP0_128,
    R__LOOP0_130,
    R__GATHER_129,
    R__LOOP1_131,
    R__TMP_132,
    R__LOOP0_134,
    R__GATHER_133,
    R__LOOP0_136,
    R__GATHER_135,
    R__LOOP0_138,
    R__GATHER_137,
    R__LOOP0_140,
    R__GATHER_139,
    R__LOOP0_142,
    R__GATHER_141,
    R__TMP_143,
    R__TMP_144,
    R__TMP_145,
    R__TMP_146,
    R__TMP_147,
    R__TMP_148,
    R__TMP_149,
    R__TMP_150,
    R__TMP_151,
    R__TMP_152,
    R__TMP_153,
    R__TMP_154,
    R__TMP_155,
    R__TMP_156,
    R__TMP_157,
    R__TMP_158,
};

enum {
    A_FILE_0,
    A_INTERACTIVE_0,
    A_EVAL_0,
    A_FUNC_TYPE_0,
    A_FSTRING_0,
    A_STATEMENTS_0,
    A_STATEMENT_0,
    A_STATEMENT_1,
    A_STATEMENT_NEWLINE_0,
    A_STATEMENT_NEWLINE_1,
    A_STATEMENT_NEWLINE_2,
    A_STATEMENT_NEWLINE_3,
    A_SIMPLE_STMTS_0,
    A_SIMPLE_STMTS_1,
    A_SIMPLE_STMT_0,
    A_SIMPLE_STMT_1,
    A_SIMPLE_STMT_2,
    A_SIMPLE_STMT_3,
    A_SIMPLE_STMT_4,
    A_SIMPLE_STMT_5,
    A_SIMPLE_STMT_6,
    A_SIMPLE_STMT_7,
    A_SIMPLE_STMT_8,
    A_SIMPLE_STMT_9,
    A_SIMPLE_STMT_10,
    A_SIMPLE_STMT_11,
    A_SIMPLE_STMT_12,
    A_COMPOUND_STMT_0,
    A_COMPOUND_STMT_1,
    A_COMPOUND_STMT_2,
    A_COMPOUND_STMT_3,
    A_COMPOUND_STMT_4,
    A_COMPOUND_STMT_5,
    A_COMPOUND_STMT_6,
    A_COMPOUND_STMT_7,
    A_ASSIGNMENT_0,
    A_ASSIGNMENT_1,
    A_ASSIGNMENT_2,
    A_ASSIGNMENT_3,
    A_ANNOTATED_RHS_0,
    A_ANNOTATED_RHS_1,
    A_AUGASSIGN_0,
    A_AUGASSIGN_1,
    A_AUGASSIGN_2,
    A_AUGASSIGN_3,
    A_AUGASSIGN_4,
    A_AUGASSIGN_5,
    A_AUGASSIGN_6,
    A_AUGASSIGN_7,
    A_AUGASSIGN_8,
    A_AUGASSIGN_9,
    A_AUGASSIGN_10,
    A_AUGASSIGN_11,
    A_AUGASSIGN_12,
    A_RETURN_STMT_0,
    A_RAISE_STMT_0,
    A_RAISE_STMT_1,
    A_GLOBAL_STMT_0,
    A_NONLOCAL_STMT_0,
    A_DEL_STMT_0,
    A_YIELD_STMT_0,
    A_ASSERT_STMT_0,
    A_IMPORT_STMT_0,
    A_IMPORT_STMT_1,
    A_IMPORT_NAME_0,
    A_IMPORT_FROM_0,
    A_IMPORT_FROM_1,
    A_IMPORT_FROM_TARGETS_0,
    A_IMPORT_FROM_TARGETS_1,
    A_IMPORT_FROM_TARGETS_2,
    A_IMPORT_FROM_AS_NAMES_0,
    A_IMPORT_FROM_AS_NAME_0,
    A_DOTTED_AS_NAMES_0,
    A_DOTTED_AS_NAME_0,
    A_DOTTED_NAME_0,
    A_DOTTED_NAME_1,
    A_BLOCK_0,
    A_BLOCK_1,
    A_DECORATORS_0,
    A_CLASS_DEF_0,
    A_CLASS_DEF_1,
    A_CLASS_DEF_RAW_0,
    A_FUNCTION_DEF_0,
    A_FUNCTION_DEF_1,
    A_FUNCTION_DEF_RAW_0,
    A_FUNCTION_DEF_RAW_1,
    A_PARAMS_0,
    A_PARAMETERS_0,
    A_PARAMETERS_1,
    A_PARAMETERS_2,
    A_PARAMETERS_3,
    A_PARAMETERS_4,
    A_SLASH_NO_DEFAULT_0,
    A_SLASH_NO_DEFAULT_1,
    A_SLASH_WITH_DEFAULT_0,
    A_SLASH_WITH_DEFAULT_1,
    A_STAR_ETC_0,
    A_STAR_ETC_1,
    A_STAR_ETC_2,
    A_STAR_ETC_3,
    A_KWDS_0,
    A_PARAM_NO_DEFAULT_0,
    A_PARAM_NO_DEFAULT_1,
    A_PARAM_NO_DEFAULT_STAR_ANNOTATION_0,
    A_PARAM_NO_DEFAULT_STAR_ANNOTATION_1,
    A_PARAM_WITH_DEFAULT_0,
    A_PARAM_WITH_DEFAULT_1,
    A_PARAM_MAYBE_DEFAULT_0,
    A_PARAM_MAYBE_DEFAULT_1,
    A_PARAM_0,
    A_PARAM_STAR_ANNOTATION_0,
    A_ANNOTATION_0,
    A_STAR_ANNOTATION_0,
    A_DEFAULT_0,
    A_IF_STMT_0,
    A_IF_STMT_1,
    A_ELIF_STMT_0,
    A_ELIF_STMT_1,
    A_ELSE_BLOCK_0,
    A_WHILE_STMT_0,
    A_FOR_STMT_0,
    A_FOR_STMT_1,
    A_WITH_STMT_0,
    A_WITH_STMT_1,
    A_WITH_STMT_2,
    A_WITH_STMT_3,
    A_WITH_ITEM_0,
    A_WITH_ITEM_1,
    A_TRY_STMT_0,
    A_TRY_STMT_1,
    A_TRY_STMT_2,
    A_EXCEPT_BLOCK_0,
    A_EXCEPT_BLOCK_1,
    A_EXCEPT_STAR_BLOCK_0,
    A_FINALLY_BLOCK_0,
    A_MATCH_STMT_0,
    A_SUBJECT_EXPR_0,
    A_SUBJECT_EXPR_1,
    A_CASE_BLOCK_0,
    A_GUARD_0,
    A_PATTERNS_0,
    A_PATTERNS_1,
    A_PATTERN_0,
    A_PATTERN_1,
    A_AS_PATTERN_0,
    A_OR_PATTERN_0,
    A_CLOSED_PATTERN_0,
    A_CLOSED_PATTERN_1,
    A_CLOSED_PATTERN_2,
    A_CLOSED_PATTERN_3,
    A_CLOSED_PATTERN_4,
    A_CLOSED_PATTERN_5,
    A_CLOSED_PATTERN_6,
    A_CLOSED_PATTERN_7,
    A_LITERAL_PATTERN_0,
    A_LITERAL_PATTERN_1,
    A_LITERAL_PATTERN_2,
    A_LITERAL_PATTERN_3,
    A_LITERAL_PATTERN_4,
    A_LITERAL_PATTERN_5,
    A_LITERAL_EXPR_0,
    A_LITERAL_EXPR_1,
    A_LITERAL_EXPR_2,
    A_LITERAL_EXPR_3,
    A_LITERAL_EXPR_4,
    A_LITERAL_EXPR_5,
    A_COMPLEX_NUMBER_0,
    A_COMPLEX_NUMBER_1,
    A_SIGNED_NUMBER_0,
    A_SIGNED_NUMBER_1,
    A_SIGNED_REAL_NUMBER_0,
    A_SIGNED_REAL_NUMBER_1,
    A_REAL_NUMBER_0,
    A_IMAGINARY_NUMBER_0,
    A_CAPTURE_PATTERN_0,
    A_PATTERN_CAPTURE_TARGET_0,
    A_WILDCARD_PATTERN_0,
    A_VALUE_PATTERN_0,
    A_ATTR_0,
    A_NAME_OR_ATTR_0,
    A_NAME_OR_ATTR_1,
    A_GROUP_PATTERN_0,
    A_SEQUENCE_PATTERN_0,
    A_SEQUENCE_PATTERN_1,
    A_OPEN_SEQUENCE_PATTERN_0,
    A_MAYBE_SEQUENCE_PATTERN_0,
    A_MAYBE_STAR_PATTERN_0,
    A_MAYBE_STAR_PATTERN_1,
    A_STAR_PATTERN_0,
    A_STAR_PATTERN_1,
    A_MAPPING_PATTERN_0,
    A_MAPPING_PATTERN_1,
    A_MAPPING_PATTERN_2,
    A_MAPPING_PATTERN_3,
    A_ITEMS_PATTERN_0,
    A_KEY_VALUE_PATTERN_0,
    A_DOUBLE_STAR_PATTERN_0,
    A_CLASS_PATTERN_0,
    A_CLASS_PATTERN_1,
    A_CLASS_PATTERN_2,
    A_CLASS_PATTERN_3,
    A_POSITIONAL_PATTERNS_0,
    A_KEYWORD_PATTERNS_0,
    A_KEYWORD_PATTERN_0,
    A_EXPRESSIONS_0,
    A_EXPRESSIONS_1,
    A_EXPRESSIONS_2,
    A_EXPRESSION_0,
    A_EXPRESSION_1,
    A_EXPRESSION_2,
    A_YIELD_EXPR_0,
    A_YIELD_EXPR_1,
    A_STAR_EXPRESSIONS_0,
    A_STAR_EXPRESSIONS_1,
    A_STAR_EXPRESSIONS_2,
    A_STAR_EXPRESSION_0,
    A_STAR_EXPRESSION_1,
    A_STAR_NAMED_EXPRESSIONS_0,
    A_STAR_NAMED_EXPRESSION_0,
    A_STAR_NAMED_EXPRESSION_1,
    A_ASSIGNMENT_EXPRESSION_0,
    A_NAMED_EXPRESSION_0,
    A_NAMED_EXPRESSION_1,
    A_DISJUNCTION_0,
    A_DISJUNCTION_1,
    A_CONJUNCTION_0,
    A_CONJUNCTION_1,
    A_INVERSION_0,
    A_INVERSION_1,
    A_COMPARISON_0,
    A_COMPARISON_1,
    A_COMPARE_OP_BITWISE_OR_PAIR_0,
    A_COMPARE_OP_BITWISE_OR_PAIR_1,
    A_COMPARE_OP_BITWISE_OR_PAIR_2,
    A_COMPARE_OP_BITWISE_OR_PAIR_3,
    A_COMPARE_OP_BITWISE_OR_PAIR_4,
    A_COMPARE_OP_BITWISE_OR_PAIR_5,
    A_COMPARE_OP_BITWISE_OR_PAIR_6,
    A_COMPARE_OP_BITWISE_OR_PAIR_7,
    A_COMPARE_OP_BITWISE_OR_PAIR_8,
    A_COMPARE_OP_BITWISE_OR_PAIR_9,
    A_EQ_BITWISE_OR_0,
    A_NOTEQ_BITWISE_OR_0,
    A_LTE_BITWISE_OR_0,
    A_LT_BITWISE_OR_0,
    A_GTE_BITWISE_OR_0,
    A_GT_BITWISE_OR_0,
    A_NOTIN_BITWISE_OR_0,
    A_IN_BITWISE_OR_0,
    A_ISNOT_BITWISE_OR_0,
    A_IS_BITWISE_OR_0,
    A_BITWISE_OR_0,
    A_BITWISE_OR_1,
    A_BITWISE_XOR_0,
    A_BITWISE_XOR_1,
    A_BITWISE_AND_0,
    A_BITWISE_AND_1,
    A_SHIFT_EXPR_0,
    A_SHIFT_EXPR_1,
    A_SHIFT_EXPR_2,
    A_SUM_0,
    A_SUM_1,
    A_SUM_2,
    A_TERM_0,
    A_TERM_1,
    A_TERM_2,
    A_TERM_3,
    A_TERM_4,
    A_TERM_5,
    A_FACTOR_0,
    A_FACTOR_1,
    A_FACTOR_2,
    A_FACTOR_3,
    A_POWER_0,
    A_POWER_1,
    A_AWAIT_PRIMARY_0,
    A_AWAIT_PRIMARY_1,
    A_PRIMARY_0,
    A_PRIMARY_1,
    A_PRIMARY_2,
    A_PRIMARY_3,
    A_PRIMARY_4,
    A_SLICES_0,
    A_SLICES_1,
    A_SLICE_0,
    A_SLICE_1,
    A_ATOM_0,
    A_ATOM_1,
    A_ATOM_2,
    A_ATOM_3,
    A_ATOM_4,
    A_ATOM_5,
    A_ATOM_6,
    A_ATOM_7,
    A_ATOM_8,
    A_ATOM_9,
    A_GROUP_0,
    A_LAMBDEF_0,
    A_LAMBDA_PARAMS_0,
    A_LAMBDA_PARAMETERS_0,
    A_LAMBDA_PARAMETERS_1,
    A_LAMBDA_PARAMETERS_2,
    A_LAMBDA_PARAMETERS_3,
    A_LAMBDA_PARAMETERS_4,
    A_LAMBDA_SLASH_NO_DEFAULT_0,
    A_LAMBDA_SLASH_NO_DEFAULT_1,
    A_LAMBDA_SLASH_WITH_DEFAULT_0,
    A_LAMBDA_SLASH_WITH_DEFAULT_1,
    A_LAMBDA_STAR_ETC_0,
    A_LAMBDA_STAR_ETC_1,
    A_LAMBDA_STAR_ETC_2,
    A_LAMBDA_KWDS_0,
    A_LAMBDA_PARAM_NO_DEFAULT_0,
    A_LAMBDA_PARAM_NO_DEFAULT_1,
    A_LAMBDA_PARAM_WITH_DEFAULT_0,
    A_LAMBDA_PARAM_WITH_DEFAULT_1,
    A_LAMBDA_PARAM_MAYBE_DEFAULT_0,
    A_LAMBDA_PARAM_MAYBE_DEFAULT_1,
    A_LAMBDA_PARAM_0,
    A_STRINGS_0,
    A_LIST_0,
    A_TUPLE_0,
    A_SET_0,
    A_DICT_0,
    A_DOUBLE_STARRED_KVPAIRS_0,
    A_DOUBLE_STARRED_KVPAIR_0,
    A_DOUBLE_STARRED_KVPAIR_1,
    A_KVPAIR_0,
    A_FOR_IF_CLAUSES_0,
    A_FOR_IF_CLAUSE_0,
    A_FOR_IF_CLAUSE_1,
    A_LISTCOMP_0,
    A_SETCOMP_0,
    A_GENEXP_0,
    A_DICTCOMP_0,
    A_ARGUMENTS_0,
    A_ARGS_0,
    A_ARGS_1,
    A_KWARGS_0,
    A_KWARGS_1,
    A_KWARGS_2,
    A_STARRED_EXPRESSION_0,
    A_KWARG_OR_STARRED_0,
    A_KWARG_OR_STARRED_1,
    A_KWARG_OR_DOUBLE_STARRED_0,
    A_KWARG_OR_DOUBLE_STARRED_1,
    A_STAR_TARGETS_0,
    A_STAR_TARGETS_1,
    A_STAR_TARGETS_LIST_SEQ_0,
    A_STAR_TARGETS_TUPLE_SEQ_0,
    A_STAR_TARGETS_TUPLE_SEQ_1,
    A_STAR_TARGET_0,
    A_STAR_TARGET_1,
    A_TARGET_WITH_STAR_ATOM_0,
    A_TARGET_WITH_STAR_ATOM_1,
    A_TARGET_WITH_STAR_ATOM_2,
    A_STAR_ATOM_0,
    A_STAR_ATOM_1,
    A_STAR_ATOM_2,
    A_STAR_ATOM_3,
    A_SINGLE_TARGET_0,
    A_SINGLE_TARGET_1,
    A_SINGLE_TARGET_2,
    A_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET_0,
    A_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET_1,
    A_T_PRIMARY_0,
    A_T_PRIMARY_1,
    A_T_PRIMARY_2,
    A_T_PRIMARY_3,
    A_T_PRIMARY_4,
    A_T_LOOKAHEAD_0,
    A_T_LOOKAHEAD_1,
    A_T_LOOKAHEAD_2,
    A_DEL_TARGETS_0,
    A_DEL_TARGET_0,
    A_DEL_TARGET_1,
    A_DEL_TARGET_2,
    A_DEL_T_ATOM_0,
    A_DEL_T_ATOM_1,
    A_DEL_T_ATOM_2,
    A_DEL_T_ATOM_3,
    A_TYPE_EXPRESSIONS_0,
    A_TYPE_EXPRESSIONS_1,
    A_TYPE_EXPRESSIONS_2,
    A_TYPE_EXPRESSIONS_3,
    A_TYPE_EXPRESSIONS_4,
    A_TYPE_EXPRESSIONS_5,
    A_TYPE_EXPRESSIONS_6,
    A_FUNC_TYPE_COMMENT_0,
    A_FUNC_TYPE_COMMENT_1,
    A__GATHER_4_0,
    A__TMP_6_0,
    A__TMP_6_1,
    A__TMP_7_0,
    A__TMP_7_1,
    A__TMP_7_2,
    A__TMP_8_0,
    A__TMP_8_1,
    A__TMP_9_0,
    A__TMP_9_1,
    A__TMP_10_0,
    A__TMP_10_1,
    A__TMP_11_0,
    A__TMP_12_0,
    A__TMP_12_1,
    A__TMP_13_0,
    A__TMP_15_0,
    A__TMP_15_1,
    A__TMP_16_0,
    A__TMP_16_1,
    A__TMP_17_0,
    A__GATHER_18_0,
    A__GATHER_20_0,
    A__TMP_22_0,
    A__TMP_22_1,
    A__TMP_23_0,
    A__GATHER_26_0,
    A__TMP_28_0,
    A__GATHER_29_0,
    A__TMP_31_0,
    A__TMP_33_0,
    A__TMP_34_0,
    A__TMP_35_0,
    A__GATHER_51_0,
    A__GATHER_53_0,
    A__GATHER_55_0,
    A__GATHER_57_0,
    A__TMP_59_0,
    A__TMP_59_1,
    A__TMP_59_2,
    A__TMP_62_0,
    A__TMP_63_0,
    A__GATHER_65_0,
    A__TMP_67_0,
    A__TMP_67_1,
    A__TMP_68_0,
    A__TMP_68_1,
    A__TMP_69_0,
    A__TMP_69_1,
    A__TMP_69_2,
    A__TMP_70_0,
    A__TMP_70_1,
    A__TMP_70_2,
    A__GATHER_71_0,
    A__GATHER_73_0,
    A__TMP_75_0,
    A__TMP_75_1,
    A__GATHER_76_0,
    A__GATHER_78_0,
    A__GATHER_82_0,
    A__TMP_87_0,
    A__GATHER_88_0,
    A__TMP_90_0,
    A__TMP_91_0,
    A__TMP_91_1,
    A__TMP_91_2,
    A__TMP_92_0,
    A__TMP_92_1,
    A__TMP_93_0,
    A__TMP_93_1,
    A__TMP_93_2,
    A__TMP_93_3,
    A__TMP_94_0,
    A__TMP_94_1,
    A__TMP_110_0,
    A__GATHER_111_0,
    A__TMP_116_0,
    A__TMP_116_1,
    A__GATHER_117_0,
    A__TMP_119_0,
    A__GATHER_120_0,
    A__GATHER_122_0,
    A__GATHER_124_0,
    A__GATHER_126_0,
    A__GATHER_129_0,
    A__TMP_132_0,
    A__GATHER_133_0,
    A__GATHER_135_0,
    A__GATHER_137_0,
    A__GATHER_139_0,
    A__GATHER_141_0,
    A__TMP_143_0,
    A__TMP_144_0,
    A__TMP_145_0,
    A__TMP_145_1,
    A__TMP_146_0,
    A__TMP_146_1,
    A__TMP_147_0,
    A__TMP_148_0,
    A__TMP_149_0,
    A__TMP_150_0,
    A__TMP_151_0,
    A__TMP_152_0,
    A__TMP_152_1,
    A__TMP_153_0,
    A__TMP_154_0,
    A__TMP_155_0,
    A__TMP_155_1,
    A__TMP_156_0,
    A__TMP_157_0,
    A__TMP_158_0,
    A__TMP_158_1,
};

static Rule all_rules[] = {
{"file",
 R_FILE,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // statements? $
    OP_RULE, R_STATEMENTS,
    OP_OPTIONAL, 
    OP_TOKEN, ENDMARKER,
    OP_RETURN, A_FILE_0,

 },
},
{"interactive",
 R_INTERACTIVE,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // statement_newline
    OP_RULE, R_STATEMENT_NEWLINE,
    OP_RETURN, A_INTERACTIVE_0,

 },
},
{"eval",
 R_EVAL,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // expressions NEWLINE* $
    OP_RULE, R_EXPRESSIONS,
    OP_RULE, R__LOOP0_1,
    OP_TOKEN, ENDMARKER,
    OP_RETURN, A_EVAL_0,

 },
},
{"func_type",
 R_FUNC_TYPE,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '(' type_expressions? ')' '->' expression NEWLINE* $
    OP_TOKEN, LPAR,
    OP_RULE, R_TYPE_EXPRESSIONS,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_TOKEN, RARROW,
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__LOOP0_2,
    OP_TOKEN, ENDMARKER,
    OP_RETURN, A_FUNC_TYPE_0,

 },
},
{"fstring",
 R_FSTRING,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // star_expressions
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_RETURN, A_FSTRING_0,

 },
},
{"statements",
 R_STATEMENTS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // statement+
    OP_RULE, R__LOOP1_3,
    OP_RETURN, A_STATEMENTS_0,

 },
},
{"statement",
 R_STATEMENT,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // compound_stmt
    OP_RULE, R_COMPOUND_STMT,
    OP_RETURN, A_STATEMENT_0,

    // simple_stmts
    OP_RULE, R_SIMPLE_STMTS,
    OP_RETURN, A_STATEMENT_1,

 },
},
{"statement_newline",
 R_STATEMENT_NEWLINE,
 0,  // memo
 0,  // leftrec
 {0, 6, 10, 14, -1},
 {
    // compound_stmt NEWLINE
    OP_RULE, R_COMPOUND_STMT,
    OP_TOKEN, NEWLINE,
    OP_RETURN, A_STATEMENT_NEWLINE_0,

    // simple_stmts
    OP_RULE, R_SIMPLE_STMTS,
    OP_RETURN, A_STATEMENT_NEWLINE_1,

    // NEWLINE
    OP_TOKEN, NEWLINE,
    OP_RETURN, A_STATEMENT_NEWLINE_2,

    // $
    OP_TOKEN, ENDMARKER,
    OP_RETURN, A_STATEMENT_NEWLINE_3,

 },
},
{"simple_stmts",
 R_SIMPLE_STMTS,
 0,  // memo
 0,  // leftrec
 {0, 10, -1},
 {
    // simple_stmt !';' NEWLINE
    OP_RULE, R_SIMPLE_STMT,
    OP_SAVE_MARK, 
    OP_TOKEN, SEMI,
    OP_NEG_LOOKAHEAD, 
    OP_TOKEN, NEWLINE,
    OP_RETURN, A_SIMPLE_STMTS_0,

    // ';'.simple_stmt+ ';'? NEWLINE
    OP_RULE, R__GATHER_4,
    OP_TOKEN, SEMI,
    OP_OPTIONAL, 
    OP_TOKEN, NEWLINE,
    OP_RETURN, A_SIMPLE_STMTS_1,

 },
},
{"simple_stmt",
 R_SIMPLE_STMT,
 1,  // memo
 0,  // leftrec
 {0, 4, 8, 16, 24, 32, 36, 44, 52, 60, 64, 68, 76, -1},
 {
    // assignment
    OP_RULE, R_ASSIGNMENT,
    OP_RETURN, A_SIMPLE_STMT_0,

    // star_expressions
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_RETURN, A_SIMPLE_STMT_1,

    // &'return' return_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 593,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_RETURN_STMT,
    OP_RETURN, A_SIMPLE_STMT_2,

    // &('import' | 'from') import_stmt
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_6,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_IMPORT_STMT,
    OP_RETURN, A_SIMPLE_STMT_3,

    // &'raise' raise_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 594,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_RAISE_STMT,
    OP_RETURN, A_SIMPLE_STMT_4,

    // 'pass'
    OP_TOKEN, 595,
    OP_RETURN, A_SIMPLE_STMT_5,

    // &'del' del_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 596,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_DEL_STMT,
    OP_RETURN, A_SIMPLE_STMT_6,

    // &'yield' yield_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 597,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_YIELD_STMT,
    OP_RETURN, A_SIMPLE_STMT_7,

    // &'assert' assert_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 598,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_ASSERT_STMT,
    OP_RETURN, A_SIMPLE_STMT_8,

    // 'break'
    OP_TOKEN, 599,
    OP_RETURN, A_SIMPLE_STMT_9,

    // 'continue'
    OP_TOKEN, 600,
    OP_RETURN, A_SIMPLE_STMT_10,

    // &'global' global_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 601,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_GLOBAL_STMT,
    OP_RETURN, A_SIMPLE_STMT_11,

    // &'nonlocal' nonlocal_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 602,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_NONLOCAL_STMT,
    OP_RETURN, A_SIMPLE_STMT_12,

 },
},
{"compound_stmt",
 R_COMPOUND_STMT,
 0,  // memo
 0,  // leftrec
 {0, 8, 16, 24, 32, 40, 48, 56, -1},
 {
    // &('def' | '@' | ASYNC) function_def
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_7,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_FUNCTION_DEF,
    OP_RETURN, A_COMPOUND_STMT_0,

    // &'if' if_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 603,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_IF_STMT,
    OP_RETURN, A_COMPOUND_STMT_1,

    // &('class' | '@') class_def
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_8,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_CLASS_DEF,
    OP_RETURN, A_COMPOUND_STMT_2,

    // &('with' | ASYNC) with_stmt
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_9,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_WITH_STMT,
    OP_RETURN, A_COMPOUND_STMT_3,

    // &('for' | ASYNC) for_stmt
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_10,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_FOR_STMT,
    OP_RETURN, A_COMPOUND_STMT_4,

    // &'try' try_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 604,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_TRY_STMT,
    OP_RETURN, A_COMPOUND_STMT_5,

    // &'while' while_stmt
    OP_SAVE_MARK, 
    OP_TOKEN, 605,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_WHILE_STMT,
    OP_RETURN, A_COMPOUND_STMT_6,

    // match_stmt
    OP_RULE, R_MATCH_STMT,
    OP_RETURN, A_COMPOUND_STMT_7,

 },
},
{"assignment",
 R_ASSIGNMENT,
 0,  // memo
 0,  // leftrec
 {0, 10, 21, 34, -1},
 {
    // NAME ':' expression ['=' annotated_rhs]
    OP_NAME, 
    OP_TOKEN, COLON,
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__TMP_11,
    OP_OPTIONAL, 
    OP_RETURN, A_ASSIGNMENT_0,

    // ('(' single_target ')' | single_subscript_attribute_target) ':' expression ['=' annotated_rhs]
    OP_RULE, R__TMP_12,
    OP_TOKEN, COLON,
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__TMP_13,
    OP_OPTIONAL, 
    OP_RETURN, A_ASSIGNMENT_1,

    // ((star_targets '='))+ (yield_expr | star_expressions) !'=' TYPE_COMMENT?
    OP_RULE, R__LOOP1_14,
    OP_RULE, R__TMP_15,
    OP_SAVE_MARK, 
    OP_TOKEN, EQUAL,
    OP_NEG_LOOKAHEAD, 
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RETURN, A_ASSIGNMENT_2,

    // single_target augassign ~ (yield_expr | star_expressions)
    OP_RULE, R_SINGLE_TARGET,
    OP_RULE, R_AUGASSIGN,
    OP_CUT, 
    OP_RULE, R__TMP_16,
    OP_RETURN, A_ASSIGNMENT_3,

 },
},
{"annotated_rhs",
 R_ANNOTATED_RHS,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // yield_expr
    OP_RULE, R_YIELD_EXPR,
    OP_RETURN, A_ANNOTATED_RHS_0,

    // star_expressions
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_RETURN, A_ANNOTATED_RHS_1,

 },
},
{"augassign",
 R_AUGASSIGN,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, -1},
 {
    // '+='
    OP_TOKEN, PLUSEQUAL,
    OP_RETURN, A_AUGASSIGN_0,

    // '-='
    OP_TOKEN, MINEQUAL,
    OP_RETURN, A_AUGASSIGN_1,

    // '*='
    OP_TOKEN, STAREQUAL,
    OP_RETURN, A_AUGASSIGN_2,

    // '@='
    OP_TOKEN, ATEQUAL,
    OP_RETURN, A_AUGASSIGN_3,

    // '/='
    OP_TOKEN, SLASHEQUAL,
    OP_RETURN, A_AUGASSIGN_4,

    // '%='
    OP_TOKEN, PERCENTEQUAL,
    OP_RETURN, A_AUGASSIGN_5,

    // '&='
    OP_TOKEN, AMPEREQUAL,
    OP_RETURN, A_AUGASSIGN_6,

    // '|='
    OP_TOKEN, VBAREQUAL,
    OP_RETURN, A_AUGASSIGN_7,

    // '^='
    OP_TOKEN, CIRCUMFLEXEQUAL,
    OP_RETURN, A_AUGASSIGN_8,

    // '<<='
    OP_TOKEN, LEFTSHIFTEQUAL,
    OP_RETURN, A_AUGASSIGN_9,

    // '>>='
    OP_TOKEN, RIGHTSHIFTEQUAL,
    OP_RETURN, A_AUGASSIGN_10,

    // '**='
    OP_TOKEN, DOUBLESTAREQUAL,
    OP_RETURN, A_AUGASSIGN_11,

    // '//='
    OP_TOKEN, DOUBLESLASHEQUAL,
    OP_RETURN, A_AUGASSIGN_12,

 },
},
{"return_stmt",
 R_RETURN_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'return' star_expressions?
    OP_TOKEN, 593,
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_OPTIONAL, 
    OP_RETURN, A_RETURN_STMT_0,

 },
},
{"raise_stmt",
 R_RAISE_STMT,
 0,  // memo
 0,  // leftrec
 {0, 9, -1},
 {
    // 'raise' expression ['from' expression]
    OP_TOKEN, 594,
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__TMP_17,
    OP_OPTIONAL, 
    OP_RETURN, A_RAISE_STMT_0,

    // 'raise'
    OP_TOKEN, 594,
    OP_RETURN, A_RAISE_STMT_1,

 },
},
{"global_stmt",
 R_GLOBAL_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'global' ','.NAME+
    OP_TOKEN, 601,
    OP_RULE, R__GATHER_18,
    OP_RETURN, A_GLOBAL_STMT_0,

 },
},
{"nonlocal_stmt",
 R_NONLOCAL_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'nonlocal' ','.NAME+
    OP_TOKEN, 602,
    OP_RULE, R__GATHER_20,
    OP_RETURN, A_NONLOCAL_STMT_0,

 },
},
{"del_stmt",
 R_DEL_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'del' del_targets &(';' | NEWLINE)
    OP_TOKEN, 596,
    OP_RULE, R_DEL_TARGETS,
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_22,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_DEL_STMT_0,

 },
},
{"yield_stmt",
 R_YIELD_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // yield_expr
    OP_RULE, R_YIELD_EXPR,
    OP_RETURN, A_YIELD_STMT_0,

 },
},
{"assert_stmt",
 R_ASSERT_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'assert' expression [',' expression]
    OP_TOKEN, 598,
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__TMP_23,
    OP_OPTIONAL, 
    OP_RETURN, A_ASSERT_STMT_0,

 },
},
{"import_stmt",
 R_IMPORT_STMT,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // import_name
    OP_RULE, R_IMPORT_NAME,
    OP_RETURN, A_IMPORT_STMT_0,

    // import_from
    OP_RULE, R_IMPORT_FROM,
    OP_RETURN, A_IMPORT_STMT_1,

 },
},
{"import_name",
 R_IMPORT_NAME,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'import' dotted_as_names
    OP_TOKEN, 606,
    OP_RULE, R_DOTTED_AS_NAMES,
    OP_RETURN, A_IMPORT_NAME_0,

 },
},
{"import_from",
 R_IMPORT_FROM,
 0,  // memo
 0,  // leftrec
 {0, 12, -1},
 {
    // 'from' (('.' | '...'))* dotted_name 'import' import_from_targets
    OP_TOKEN, 607,
    OP_RULE, R__LOOP0_24,
    OP_RULE, R_DOTTED_NAME,
    OP_TOKEN, 606,
    OP_RULE, R_IMPORT_FROM_TARGETS,
    OP_RETURN, A_IMPORT_FROM_0,

    // 'from' (('.' | '...'))+ 'import' import_from_targets
    OP_TOKEN, 607,
    OP_RULE, R__LOOP1_25,
    OP_TOKEN, 606,
    OP_RULE, R_IMPORT_FROM_TARGETS,
    OP_RETURN, A_IMPORT_FROM_1,

 },
},
{"import_from_targets",
 R_IMPORT_FROM_TARGETS,
 0,  // memo
 0,  // leftrec
 {0, 11, 19, -1},
 {
    // '(' import_from_as_names ','? ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_IMPORT_FROM_AS_NAMES,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_IMPORT_FROM_TARGETS_0,

    // import_from_as_names !','
    OP_RULE, R_IMPORT_FROM_AS_NAMES,
    OP_SAVE_MARK, 
    OP_TOKEN, COMMA,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_IMPORT_FROM_TARGETS_1,

    // '*'
    OP_TOKEN, STAR,
    OP_RETURN, A_IMPORT_FROM_TARGETS_2,

 },
},
{"import_from_as_names",
 R_IMPORT_FROM_AS_NAMES,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.import_from_as_name+
    OP_RULE, R__GATHER_26,
    OP_RETURN, A_IMPORT_FROM_AS_NAMES_0,

 },
},
{"import_from_as_name",
 R_IMPORT_FROM_AS_NAME,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME ['as' NAME]
    OP_NAME, 
    OP_RULE, R__TMP_28,
    OP_OPTIONAL, 
    OP_RETURN, A_IMPORT_FROM_AS_NAME_0,

 },
},
{"dotted_as_names",
 R_DOTTED_AS_NAMES,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.dotted_as_name+
    OP_RULE, R__GATHER_29,
    OP_RETURN, A_DOTTED_AS_NAMES_0,

 },
},
{"dotted_as_name",
 R_DOTTED_AS_NAME,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // dotted_name ['as' NAME]
    OP_RULE, R_DOTTED_NAME,
    OP_RULE, R__TMP_31,
    OP_OPTIONAL, 
    OP_RETURN, A_DOTTED_AS_NAME_0,

 },
},
{"dotted_name",
 R_DOTTED_NAME,
 0,  // memo
 1,  // leftrec
 {0, 7, -1},
 {
    // dotted_name '.' NAME
    OP_RULE, R_DOTTED_NAME,
    OP_TOKEN, DOT,
    OP_NAME, 
    OP_RETURN, A_DOTTED_NAME_0,

    // NAME
    OP_NAME, 
    OP_RETURN, A_DOTTED_NAME_1,

 },
},
{"block",
 R_BLOCK,
 1,  // memo
 0,  // leftrec
 {0, 10, -1},
 {
    // NEWLINE INDENT statements DEDENT
    OP_TOKEN, NEWLINE,
    OP_TOKEN, INDENT,
    OP_RULE, R_STATEMENTS,
    OP_TOKEN, DEDENT,
    OP_RETURN, A_BLOCK_0,

    // simple_stmts
    OP_RULE, R_SIMPLE_STMTS,
    OP_RETURN, A_BLOCK_1,

 },
},
{"decorators",
 R_DECORATORS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // (('@' named_expression NEWLINE))+
    OP_RULE, R__LOOP1_32,
    OP_RETURN, A_DECORATORS_0,

 },
},
{"class_def",
 R_CLASS_DEF,
 0,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // decorators class_def_raw
    OP_RULE, R_DECORATORS,
    OP_RULE, R_CLASS_DEF_RAW,
    OP_RETURN, A_CLASS_DEF_0,

    // class_def_raw
    OP_RULE, R_CLASS_DEF_RAW,
    OP_RETURN, A_CLASS_DEF_1,

 },
},
{"class_def_raw",
 R_CLASS_DEF_RAW,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'class' NAME ['(' arguments? ')'] ':' block
    OP_TOKEN, 608,
    OP_NAME, 
    OP_RULE, R__TMP_33,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_CLASS_DEF_RAW_0,

 },
},
{"function_def",
 R_FUNCTION_DEF,
 0,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // decorators function_def_raw
    OP_RULE, R_DECORATORS,
    OP_RULE, R_FUNCTION_DEF_RAW,
    OP_RETURN, A_FUNCTION_DEF_0,

    // function_def_raw
    OP_RULE, R_FUNCTION_DEF_RAW,
    OP_RETURN, A_FUNCTION_DEF_1,

 },
},
{"function_def_raw",
 R_FUNCTION_DEF_RAW,
 0,  // memo
 0,  // leftrec
 {0, 22, -1},
 {
    // 'def' NAME &&'(' params? ')' ['->' expression] &&':' func_type_comment? block
    OP_TOKEN, 609,
    OP_NAME, 
    OP_TOKEN, LPAR,
    OP_RULE, R_PARAMS,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RULE, R__TMP_34,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_FUNC_TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RULE, R_BLOCK,
    OP_RETURN, A_FUNCTION_DEF_RAW_0,

    // ASYNC 'def' NAME &&'(' params? ')' ['->' expression] &&':' func_type_comment? block
    OP_TOKEN, ASYNC,
    OP_TOKEN, 609,
    OP_NAME, 
    OP_TOKEN, LPAR,
    OP_RULE, R_PARAMS,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RULE, R__TMP_35,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_FUNC_TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RULE, R_BLOCK,
    OP_RETURN, A_FUNCTION_DEF_RAW_1,

 },
},
{"params",
 R_PARAMS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // parameters
    OP_RULE, R_PARAMETERS,
    OP_RETURN, A_PARAMS_0,

 },
},
{"parameters",
 R_PARAMETERS,
 0,  // memo
 0,  // leftrec
 {0, 11, 20, 29, 36, -1},
 {
    // slash_no_default param_no_default* param_with_default* star_etc?
    OP_RULE, R_SLASH_NO_DEFAULT,
    OP_RULE, R__LOOP0_36,
    OP_RULE, R__LOOP0_37,
    OP_RULE, R_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAMETERS_0,

    // slash_with_default param_with_default* star_etc?
    OP_RULE, R_SLASH_WITH_DEFAULT,
    OP_RULE, R__LOOP0_38,
    OP_RULE, R_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAMETERS_1,

    // param_no_default+ param_with_default* star_etc?
    OP_RULE, R__LOOP1_39,
    OP_RULE, R__LOOP0_40,
    OP_RULE, R_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAMETERS_2,

    // param_with_default+ star_etc?
    OP_RULE, R__LOOP1_41,
    OP_RULE, R_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAMETERS_3,

    // star_etc
    OP_RULE, R_STAR_ETC,
    OP_RETURN, A_PARAMETERS_4,

 },
},
{"slash_no_default",
 R_SLASH_NO_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // param_no_default+ '/' ','
    OP_RULE, R__LOOP1_42,
    OP_TOKEN, SLASH,
    OP_TOKEN, COMMA,
    OP_RETURN, A_SLASH_NO_DEFAULT_0,

    // param_no_default+ '/' &')'
    OP_RULE, R__LOOP1_43,
    OP_TOKEN, SLASH,
    OP_SAVE_MARK, 
    OP_TOKEN, RPAR,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_SLASH_NO_DEFAULT_1,

 },
},
{"slash_with_default",
 R_SLASH_WITH_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 10, -1},
 {
    // param_no_default* param_with_default+ '/' ','
    OP_RULE, R__LOOP0_44,
    OP_RULE, R__LOOP1_45,
    OP_TOKEN, SLASH,
    OP_TOKEN, COMMA,
    OP_RETURN, A_SLASH_WITH_DEFAULT_0,

    // param_no_default* param_with_default+ '/' &')'
    OP_RULE, R__LOOP0_46,
    OP_RULE, R__LOOP1_47,
    OP_TOKEN, SLASH,
    OP_SAVE_MARK, 
    OP_TOKEN, RPAR,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_SLASH_WITH_DEFAULT_1,

 },
},
{"star_etc",
 R_STAR_ETC,
 0,  // memo
 0,  // leftrec
 {0, 11, 22, 33, -1},
 {
    // '*' param_no_default param_maybe_default* kwds?
    OP_TOKEN, STAR,
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_RULE, R__LOOP0_48,
    OP_RULE, R_KWDS,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_ETC_0,

    // '*' param_no_default_star_annotation param_maybe_default* kwds?
    OP_TOKEN, STAR,
    OP_RULE, R_PARAM_NO_DEFAULT_STAR_ANNOTATION,
    OP_RULE, R__LOOP0_49,
    OP_RULE, R_KWDS,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_ETC_1,

    // '*' ',' param_maybe_default+ kwds?
    OP_TOKEN, STAR,
    OP_TOKEN, COMMA,
    OP_RULE, R__LOOP1_50,
    OP_RULE, R_KWDS,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_ETC_2,

    // kwds
    OP_RULE, R_KWDS,
    OP_RETURN, A_STAR_ETC_3,

 },
},
{"kwds",
 R_KWDS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '**' param_no_default
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_RETURN, A_KWDS_0,

 },
},
{"param_no_default",
 R_PARAM_NO_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 9, -1},
 {
    // param ',' TYPE_COMMENT?
    OP_RULE, R_PARAM,
    OP_TOKEN, COMMA,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAM_NO_DEFAULT_0,

    // param TYPE_COMMENT? &')'
    OP_RULE, R_PARAM,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_SAVE_MARK, 
    OP_TOKEN, RPAR,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_PARAM_NO_DEFAULT_1,

 },
},
{"param_no_default_star_annotation",
 R_PARAM_NO_DEFAULT_STAR_ANNOTATION,
 0,  // memo
 0,  // leftrec
 {0, 9, -1},
 {
    // param_star_annotation ',' TYPE_COMMENT?
    OP_RULE, R_PARAM_STAR_ANNOTATION,
    OP_TOKEN, COMMA,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAM_NO_DEFAULT_STAR_ANNOTATION_0,

    // param_star_annotation TYPE_COMMENT? &')'
    OP_RULE, R_PARAM_STAR_ANNOTATION,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_SAVE_MARK, 
    OP_TOKEN, RPAR,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_PARAM_NO_DEFAULT_STAR_ANNOTATION_1,

 },
},
{"param_with_default",
 R_PARAM_WITH_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 11, -1},
 {
    // param default ',' TYPE_COMMENT?
    OP_RULE, R_PARAM,
    OP_RULE, R_DEFAULT,
    OP_TOKEN, COMMA,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAM_WITH_DEFAULT_0,

    // param default TYPE_COMMENT? &')'
    OP_RULE, R_PARAM,
    OP_RULE, R_DEFAULT,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_SAVE_MARK, 
    OP_TOKEN, RPAR,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_PARAM_WITH_DEFAULT_1,

 },
},
{"param_maybe_default",
 R_PARAM_MAYBE_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 12, -1},
 {
    // param default? ',' TYPE_COMMENT?
    OP_RULE, R_PARAM,
    OP_RULE, R_DEFAULT,
    OP_OPTIONAL, 
    OP_TOKEN, COMMA,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAM_MAYBE_DEFAULT_0,

    // param default? TYPE_COMMENT? &')'
    OP_RULE, R_PARAM,
    OP_RULE, R_DEFAULT,
    OP_OPTIONAL, 
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_SAVE_MARK, 
    OP_TOKEN, RPAR,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_PARAM_MAYBE_DEFAULT_1,

 },
},
{"param",
 R_PARAM,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME annotation?
    OP_NAME, 
    OP_RULE, R_ANNOTATION,
    OP_OPTIONAL, 
    OP_RETURN, A_PARAM_0,

 },
},
{"param_star_annotation",
 R_PARAM_STAR_ANNOTATION,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME star_annotation
    OP_NAME, 
    OP_RULE, R_STAR_ANNOTATION,
    OP_RETURN, A_PARAM_STAR_ANNOTATION_0,

 },
},
{"annotation",
 R_ANNOTATION,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ':' expression
    OP_TOKEN, COLON,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_ANNOTATION_0,

 },
},
{"star_annotation",
 R_STAR_ANNOTATION,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ':' star_expression
    OP_TOKEN, COLON,
    OP_RULE, R_STAR_EXPRESSION,
    OP_RETURN, A_STAR_ANNOTATION_0,

 },
},
{"default",
 R_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '=' expression
    OP_TOKEN, EQUAL,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_DEFAULT_0,

 },
},
{"if_stmt",
 R_IF_STMT,
 0,  // memo
 0,  // leftrec
 {0, 12, -1},
 {
    // 'if' named_expression ':' block elif_stmt
    OP_TOKEN, 603,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R_ELIF_STMT,
    OP_RETURN, A_IF_STMT_0,

    // 'if' named_expression ':' block else_block?
    OP_TOKEN, 603,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R_ELSE_BLOCK,
    OP_OPTIONAL, 
    OP_RETURN, A_IF_STMT_1,

 },
},
{"elif_stmt",
 R_ELIF_STMT,
 0,  // memo
 0,  // leftrec
 {0, 12, -1},
 {
    // 'elif' named_expression ':' block elif_stmt
    OP_TOKEN, 610,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R_ELIF_STMT,
    OP_RETURN, A_ELIF_STMT_0,

    // 'elif' named_expression ':' block else_block?
    OP_TOKEN, 610,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R_ELSE_BLOCK,
    OP_OPTIONAL, 
    OP_RETURN, A_ELIF_STMT_1,

 },
},
{"else_block",
 R_ELSE_BLOCK,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'else' &&':' block
    OP_TOKEN, 611,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_ELSE_BLOCK_0,

 },
},
{"while_stmt",
 R_WHILE_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'while' named_expression ':' block else_block?
    OP_TOKEN, 605,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R_ELSE_BLOCK,
    OP_OPTIONAL, 
    OP_RETURN, A_WHILE_STMT_0,

 },
},
{"for_stmt",
 R_FOR_STMT,
 0,  // memo
 0,  // leftrec
 {0, 21, -1},
 {
    // 'for' star_targets 'in' ~ star_expressions ':' TYPE_COMMENT? block else_block?
    OP_TOKEN, 612,
    OP_RULE, R_STAR_TARGETS,
    OP_TOKEN, 613,
    OP_CUT, 
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_TOKEN, COLON,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RULE, R_BLOCK,
    OP_RULE, R_ELSE_BLOCK,
    OP_OPTIONAL, 
    OP_RETURN, A_FOR_STMT_0,

    // ASYNC 'for' star_targets 'in' ~ star_expressions ':' TYPE_COMMENT? block else_block?
    OP_TOKEN, ASYNC,
    OP_TOKEN, 612,
    OP_RULE, R_STAR_TARGETS,
    OP_TOKEN, 613,
    OP_CUT, 
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_TOKEN, COLON,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RULE, R_BLOCK,
    OP_RULE, R_ELSE_BLOCK,
    OP_OPTIONAL, 
    OP_RETURN, A_FOR_STMT_1,

 },
},
{"with_stmt",
 R_WITH_STMT,
 0,  // memo
 0,  // leftrec
 {0, 17, 30, 49, -1},
 {
    // 'with' '(' ','.with_item+ ','? ')' ':' block
    OP_TOKEN, 614,
    OP_TOKEN, LPAR,
    OP_RULE, R__GATHER_51,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_WITH_STMT_0,

    // 'with' ','.with_item+ ':' TYPE_COMMENT? block
    OP_TOKEN, 614,
    OP_RULE, R__GATHER_53,
    OP_TOKEN, COLON,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RULE, R_BLOCK,
    OP_RETURN, A_WITH_STMT_1,

    // ASYNC 'with' '(' ','.with_item+ ','? ')' ':' block
    OP_TOKEN, ASYNC,
    OP_TOKEN, 614,
    OP_TOKEN, LPAR,
    OP_RULE, R__GATHER_55,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_WITH_STMT_2,

    // ASYNC 'with' ','.with_item+ ':' TYPE_COMMENT? block
    OP_TOKEN, ASYNC,
    OP_TOKEN, 614,
    OP_RULE, R__GATHER_57,
    OP_TOKEN, COLON,
    OP_TOKEN, TYPE_COMMENT,
    OP_OPTIONAL, 
    OP_RULE, R_BLOCK,
    OP_RETURN, A_WITH_STMT_3,

 },
},
{"with_item",
 R_WITH_ITEM,
 0,  // memo
 0,  // leftrec
 {0, 12, -1},
 {
    // expression 'as' star_target &(',' | ')' | ':')
    OP_RULE, R_EXPRESSION,
    OP_TOKEN, 615,
    OP_RULE, R_STAR_TARGET,
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_59,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_WITH_ITEM_0,

    // expression
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_WITH_ITEM_1,

 },
},
{"try_stmt",
 R_TRY_STMT,
 0,  // memo
 0,  // leftrec
 {0, 10, 26, -1},
 {
    // 'try' &&':' block finally_block
    OP_TOKEN, 604,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R_FINALLY_BLOCK,
    OP_RETURN, A_TRY_STMT_0,

    // 'try' &&':' block except_block+ else_block? finally_block?
    OP_TOKEN, 604,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R__LOOP1_60,
    OP_RULE, R_ELSE_BLOCK,
    OP_OPTIONAL, 
    OP_RULE, R_FINALLY_BLOCK,
    OP_OPTIONAL, 
    OP_RETURN, A_TRY_STMT_1,

    // 'try' &&':' block except_star_block+ else_block? finally_block?
    OP_TOKEN, 604,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RULE, R__LOOP1_61,
    OP_RULE, R_ELSE_BLOCK,
    OP_OPTIONAL, 
    OP_RULE, R_FINALLY_BLOCK,
    OP_OPTIONAL, 
    OP_RETURN, A_TRY_STMT_2,

 },
},
{"except_block",
 R_EXCEPT_BLOCK,
 0,  // memo
 0,  // leftrec
 {0, 13, -1},
 {
    // 'except' expression ['as' NAME] ':' block
    OP_TOKEN, 616,
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__TMP_62,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_EXCEPT_BLOCK_0,

    // 'except' ':' block
    OP_TOKEN, 616,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_EXCEPT_BLOCK_1,

 },
},
{"except_star_block",
 R_EXCEPT_STAR_BLOCK,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'except' '*' expression ['as' NAME] ':' block
    OP_TOKEN, 616,
    OP_TOKEN, STAR,
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__TMP_63,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_EXCEPT_STAR_BLOCK_0,

 },
},
{"finally_block",
 R_FINALLY_BLOCK,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'finally' &&':' block
    OP_TOKEN, 617,
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_FINALLY_BLOCK_0,

 },
},
{"match_stmt",
 R_MATCH_STMT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // "match" subject_expr ':' NEWLINE INDENT case_block+ DEDENT
    OP_SOFT_KEYWORD, SK_MATCH,
    OP_RULE, R_SUBJECT_EXPR,
    OP_TOKEN, COLON,
    OP_TOKEN, NEWLINE,
    OP_TOKEN, INDENT,
    OP_RULE, R__LOOP1_64,
    OP_TOKEN, DEDENT,
    OP_RETURN, A_MATCH_STMT_0,

 },
},
{"subject_expr",
 R_SUBJECT_EXPR,
 0,  // memo
 0,  // leftrec
 {0, 9, -1},
 {
    // star_named_expression ',' star_named_expressions?
    OP_RULE, R_STAR_NAMED_EXPRESSION,
    OP_TOKEN, COMMA,
    OP_RULE, R_STAR_NAMED_EXPRESSIONS,
    OP_OPTIONAL, 
    OP_RETURN, A_SUBJECT_EXPR_0,

    // named_expression
    OP_RULE, R_NAMED_EXPRESSION,
    OP_RETURN, A_SUBJECT_EXPR_1,

 },
},
{"case_block",
 R_CASE_BLOCK,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // "case" patterns guard? ':' block
    OP_SOFT_KEYWORD, SK_CASE,
    OP_RULE, R_PATTERNS,
    OP_RULE, R_GUARD,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_BLOCK,
    OP_RETURN, A_CASE_BLOCK_0,

 },
},
{"guard",
 R_GUARD,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'if' named_expression
    OP_TOKEN, 603,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_RETURN, A_GUARD_0,

 },
},
{"patterns",
 R_PATTERNS,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // open_sequence_pattern
    OP_RULE, R_OPEN_SEQUENCE_PATTERN,
    OP_RETURN, A_PATTERNS_0,

    // pattern
    OP_RULE, R_PATTERN,
    OP_RETURN, A_PATTERNS_1,

 },
},
{"pattern",
 R_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // as_pattern
    OP_RULE, R_AS_PATTERN,
    OP_RETURN, A_PATTERN_0,

    // or_pattern
    OP_RULE, R_OR_PATTERN,
    OP_RETURN, A_PATTERN_1,

 },
},
{"as_pattern",
 R_AS_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // or_pattern 'as' pattern_capture_target
    OP_RULE, R_OR_PATTERN,
    OP_TOKEN, 615,
    OP_RULE, R_PATTERN_CAPTURE_TARGET,
    OP_RETURN, A_AS_PATTERN_0,

 },
},
{"or_pattern",
 R_OR_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '|'.closed_pattern+
    OP_RULE, R__GATHER_65,
    OP_RETURN, A_OR_PATTERN_0,

 },
},
{"closed_pattern",
 R_CLOSED_PATTERN,
 1,  // memo
 0,  // leftrec
 {0, 4, 8, 12, 16, 20, 24, 28, -1},
 {
    // literal_pattern
    OP_RULE, R_LITERAL_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_0,

    // capture_pattern
    OP_RULE, R_CAPTURE_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_1,

    // wildcard_pattern
    OP_RULE, R_WILDCARD_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_2,

    // value_pattern
    OP_RULE, R_VALUE_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_3,

    // group_pattern
    OP_RULE, R_GROUP_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_4,

    // sequence_pattern
    OP_RULE, R_SEQUENCE_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_5,

    // mapping_pattern
    OP_RULE, R_MAPPING_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_6,

    // class_pattern
    OP_RULE, R_CLASS_PATTERN,
    OP_RETURN, A_CLOSED_PATTERN_7,

 },
},
{"literal_pattern",
 R_LITERAL_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, 8, 12, 16, 20, 24, -1},
 {
    // signed_number !('+' | '-')
    OP_RULE, R_SIGNED_NUMBER,
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_67,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_LITERAL_PATTERN_0,

    // complex_number
    OP_RULE, R_COMPLEX_NUMBER,
    OP_RETURN, A_LITERAL_PATTERN_1,

    // strings
    OP_RULE, R_STRINGS,
    OP_RETURN, A_LITERAL_PATTERN_2,

    // 'None'
    OP_TOKEN, 618,
    OP_RETURN, A_LITERAL_PATTERN_3,

    // 'True'
    OP_TOKEN, 619,
    OP_RETURN, A_LITERAL_PATTERN_4,

    // 'False'
    OP_TOKEN, 620,
    OP_RETURN, A_LITERAL_PATTERN_5,

 },
},
{"literal_expr",
 R_LITERAL_EXPR,
 0,  // memo
 0,  // leftrec
 {0, 8, 12, 16, 20, 24, -1},
 {
    // signed_number !('+' | '-')
    OP_RULE, R_SIGNED_NUMBER,
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_68,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_LITERAL_EXPR_0,

    // complex_number
    OP_RULE, R_COMPLEX_NUMBER,
    OP_RETURN, A_LITERAL_EXPR_1,

    // strings
    OP_RULE, R_STRINGS,
    OP_RETURN, A_LITERAL_EXPR_2,

    // 'None'
    OP_TOKEN, 618,
    OP_RETURN, A_LITERAL_EXPR_3,

    // 'True'
    OP_TOKEN, 619,
    OP_RETURN, A_LITERAL_EXPR_4,

    // 'False'
    OP_TOKEN, 620,
    OP_RETURN, A_LITERAL_EXPR_5,

 },
},
{"complex_number",
 R_COMPLEX_NUMBER,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // signed_real_number '+' imaginary_number
    OP_RULE, R_SIGNED_REAL_NUMBER,
    OP_TOKEN, PLUS,
    OP_RULE, R_IMAGINARY_NUMBER,
    OP_RETURN, A_COMPLEX_NUMBER_0,

    // signed_real_number '-' imaginary_number
    OP_RULE, R_SIGNED_REAL_NUMBER,
    OP_TOKEN, MINUS,
    OP_RULE, R_IMAGINARY_NUMBER,
    OP_RETURN, A_COMPLEX_NUMBER_1,

 },
},
{"signed_number",
 R_SIGNED_NUMBER,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // NUMBER
    OP_NUMBER, 
    OP_RETURN, A_SIGNED_NUMBER_0,

    // '-' NUMBER
    OP_TOKEN, MINUS,
    OP_NUMBER, 
    OP_RETURN, A_SIGNED_NUMBER_1,

 },
},
{"signed_real_number",
 R_SIGNED_REAL_NUMBER,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // real_number
    OP_RULE, R_REAL_NUMBER,
    OP_RETURN, A_SIGNED_REAL_NUMBER_0,

    // '-' real_number
    OP_TOKEN, MINUS,
    OP_RULE, R_REAL_NUMBER,
    OP_RETURN, A_SIGNED_REAL_NUMBER_1,

 },
},
{"real_number",
 R_REAL_NUMBER,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NUMBER
    OP_NUMBER, 
    OP_RETURN, A_REAL_NUMBER_0,

 },
},
{"imaginary_number",
 R_IMAGINARY_NUMBER,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NUMBER
    OP_NUMBER, 
    OP_RETURN, A_IMAGINARY_NUMBER_0,

 },
},
{"capture_pattern",
 R_CAPTURE_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // pattern_capture_target
    OP_RULE, R_PATTERN_CAPTURE_TARGET,
    OP_RETURN, A_CAPTURE_PATTERN_0,

 },
},
{"pattern_capture_target",
 R_PATTERN_CAPTURE_TARGET,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // !"_" NAME !('.' | '(' | '=')
    OP_SAVE_MARK, 
    OP_SOFT_KEYWORD, SK__,
    OP_NEG_LOOKAHEAD, 
    OP_NAME, 
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_69,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_PATTERN_CAPTURE_TARGET_0,

 },
},
{"wildcard_pattern",
 R_WILDCARD_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // "_"
    OP_SOFT_KEYWORD, SK__,
    OP_RETURN, A_WILDCARD_PATTERN_0,

 },
},
{"value_pattern",
 R_VALUE_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // attr !('.' | '(' | '=')
    OP_RULE, R_ATTR,
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_70,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_VALUE_PATTERN_0,

 },
},
{"attr",
 R_ATTR,
 0,  // memo
 1,  // leftrec
 {0, -1},
 {
    // name_or_attr '.' NAME
    OP_RULE, R_NAME_OR_ATTR,
    OP_TOKEN, DOT,
    OP_NAME, 
    OP_RETURN, A_ATTR_0,

 },
},
{"name_or_attr",
 R_NAME_OR_ATTR,
 0,  // memo
 1,  // leftrec
 {0, 4, -1},
 {
    // attr
    OP_RULE, R_ATTR,
    OP_RETURN, A_NAME_OR_ATTR_0,

    // NAME
    OP_NAME, 
    OP_RETURN, A_NAME_OR_ATTR_1,

 },
},
{"group_pattern",
 R_GROUP_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '(' pattern ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_PATTERN,
    OP_TOKEN, RPAR,
    OP_RETURN, A_GROUP_PATTERN_0,

 },
},
{"sequence_pattern",
 R_SEQUENCE_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, 9, -1},
 {
    // '[' maybe_sequence_pattern? ']'
    OP_TOKEN, LSQB,
    OP_RULE, R_MAYBE_SEQUENCE_PATTERN,
    OP_OPTIONAL, 
    OP_TOKEN, RSQB,
    OP_RETURN, A_SEQUENCE_PATTERN_0,

    // '(' open_sequence_pattern? ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_OPEN_SEQUENCE_PATTERN,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_SEQUENCE_PATTERN_1,

 },
},
{"open_sequence_pattern",
 R_OPEN_SEQUENCE_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // maybe_star_pattern ',' maybe_sequence_pattern?
    OP_RULE, R_MAYBE_STAR_PATTERN,
    OP_TOKEN, COMMA,
    OP_RULE, R_MAYBE_SEQUENCE_PATTERN,
    OP_OPTIONAL, 
    OP_RETURN, A_OPEN_SEQUENCE_PATTERN_0,

 },
},
{"maybe_sequence_pattern",
 R_MAYBE_SEQUENCE_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.maybe_star_pattern+ ','?
    OP_RULE, R__GATHER_71,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_MAYBE_SEQUENCE_PATTERN_0,

 },
},
{"maybe_star_pattern",
 R_MAYBE_STAR_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // star_pattern
    OP_RULE, R_STAR_PATTERN,
    OP_RETURN, A_MAYBE_STAR_PATTERN_0,

    // pattern
    OP_RULE, R_PATTERN,
    OP_RETURN, A_MAYBE_STAR_PATTERN_1,

 },
},
{"star_pattern",
 R_STAR_PATTERN,
 1,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // '*' pattern_capture_target
    OP_TOKEN, STAR,
    OP_RULE, R_PATTERN_CAPTURE_TARGET,
    OP_RETURN, A_STAR_PATTERN_0,

    // '*' wildcard_pattern
    OP_TOKEN, STAR,
    OP_RULE, R_WILDCARD_PATTERN,
    OP_RETURN, A_STAR_PATTERN_1,

 },
},
{"mapping_pattern",
 R_MAPPING_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, 6, 17, 32, -1},
 {
    // '{' '}'
    OP_TOKEN, LBRACE,
    OP_TOKEN, RBRACE,
    OP_RETURN, A_MAPPING_PATTERN_0,

    // '{' double_star_pattern ','? '}'
    OP_TOKEN, LBRACE,
    OP_RULE, R_DOUBLE_STAR_PATTERN,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RBRACE,
    OP_RETURN, A_MAPPING_PATTERN_1,

    // '{' items_pattern ',' double_star_pattern ','? '}'
    OP_TOKEN, LBRACE,
    OP_RULE, R_ITEMS_PATTERN,
    OP_TOKEN, COMMA,
    OP_RULE, R_DOUBLE_STAR_PATTERN,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RBRACE,
    OP_RETURN, A_MAPPING_PATTERN_2,

    // '{' items_pattern ','? '}'
    OP_TOKEN, LBRACE,
    OP_RULE, R_ITEMS_PATTERN,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RBRACE,
    OP_RETURN, A_MAPPING_PATTERN_3,

 },
},
{"items_pattern",
 R_ITEMS_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.key_value_pattern+
    OP_RULE, R__GATHER_73,
    OP_RETURN, A_ITEMS_PATTERN_0,

 },
},
{"key_value_pattern",
 R_KEY_VALUE_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // (literal_expr | attr) ':' pattern
    OP_RULE, R__TMP_75,
    OP_TOKEN, COLON,
    OP_RULE, R_PATTERN,
    OP_RETURN, A_KEY_VALUE_PATTERN_0,

 },
},
{"double_star_pattern",
 R_DOUBLE_STAR_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '**' pattern_capture_target
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_PATTERN_CAPTURE_TARGET,
    OP_RETURN, A_DOUBLE_STAR_PATTERN_0,

 },
},
{"class_pattern",
 R_CLASS_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, 8, 21, 34, -1},
 {
    // name_or_attr '(' ')'
    OP_RULE, R_NAME_OR_ATTR,
    OP_TOKEN, LPAR,
    OP_TOKEN, RPAR,
    OP_RETURN, A_CLASS_PATTERN_0,

    // name_or_attr '(' positional_patterns ','? ')'
    OP_RULE, R_NAME_OR_ATTR,
    OP_TOKEN, LPAR,
    OP_RULE, R_POSITIONAL_PATTERNS,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_CLASS_PATTERN_1,

    // name_or_attr '(' keyword_patterns ','? ')'
    OP_RULE, R_NAME_OR_ATTR,
    OP_TOKEN, LPAR,
    OP_RULE, R_KEYWORD_PATTERNS,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_CLASS_PATTERN_2,

    // name_or_attr '(' positional_patterns ',' keyword_patterns ','? ')'
    OP_RULE, R_NAME_OR_ATTR,
    OP_TOKEN, LPAR,
    OP_RULE, R_POSITIONAL_PATTERNS,
    OP_TOKEN, COMMA,
    OP_RULE, R_KEYWORD_PATTERNS,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_CLASS_PATTERN_3,

 },
},
{"positional_patterns",
 R_POSITIONAL_PATTERNS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.pattern+
    OP_RULE, R__GATHER_76,
    OP_RETURN, A_POSITIONAL_PATTERNS_0,

 },
},
{"keyword_patterns",
 R_KEYWORD_PATTERNS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.keyword_pattern+
    OP_RULE, R__GATHER_78,
    OP_RETURN, A_KEYWORD_PATTERNS_0,

 },
},
{"keyword_pattern",
 R_KEYWORD_PATTERN,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME '=' pattern
    OP_NAME, 
    OP_TOKEN, EQUAL,
    OP_RULE, R_PATTERN,
    OP_RETURN, A_KEYWORD_PATTERN_0,

 },
},
{"expressions",
 R_EXPRESSIONS,
 0,  // memo
 0,  // leftrec
 {0, 9, 15, -1},
 {
    // expression ((',' expression))+ ','?
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__LOOP1_80,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_EXPRESSIONS_0,

    // expression ','
    OP_RULE, R_EXPRESSION,
    OP_TOKEN, COMMA,
    OP_RETURN, A_EXPRESSIONS_1,

    // expression
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_EXPRESSIONS_2,

 },
},
{"expression",
 R_EXPRESSION,
 1,  // memo
 0,  // leftrec
 {0, 12, 16, -1},
 {
    // disjunction 'if' disjunction 'else' expression
    OP_RULE, R_DISJUNCTION,
    OP_TOKEN, 603,
    OP_RULE, R_DISJUNCTION,
    OP_TOKEN, 611,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_EXPRESSION_0,

    // disjunction
    OP_RULE, R_DISJUNCTION,
    OP_RETURN, A_EXPRESSION_1,

    // lambdef
    OP_RULE, R_LAMBDEF,
    OP_RETURN, A_EXPRESSION_2,

 },
},
{"yield_expr",
 R_YIELD_EXPR,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // 'yield' 'from' expression
    OP_TOKEN, 597,
    OP_TOKEN, 607,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_YIELD_EXPR_0,

    // 'yield' star_expressions?
    OP_TOKEN, 597,
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_OPTIONAL, 
    OP_RETURN, A_YIELD_EXPR_1,

 },
},
{"star_expressions",
 R_STAR_EXPRESSIONS,
 0,  // memo
 0,  // leftrec
 {0, 9, 15, -1},
 {
    // star_expression ((',' star_expression))+ ','?
    OP_RULE, R_STAR_EXPRESSION,
    OP_RULE, R__LOOP1_81,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_EXPRESSIONS_0,

    // star_expression ','
    OP_RULE, R_STAR_EXPRESSION,
    OP_TOKEN, COMMA,
    OP_RETURN, A_STAR_EXPRESSIONS_1,

    // star_expression
    OP_RULE, R_STAR_EXPRESSION,
    OP_RETURN, A_STAR_EXPRESSIONS_2,

 },
},
{"star_expression",
 R_STAR_EXPRESSION,
 1,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // '*' bitwise_or
    OP_TOKEN, STAR,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_STAR_EXPRESSION_0,

    // expression
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_STAR_EXPRESSION_1,

 },
},
{"star_named_expressions",
 R_STAR_NAMED_EXPRESSIONS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.star_named_expression+ ','?
    OP_RULE, R__GATHER_82,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_NAMED_EXPRESSIONS_0,

 },
},
{"star_named_expression",
 R_STAR_NAMED_EXPRESSION,
 0,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // '*' bitwise_or
    OP_TOKEN, STAR,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_STAR_NAMED_EXPRESSION_0,

    // named_expression
    OP_RULE, R_NAMED_EXPRESSION,
    OP_RETURN, A_STAR_NAMED_EXPRESSION_1,

 },
},
{"assignment_expression",
 R_ASSIGNMENT_EXPRESSION,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME ':=' ~ expression
    OP_NAME, 
    OP_TOKEN, COLONEQUAL,
    OP_CUT, 
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_ASSIGNMENT_EXPRESSION_0,

 },
},
{"named_expression",
 R_NAMED_EXPRESSION,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // assignment_expression
    OP_RULE, R_ASSIGNMENT_EXPRESSION,
    OP_RETURN, A_NAMED_EXPRESSION_0,

    // expression !':='
    OP_RULE, R_EXPRESSION,
    OP_SAVE_MARK, 
    OP_TOKEN, COLONEQUAL,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_NAMED_EXPRESSION_1,

 },
},
{"disjunction",
 R_DISJUNCTION,
 1,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // conjunction (('or' conjunction))+
    OP_RULE, R_CONJUNCTION,
    OP_RULE, R__LOOP1_84,
    OP_RETURN, A_DISJUNCTION_0,

    // conjunction
    OP_RULE, R_CONJUNCTION,
    OP_RETURN, A_DISJUNCTION_1,

 },
},
{"conjunction",
 R_CONJUNCTION,
 1,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // inversion (('and' inversion))+
    OP_RULE, R_INVERSION,
    OP_RULE, R__LOOP1_85,
    OP_RETURN, A_CONJUNCTION_0,

    // inversion
    OP_RULE, R_INVERSION,
    OP_RETURN, A_CONJUNCTION_1,

 },
},
{"inversion",
 R_INVERSION,
 1,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // 'not' inversion
    OP_TOKEN, 621,
    OP_RULE, R_INVERSION,
    OP_RETURN, A_INVERSION_0,

    // comparison
    OP_RULE, R_COMPARISON,
    OP_RETURN, A_INVERSION_1,

 },
},
{"comparison",
 R_COMPARISON,
 0,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // bitwise_or compare_op_bitwise_or_pair+
    OP_RULE, R_BITWISE_OR,
    OP_RULE, R__LOOP1_86,
    OP_RETURN, A_COMPARISON_0,

    // bitwise_or
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_COMPARISON_1,

 },
},
{"compare_op_bitwise_or_pair",
 R_COMPARE_OP_BITWISE_OR_PAIR,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, -1},
 {
    // eq_bitwise_or
    OP_RULE, R_EQ_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_0,

    // noteq_bitwise_or
    OP_RULE, R_NOTEQ_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_1,

    // lte_bitwise_or
    OP_RULE, R_LTE_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_2,

    // lt_bitwise_or
    OP_RULE, R_LT_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_3,

    // gte_bitwise_or
    OP_RULE, R_GTE_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_4,

    // gt_bitwise_or
    OP_RULE, R_GT_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_5,

    // notin_bitwise_or
    OP_RULE, R_NOTIN_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_6,

    // in_bitwise_or
    OP_RULE, R_IN_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_7,

    // isnot_bitwise_or
    OP_RULE, R_ISNOT_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_8,

    // is_bitwise_or
    OP_RULE, R_IS_BITWISE_OR,
    OP_RETURN, A_COMPARE_OP_BITWISE_OR_PAIR_9,

 },
},
{"eq_bitwise_or",
 R_EQ_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '==' bitwise_or
    OP_TOKEN, EQEQUAL,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_EQ_BITWISE_OR_0,

 },
},
{"noteq_bitwise_or",
 R_NOTEQ_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ('!=') bitwise_or
    OP_RULE, R__TMP_87,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_NOTEQ_BITWISE_OR_0,

 },
},
{"lte_bitwise_or",
 R_LTE_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '<=' bitwise_or
    OP_TOKEN, LESSEQUAL,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_LTE_BITWISE_OR_0,

 },
},
{"lt_bitwise_or",
 R_LT_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '<' bitwise_or
    OP_TOKEN, LESS,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_LT_BITWISE_OR_0,

 },
},
{"gte_bitwise_or",
 R_GTE_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '>=' bitwise_or
    OP_TOKEN, GREATEREQUAL,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_GTE_BITWISE_OR_0,

 },
},
{"gt_bitwise_or",
 R_GT_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '>' bitwise_or
    OP_TOKEN, GREATER,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_GT_BITWISE_OR_0,

 },
},
{"notin_bitwise_or",
 R_NOTIN_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'not' 'in' bitwise_or
    OP_TOKEN, 621,
    OP_TOKEN, 613,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_NOTIN_BITWISE_OR_0,

 },
},
{"in_bitwise_or",
 R_IN_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'in' bitwise_or
    OP_TOKEN, 613,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_IN_BITWISE_OR_0,

 },
},
{"isnot_bitwise_or",
 R_ISNOT_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'is' 'not' bitwise_or
    OP_TOKEN, 622,
    OP_TOKEN, 621,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_ISNOT_BITWISE_OR_0,

 },
},
{"is_bitwise_or",
 R_IS_BITWISE_OR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'is' bitwise_or
    OP_TOKEN, 622,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_IS_BITWISE_OR_0,

 },
},
{"bitwise_or",
 R_BITWISE_OR,
 0,  // memo
 1,  // leftrec
 {0, 8, -1},
 {
    // bitwise_or '|' bitwise_xor
    OP_RULE, R_BITWISE_OR,
    OP_TOKEN, VBAR,
    OP_RULE, R_BITWISE_XOR,
    OP_RETURN, A_BITWISE_OR_0,

    // bitwise_xor
    OP_RULE, R_BITWISE_XOR,
    OP_RETURN, A_BITWISE_OR_1,

 },
},
{"bitwise_xor",
 R_BITWISE_XOR,
 0,  // memo
 1,  // leftrec
 {0, 8, -1},
 {
    // bitwise_xor '^' bitwise_and
    OP_RULE, R_BITWISE_XOR,
    OP_TOKEN, CIRCUMFLEX,
    OP_RULE, R_BITWISE_AND,
    OP_RETURN, A_BITWISE_XOR_0,

    // bitwise_and
    OP_RULE, R_BITWISE_AND,
    OP_RETURN, A_BITWISE_XOR_1,

 },
},
{"bitwise_and",
 R_BITWISE_AND,
 0,  // memo
 1,  // leftrec
 {0, 8, -1},
 {
    // bitwise_and '&' shift_expr
    OP_RULE, R_BITWISE_AND,
    OP_TOKEN, AMPER,
    OP_RULE, R_SHIFT_EXPR,
    OP_RETURN, A_BITWISE_AND_0,

    // shift_expr
    OP_RULE, R_SHIFT_EXPR,
    OP_RETURN, A_BITWISE_AND_1,

 },
},
{"shift_expr",
 R_SHIFT_EXPR,
 0,  // memo
 1,  // leftrec
 {0, 8, 16, -1},
 {
    // shift_expr '<<' sum
    OP_RULE, R_SHIFT_EXPR,
    OP_TOKEN, LEFTSHIFT,
    OP_RULE, R_SUM,
    OP_RETURN, A_SHIFT_EXPR_0,

    // shift_expr '>>' sum
    OP_RULE, R_SHIFT_EXPR,
    OP_TOKEN, RIGHTSHIFT,
    OP_RULE, R_SUM,
    OP_RETURN, A_SHIFT_EXPR_1,

    // sum
    OP_RULE, R_SUM,
    OP_RETURN, A_SHIFT_EXPR_2,

 },
},
{"sum",
 R_SUM,
 0,  // memo
 1,  // leftrec
 {0, 8, 16, -1},
 {
    // sum '+' term
    OP_RULE, R_SUM,
    OP_TOKEN, PLUS,
    OP_RULE, R_TERM,
    OP_RETURN, A_SUM_0,

    // sum '-' term
    OP_RULE, R_SUM,
    OP_TOKEN, MINUS,
    OP_RULE, R_TERM,
    OP_RETURN, A_SUM_1,

    // term
    OP_RULE, R_TERM,
    OP_RETURN, A_SUM_2,

 },
},
{"term",
 R_TERM,
 0,  // memo
 1,  // leftrec
 {0, 8, 16, 24, 32, 40, -1},
 {
    // term '*' factor
    OP_RULE, R_TERM,
    OP_TOKEN, STAR,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_TERM_0,

    // term '/' factor
    OP_RULE, R_TERM,
    OP_TOKEN, SLASH,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_TERM_1,

    // term '//' factor
    OP_RULE, R_TERM,
    OP_TOKEN, DOUBLESLASH,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_TERM_2,

    // term '%' factor
    OP_RULE, R_TERM,
    OP_TOKEN, PERCENT,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_TERM_3,

    // term '@' factor
    OP_RULE, R_TERM,
    OP_TOKEN, AT,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_TERM_4,

    // factor
    OP_RULE, R_FACTOR,
    OP_RETURN, A_TERM_5,

 },
},
{"factor",
 R_FACTOR,
 1,  // memo
 0,  // leftrec
 {0, 6, 12, 18, -1},
 {
    // '+' factor
    OP_TOKEN, PLUS,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_FACTOR_0,

    // '-' factor
    OP_TOKEN, MINUS,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_FACTOR_1,

    // '~' factor
    OP_TOKEN, TILDE,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_FACTOR_2,

    // power
    OP_RULE, R_POWER,
    OP_RETURN, A_FACTOR_3,

 },
},
{"power",
 R_POWER,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // await_primary '**' factor
    OP_RULE, R_AWAIT_PRIMARY,
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_FACTOR,
    OP_RETURN, A_POWER_0,

    // await_primary
    OP_RULE, R_AWAIT_PRIMARY,
    OP_RETURN, A_POWER_1,

 },
},
{"await_primary",
 R_AWAIT_PRIMARY,
 1,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // AWAIT primary
    OP_TOKEN, AWAIT,
    OP_RULE, R_PRIMARY,
    OP_RETURN, A_AWAIT_PRIMARY_0,

    // primary
    OP_RULE, R_PRIMARY,
    OP_RETURN, A_AWAIT_PRIMARY_1,

 },
},
{"primary",
 R_PRIMARY,
 0,  // memo
 1,  // leftrec
 {0, 7, 13, 24, 34, -1},
 {
    // primary '.' NAME
    OP_RULE, R_PRIMARY,
    OP_TOKEN, DOT,
    OP_NAME, 
    OP_RETURN, A_PRIMARY_0,

    // primary genexp
    OP_RULE, R_PRIMARY,
    OP_RULE, R_GENEXP,
    OP_RETURN, A_PRIMARY_1,

    // primary '(' arguments? ')'
    OP_RULE, R_PRIMARY,
    OP_TOKEN, LPAR,
    OP_RULE, R_ARGUMENTS,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_PRIMARY_2,

    // primary '[' slices ']'
    OP_RULE, R_PRIMARY,
    OP_TOKEN, LSQB,
    OP_RULE, R_SLICES,
    OP_TOKEN, RSQB,
    OP_RETURN, A_PRIMARY_3,

    // atom
    OP_RULE, R_ATOM,
    OP_RETURN, A_PRIMARY_4,

 },
},
{"slices",
 R_SLICES,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // slice !','
    OP_RULE, R_SLICE,
    OP_SAVE_MARK, 
    OP_TOKEN, COMMA,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_SLICES_0,

    // ','.(slice | starred_expression)+ ','?
    OP_RULE, R__GATHER_88,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_SLICES_1,

 },
},
{"slice",
 R_SLICE,
 0,  // memo
 0,  // leftrec
 {0, 13, -1},
 {
    // expression? ':' expression? [':' expression?]
    OP_RULE, R_EXPRESSION,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_EXPRESSION,
    OP_OPTIONAL, 
    OP_RULE, R__TMP_90,
    OP_OPTIONAL, 
    OP_RETURN, A_SLICE_0,

    // named_expression
    OP_RULE, R_NAMED_EXPRESSION,
    OP_RETURN, A_SLICE_1,

 },
},
{"atom",
 R_ATOM,
 0,  // memo
 0,  // leftrec
 {0, 3, 7, 11, 15, 22, 25, 33, 41, 49, -1},
 {
    // NAME
    OP_NAME, 
    OP_RETURN, A_ATOM_0,

    // 'True'
    OP_TOKEN, 619,
    OP_RETURN, A_ATOM_1,

    // 'False'
    OP_TOKEN, 620,
    OP_RETURN, A_ATOM_2,

    // 'None'
    OP_TOKEN, 618,
    OP_RETURN, A_ATOM_3,

    // &STRING strings
    OP_SAVE_MARK, 
    OP_STRING, 
    OP_POS_LOOKAHEAD, 
    OP_RULE, R_STRINGS,
    OP_RETURN, A_ATOM_4,

    // NUMBER
    OP_NUMBER, 
    OP_RETURN, A_ATOM_5,

    // &'(' (tuple | group | genexp)
    OP_SAVE_MARK, 
    OP_TOKEN, LPAR,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R__TMP_91,
    OP_RETURN, A_ATOM_6,

    // &'[' (list | listcomp)
    OP_SAVE_MARK, 
    OP_TOKEN, LSQB,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R__TMP_92,
    OP_RETURN, A_ATOM_7,

    // &'{' (dict | set | dictcomp | setcomp)
    OP_SAVE_MARK, 
    OP_TOKEN, LBRACE,
    OP_POS_LOOKAHEAD, 
    OP_RULE, R__TMP_93,
    OP_RETURN, A_ATOM_8,

    // '...'
    OP_TOKEN, ELLIPSIS,
    OP_RETURN, A_ATOM_9,

 },
},
{"group",
 R_GROUP,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '(' (yield_expr | named_expression) ')'
    OP_TOKEN, LPAR,
    OP_RULE, R__TMP_94,
    OP_TOKEN, RPAR,
    OP_RETURN, A_GROUP_0,

 },
},
{"lambdef",
 R_LAMBDEF,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'lambda' lambda_params? ':' expression
    OP_TOKEN, 623,
    OP_RULE, R_LAMBDA_PARAMS,
    OP_OPTIONAL, 
    OP_TOKEN, COLON,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_LAMBDEF_0,

 },
},
{"lambda_params",
 R_LAMBDA_PARAMS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // lambda_parameters
    OP_RULE, R_LAMBDA_PARAMETERS,
    OP_RETURN, A_LAMBDA_PARAMS_0,

 },
},
{"lambda_parameters",
 R_LAMBDA_PARAMETERS,
 0,  // memo
 0,  // leftrec
 {0, 11, 20, 29, 36, -1},
 {
    // lambda_slash_no_default lambda_param_no_default* lambda_param_with_default* lambda_star_etc?
    OP_RULE, R_LAMBDA_SLASH_NO_DEFAULT,
    OP_RULE, R__LOOP0_95,
    OP_RULE, R__LOOP0_96,
    OP_RULE, R_LAMBDA_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_LAMBDA_PARAMETERS_0,

    // lambda_slash_with_default lambda_param_with_default* lambda_star_etc?
    OP_RULE, R_LAMBDA_SLASH_WITH_DEFAULT,
    OP_RULE, R__LOOP0_97,
    OP_RULE, R_LAMBDA_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_LAMBDA_PARAMETERS_1,

    // lambda_param_no_default+ lambda_param_with_default* lambda_star_etc?
    OP_RULE, R__LOOP1_98,
    OP_RULE, R__LOOP0_99,
    OP_RULE, R_LAMBDA_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_LAMBDA_PARAMETERS_2,

    // lambda_param_with_default+ lambda_star_etc?
    OP_RULE, R__LOOP1_100,
    OP_RULE, R_LAMBDA_STAR_ETC,
    OP_OPTIONAL, 
    OP_RETURN, A_LAMBDA_PARAMETERS_3,

    // lambda_star_etc
    OP_RULE, R_LAMBDA_STAR_ETC,
    OP_RETURN, A_LAMBDA_PARAMETERS_4,

 },
},
{"lambda_slash_no_default",
 R_LAMBDA_SLASH_NO_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // lambda_param_no_default+ '/' ','
    OP_RULE, R__LOOP1_101,
    OP_TOKEN, SLASH,
    OP_TOKEN, COMMA,
    OP_RETURN, A_LAMBDA_SLASH_NO_DEFAULT_0,

    // lambda_param_no_default+ '/' &':'
    OP_RULE, R__LOOP1_102,
    OP_TOKEN, SLASH,
    OP_SAVE_MARK, 
    OP_TOKEN, COLON,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_LAMBDA_SLASH_NO_DEFAULT_1,

 },
},
{"lambda_slash_with_default",
 R_LAMBDA_SLASH_WITH_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 10, -1},
 {
    // lambda_param_no_default* lambda_param_with_default+ '/' ','
    OP_RULE, R__LOOP0_103,
    OP_RULE, R__LOOP1_104,
    OP_TOKEN, SLASH,
    OP_TOKEN, COMMA,
    OP_RETURN, A_LAMBDA_SLASH_WITH_DEFAULT_0,

    // lambda_param_no_default* lambda_param_with_default+ '/' &':'
    OP_RULE, R__LOOP0_105,
    OP_RULE, R__LOOP1_106,
    OP_TOKEN, SLASH,
    OP_SAVE_MARK, 
    OP_TOKEN, COLON,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_LAMBDA_SLASH_WITH_DEFAULT_1,

 },
},
{"lambda_star_etc",
 R_LAMBDA_STAR_ETC,
 0,  // memo
 0,  // leftrec
 {0, 11, 22, -1},
 {
    // '*' lambda_param_no_default lambda_param_maybe_default* lambda_kwds?
    OP_TOKEN, STAR,
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_RULE, R__LOOP0_107,
    OP_RULE, R_LAMBDA_KWDS,
    OP_OPTIONAL, 
    OP_RETURN, A_LAMBDA_STAR_ETC_0,

    // '*' ',' lambda_param_maybe_default+ lambda_kwds?
    OP_TOKEN, STAR,
    OP_TOKEN, COMMA,
    OP_RULE, R__LOOP1_108,
    OP_RULE, R_LAMBDA_KWDS,
    OP_OPTIONAL, 
    OP_RETURN, A_LAMBDA_STAR_ETC_1,

    // lambda_kwds
    OP_RULE, R_LAMBDA_KWDS,
    OP_RETURN, A_LAMBDA_STAR_ETC_2,

 },
},
{"lambda_kwds",
 R_LAMBDA_KWDS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '**' lambda_param_no_default
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_RETURN, A_LAMBDA_KWDS_0,

 },
},
{"lambda_param_no_default",
 R_LAMBDA_PARAM_NO_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // lambda_param ','
    OP_RULE, R_LAMBDA_PARAM,
    OP_TOKEN, COMMA,
    OP_RETURN, A_LAMBDA_PARAM_NO_DEFAULT_0,

    // lambda_param &':'
    OP_RULE, R_LAMBDA_PARAM,
    OP_SAVE_MARK, 
    OP_TOKEN, COLON,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_LAMBDA_PARAM_NO_DEFAULT_1,

 },
},
{"lambda_param_with_default",
 R_LAMBDA_PARAM_WITH_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // lambda_param default ','
    OP_RULE, R_LAMBDA_PARAM,
    OP_RULE, R_DEFAULT,
    OP_TOKEN, COMMA,
    OP_RETURN, A_LAMBDA_PARAM_WITH_DEFAULT_0,

    // lambda_param default &':'
    OP_RULE, R_LAMBDA_PARAM,
    OP_RULE, R_DEFAULT,
    OP_SAVE_MARK, 
    OP_TOKEN, COLON,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_LAMBDA_PARAM_WITH_DEFAULT_1,

 },
},
{"lambda_param_maybe_default",
 R_LAMBDA_PARAM_MAYBE_DEFAULT,
 0,  // memo
 0,  // leftrec
 {0, 9, -1},
 {
    // lambda_param default? ','
    OP_RULE, R_LAMBDA_PARAM,
    OP_RULE, R_DEFAULT,
    OP_OPTIONAL, 
    OP_TOKEN, COMMA,
    OP_RETURN, A_LAMBDA_PARAM_MAYBE_DEFAULT_0,

    // lambda_param default? &':'
    OP_RULE, R_LAMBDA_PARAM,
    OP_RULE, R_DEFAULT,
    OP_OPTIONAL, 
    OP_SAVE_MARK, 
    OP_TOKEN, COLON,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_LAMBDA_PARAM_MAYBE_DEFAULT_1,

 },
},
{"lambda_param",
 R_LAMBDA_PARAM,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME
    OP_NAME, 
    OP_RETURN, A_LAMBDA_PARAM_0,

 },
},
{"strings",
 R_STRINGS,
 1,  // memo
 0,  // leftrec
 {0, -1},
 {
    // STRING+
    OP_RULE, R__LOOP1_109,
    OP_RETURN, A_STRINGS_0,

 },
},
{"list",
 R_LIST,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '[' star_named_expressions? ']'
    OP_TOKEN, LSQB,
    OP_RULE, R_STAR_NAMED_EXPRESSIONS,
    OP_OPTIONAL, 
    OP_TOKEN, RSQB,
    OP_RETURN, A_LIST_0,

 },
},
{"tuple",
 R_TUPLE,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '(' [star_named_expression ',' star_named_expressions?] ')'
    OP_TOKEN, LPAR,
    OP_RULE, R__TMP_110,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_TUPLE_0,

 },
},
{"set",
 R_SET,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '{' star_named_expressions '}'
    OP_TOKEN, LBRACE,
    OP_RULE, R_STAR_NAMED_EXPRESSIONS,
    OP_TOKEN, RBRACE,
    OP_RETURN, A_SET_0,

 },
},
{"dict",
 R_DICT,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '{' double_starred_kvpairs? '}'
    OP_TOKEN, LBRACE,
    OP_RULE, R_DOUBLE_STARRED_KVPAIRS,
    OP_OPTIONAL, 
    OP_TOKEN, RBRACE,
    OP_RETURN, A_DICT_0,

 },
},
{"double_starred_kvpairs",
 R_DOUBLE_STARRED_KVPAIRS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.double_starred_kvpair+ ','?
    OP_RULE, R__GATHER_111,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_DOUBLE_STARRED_KVPAIRS_0,

 },
},
{"double_starred_kvpair",
 R_DOUBLE_STARRED_KVPAIR,
 0,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // '**' bitwise_or
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_BITWISE_OR,
    OP_RETURN, A_DOUBLE_STARRED_KVPAIR_0,

    // kvpair
    OP_RULE, R_KVPAIR,
    OP_RETURN, A_DOUBLE_STARRED_KVPAIR_1,

 },
},
{"kvpair",
 R_KVPAIR,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // expression ':' expression
    OP_RULE, R_EXPRESSION,
    OP_TOKEN, COLON,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_KVPAIR_0,

 },
},
{"for_if_clauses",
 R_FOR_IF_CLAUSES,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // for_if_clause+
    OP_RULE, R__LOOP1_113,
    OP_RETURN, A_FOR_IF_CLAUSES_0,

 },
},
{"for_if_clause",
 R_FOR_IF_CLAUSE,
 0,  // memo
 0,  // leftrec
 {0, 15, -1},
 {
    // ASYNC 'for' star_targets 'in' ~ disjunction (('if' disjunction))*
    OP_TOKEN, ASYNC,
    OP_TOKEN, 612,
    OP_RULE, R_STAR_TARGETS,
    OP_TOKEN, 613,
    OP_CUT, 
    OP_RULE, R_DISJUNCTION,
    OP_RULE, R__LOOP0_114,
    OP_RETURN, A_FOR_IF_CLAUSE_0,

    // 'for' star_targets 'in' ~ disjunction (('if' disjunction))*
    OP_TOKEN, 612,
    OP_RULE, R_STAR_TARGETS,
    OP_TOKEN, 613,
    OP_CUT, 
    OP_RULE, R_DISJUNCTION,
    OP_RULE, R__LOOP0_115,
    OP_RETURN, A_FOR_IF_CLAUSE_1,

 },
},
{"listcomp",
 R_LISTCOMP,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '[' named_expression for_if_clauses ']'
    OP_TOKEN, LSQB,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_RULE, R_FOR_IF_CLAUSES,
    OP_TOKEN, RSQB,
    OP_RETURN, A_LISTCOMP_0,

 },
},
{"setcomp",
 R_SETCOMP,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '{' named_expression for_if_clauses '}'
    OP_TOKEN, LBRACE,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_RULE, R_FOR_IF_CLAUSES,
    OP_TOKEN, RBRACE,
    OP_RETURN, A_SETCOMP_0,

 },
},
{"genexp",
 R_GENEXP,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '(' (assignment_expression | expression !':=') for_if_clauses ')'
    OP_TOKEN, LPAR,
    OP_RULE, R__TMP_116,
    OP_RULE, R_FOR_IF_CLAUSES,
    OP_TOKEN, RPAR,
    OP_RETURN, A_GENEXP_0,

 },
},
{"dictcomp",
 R_DICTCOMP,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '{' kvpair for_if_clauses '}'
    OP_TOKEN, LBRACE,
    OP_RULE, R_KVPAIR,
    OP_RULE, R_FOR_IF_CLAUSES,
    OP_TOKEN, RBRACE,
    OP_RETURN, A_DICTCOMP_0,

 },
},
{"arguments",
 R_ARGUMENTS,
 1,  // memo
 0,  // leftrec
 {0, -1},
 {
    // args ','? &')'
    OP_RULE, R_ARGS,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_SAVE_MARK, 
    OP_TOKEN, RPAR,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_ARGUMENTS_0,

 },
},
{"args",
 R_ARGS,
 0,  // memo
 0,  // leftrec
 {0, 7, -1},
 {
    // ','.(starred_expression | (assignment_expression | expression !':=') !'=')+ [',' kwargs]
    OP_RULE, R__GATHER_117,
    OP_RULE, R__TMP_119,
    OP_OPTIONAL, 
    OP_RETURN, A_ARGS_0,

    // kwargs
    OP_RULE, R_KWARGS,
    OP_RETURN, A_ARGS_1,

 },
},
{"kwargs",
 R_KWARGS,
 0,  // memo
 0,  // leftrec
 {0, 8, 12, -1},
 {
    // ','.kwarg_or_starred+ ',' ','.kwarg_or_double_starred+
    OP_RULE, R__GATHER_120,
    OP_TOKEN, COMMA,
    OP_RULE, R__GATHER_122,
    OP_RETURN, A_KWARGS_0,

    // ','.kwarg_or_starred+
    OP_RULE, R__GATHER_124,
    OP_RETURN, A_KWARGS_1,

    // ','.kwarg_or_double_starred+
    OP_RULE, R__GATHER_126,
    OP_RETURN, A_KWARGS_2,

 },
},
{"starred_expression",
 R_STARRED_EXPRESSION,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '*' expression
    OP_TOKEN, STAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_STARRED_EXPRESSION_0,

 },
},
{"kwarg_or_starred",
 R_KWARG_OR_STARRED,
 0,  // memo
 0,  // leftrec
 {0, 7, -1},
 {
    // NAME '=' expression
    OP_NAME, 
    OP_TOKEN, EQUAL,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_KWARG_OR_STARRED_0,

    // starred_expression
    OP_RULE, R_STARRED_EXPRESSION,
    OP_RETURN, A_KWARG_OR_STARRED_1,

 },
},
{"kwarg_or_double_starred",
 R_KWARG_OR_DOUBLE_STARRED,
 0,  // memo
 0,  // leftrec
 {0, 7, -1},
 {
    // NAME '=' expression
    OP_NAME, 
    OP_TOKEN, EQUAL,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_KWARG_OR_DOUBLE_STARRED_0,

    // '**' expression
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_KWARG_OR_DOUBLE_STARRED_1,

 },
},
{"star_targets",
 R_STAR_TARGETS,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // star_target !','
    OP_RULE, R_STAR_TARGET,
    OP_SAVE_MARK, 
    OP_TOKEN, COMMA,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_STAR_TARGETS_0,

    // star_target ((',' star_target))* ','?
    OP_RULE, R_STAR_TARGET,
    OP_RULE, R__LOOP0_128,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_TARGETS_1,

 },
},
{"star_targets_list_seq",
 R_STAR_TARGETS_LIST_SEQ,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.star_target+ ','?
    OP_RULE, R__GATHER_129,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_TARGETS_LIST_SEQ_0,

 },
},
{"star_targets_tuple_seq",
 R_STAR_TARGETS_TUPLE_SEQ,
 0,  // memo
 0,  // leftrec
 {0, 9, -1},
 {
    // star_target ((',' star_target))+ ','?
    OP_RULE, R_STAR_TARGET,
    OP_RULE, R__LOOP1_131,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_STAR_TARGETS_TUPLE_SEQ_0,

    // star_target ','
    OP_RULE, R_STAR_TARGET,
    OP_TOKEN, COMMA,
    OP_RETURN, A_STAR_TARGETS_TUPLE_SEQ_1,

 },
},
{"star_target",
 R_STAR_TARGET,
 1,  // memo
 0,  // leftrec
 {0, 6, -1},
 {
    // '*' (!'*' star_target)
    OP_TOKEN, STAR,
    OP_RULE, R__TMP_132,
    OP_RETURN, A_STAR_TARGET_0,

    // target_with_star_atom
    OP_RULE, R_TARGET_WITH_STAR_ATOM,
    OP_RETURN, A_STAR_TARGET_1,

 },
},
{"target_with_star_atom",
 R_TARGET_WITH_STAR_ATOM,
 1,  // memo
 0,  // leftrec
 {0, 11, 25, -1},
 {
    // t_primary '.' NAME !t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, DOT,
    OP_NAME, 
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_TARGET_WITH_STAR_ATOM_0,

    // t_primary '[' slices ']' !t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, LSQB,
    OP_RULE, R_SLICES,
    OP_TOKEN, RSQB,
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_TARGET_WITH_STAR_ATOM_1,

    // star_atom
    OP_RULE, R_STAR_ATOM,
    OP_RETURN, A_TARGET_WITH_STAR_ATOM_2,

 },
},
{"star_atom",
 R_STAR_ATOM,
 0,  // memo
 0,  // leftrec
 {0, 3, 11, 20, -1},
 {
    // NAME
    OP_NAME, 
    OP_RETURN, A_STAR_ATOM_0,

    // '(' target_with_star_atom ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_TARGET_WITH_STAR_ATOM,
    OP_TOKEN, RPAR,
    OP_RETURN, A_STAR_ATOM_1,

    // '(' star_targets_tuple_seq? ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_STAR_TARGETS_TUPLE_SEQ,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_STAR_ATOM_2,

    // '[' star_targets_list_seq? ']'
    OP_TOKEN, LSQB,
    OP_RULE, R_STAR_TARGETS_LIST_SEQ,
    OP_OPTIONAL, 
    OP_TOKEN, RSQB,
    OP_RETURN, A_STAR_ATOM_3,

 },
},
{"single_target",
 R_SINGLE_TARGET,
 0,  // memo
 0,  // leftrec
 {0, 4, 7, -1},
 {
    // single_subscript_attribute_target
    OP_RULE, R_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET,
    OP_RETURN, A_SINGLE_TARGET_0,

    // NAME
    OP_NAME, 
    OP_RETURN, A_SINGLE_TARGET_1,

    // '(' single_target ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_SINGLE_TARGET,
    OP_TOKEN, RPAR,
    OP_RETURN, A_SINGLE_TARGET_2,

 },
},
{"single_subscript_attribute_target",
 R_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET,
 0,  // memo
 0,  // leftrec
 {0, 11, -1},
 {
    // t_primary '.' NAME !t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, DOT,
    OP_NAME, 
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET_0,

    // t_primary '[' slices ']' !t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, LSQB,
    OP_RULE, R_SLICES,
    OP_TOKEN, RSQB,
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET_1,

 },
},
{"t_primary",
 R_T_PRIMARY,
 0,  // memo
 1,  // leftrec
 {0, 11, 25, 35, 50, -1},
 {
    // t_primary '.' NAME &t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, DOT,
    OP_NAME, 
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_T_PRIMARY_0,

    // t_primary '[' slices ']' &t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, LSQB,
    OP_RULE, R_SLICES,
    OP_TOKEN, RSQB,
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_T_PRIMARY_1,

    // t_primary genexp &t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_RULE, R_GENEXP,
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_T_PRIMARY_2,

    // t_primary '(' arguments? ')' &t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, LPAR,
    OP_RULE, R_ARGUMENTS,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_T_PRIMARY_3,

    // atom &t_lookahead
    OP_RULE, R_ATOM,
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_T_PRIMARY_4,

 },
},
{"t_lookahead",
 R_T_LOOKAHEAD,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, -1},
 {
    // '('
    OP_TOKEN, LPAR,
    OP_RETURN, A_T_LOOKAHEAD_0,

    // '['
    OP_TOKEN, LSQB,
    OP_RETURN, A_T_LOOKAHEAD_1,

    // '.'
    OP_TOKEN, DOT,
    OP_RETURN, A_T_LOOKAHEAD_2,

 },
},
{"del_targets",
 R_DEL_TARGETS,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ','.del_target+ ','?
    OP_RULE, R__GATHER_133,
    OP_TOKEN, COMMA,
    OP_OPTIONAL, 
    OP_RETURN, A_DEL_TARGETS_0,

 },
},
{"del_target",
 R_DEL_TARGET,
 1,  // memo
 0,  // leftrec
 {0, 11, 25, -1},
 {
    // t_primary '.' NAME !t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, DOT,
    OP_NAME, 
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_DEL_TARGET_0,

    // t_primary '[' slices ']' !t_lookahead
    OP_RULE, R_T_PRIMARY,
    OP_TOKEN, LSQB,
    OP_RULE, R_SLICES,
    OP_TOKEN, RSQB,
    OP_SAVE_MARK, 
    OP_RULE, R_T_LOOKAHEAD,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A_DEL_TARGET_1,

    // del_t_atom
    OP_RULE, R_DEL_T_ATOM,
    OP_RETURN, A_DEL_TARGET_2,

 },
},
{"del_t_atom",
 R_DEL_T_ATOM,
 0,  // memo
 0,  // leftrec
 {0, 3, 11, 20, -1},
 {
    // NAME
    OP_NAME, 
    OP_RETURN, A_DEL_T_ATOM_0,

    // '(' del_target ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_DEL_TARGET,
    OP_TOKEN, RPAR,
    OP_RETURN, A_DEL_T_ATOM_1,

    // '(' del_targets? ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_DEL_TARGETS,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A_DEL_T_ATOM_2,

    // '[' del_targets? ']'
    OP_TOKEN, LSQB,
    OP_RULE, R_DEL_TARGETS,
    OP_OPTIONAL, 
    OP_TOKEN, RSQB,
    OP_RETURN, A_DEL_T_ATOM_3,

 },
},
{"type_expressions",
 R_TYPE_EXPRESSIONS,
 0,  // memo
 0,  // leftrec
 {0, 16, 26, 36, 48, 54, 60, -1},
 {
    // ','.expression+ ',' '*' expression ',' '**' expression
    OP_RULE, R__GATHER_135,
    OP_TOKEN, COMMA,
    OP_TOKEN, STAR,
    OP_RULE, R_EXPRESSION,
    OP_TOKEN, COMMA,
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_TYPE_EXPRESSIONS_0,

    // ','.expression+ ',' '*' expression
    OP_RULE, R__GATHER_137,
    OP_TOKEN, COMMA,
    OP_TOKEN, STAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_TYPE_EXPRESSIONS_1,

    // ','.expression+ ',' '**' expression
    OP_RULE, R__GATHER_139,
    OP_TOKEN, COMMA,
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_TYPE_EXPRESSIONS_2,

    // '*' expression ',' '**' expression
    OP_TOKEN, STAR,
    OP_RULE, R_EXPRESSION,
    OP_TOKEN, COMMA,
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_TYPE_EXPRESSIONS_3,

    // '*' expression
    OP_TOKEN, STAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_TYPE_EXPRESSIONS_4,

    // '**' expression
    OP_TOKEN, DOUBLESTAR,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A_TYPE_EXPRESSIONS_5,

    // ','.expression+
    OP_RULE, R__GATHER_141,
    OP_RETURN, A_TYPE_EXPRESSIONS_6,

 },
},
{"func_type_comment",
 R_FUNC_TYPE_COMMENT,
 0,  // memo
 0,  // leftrec
 {0, 10, -1},
 {
    // NEWLINE TYPE_COMMENT &(NEWLINE INDENT)
    OP_TOKEN, NEWLINE,
    OP_TOKEN, TYPE_COMMENT,
    OP_SAVE_MARK, 
    OP_RULE, R__TMP_143,
    OP_POS_LOOKAHEAD, 
    OP_RETURN, A_FUNC_TYPE_COMMENT_0,

    // TYPE_COMMENT
    OP_TOKEN, TYPE_COMMENT,
    OP_RETURN, A_FUNC_TYPE_COMMENT_1,

 },
},
{"root",
 R_ROOT,
 0,
 0,
 {0, 3, -1},
 {
    // <Artificial alternative>
    OP_RULE, R_FILE,
    OP_SUCCESS, 

    // <Artificial alternative>
    OP_FAILURE, 

 },
},
{"_loop0_1",
 R__LOOP0_1,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // NEWLINE
    OP_TOKEN, NEWLINE,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_2",
 R__LOOP0_2,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // NEWLINE
    OP_TOKEN, NEWLINE,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_3",
 R__LOOP1_3,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // statement
    OP_RULE, R_STATEMENT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_5",
 R__LOOP0_5,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ';' simple_stmt
    OP_TOKEN, SEMI,
    OP_RULE, R_SIMPLE_STMT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_4",
 R__GATHER_4,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // simple_stmt _loop0_5
    OP_RULE, R_SIMPLE_STMT,
    OP_RULE, R__LOOP0_5,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_6",
 R__TMP_6,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // 'import'
    OP_TOKEN, 606,
    OP_RETURN, A__TMP_6_0,

    // 'from'
    OP_TOKEN, 607,
    OP_RETURN, A__TMP_6_1,

 },
},
{"_tmp_7",
 R__TMP_7,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, -1},
 {
    // 'def'
    OP_TOKEN, 609,
    OP_RETURN, A__TMP_7_0,

    // '@'
    OP_TOKEN, AT,
    OP_RETURN, A__TMP_7_1,

    // ASYNC
    OP_TOKEN, ASYNC,
    OP_RETURN, A__TMP_7_2,

 },
},
{"_tmp_8",
 R__TMP_8,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // 'class'
    OP_TOKEN, 608,
    OP_RETURN, A__TMP_8_0,

    // '@'
    OP_TOKEN, AT,
    OP_RETURN, A__TMP_8_1,

 },
},
{"_tmp_9",
 R__TMP_9,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // 'with'
    OP_TOKEN, 614,
    OP_RETURN, A__TMP_9_0,

    // ASYNC
    OP_TOKEN, ASYNC,
    OP_RETURN, A__TMP_9_1,

 },
},
{"_tmp_10",
 R__TMP_10,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // 'for'
    OP_TOKEN, 612,
    OP_RETURN, A__TMP_10_0,

    // ASYNC
    OP_TOKEN, ASYNC,
    OP_RETURN, A__TMP_10_1,

 },
},
{"_tmp_11",
 R__TMP_11,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '=' annotated_rhs
    OP_TOKEN, EQUAL,
    OP_RULE, R_ANNOTATED_RHS,
    OP_RETURN, A__TMP_11_0,

 },
},
{"_tmp_12",
 R__TMP_12,
 0,  // memo
 0,  // leftrec
 {0, 8, -1},
 {
    // '(' single_target ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_SINGLE_TARGET,
    OP_TOKEN, RPAR,
    OP_RETURN, A__TMP_12_0,

    // single_subscript_attribute_target
    OP_RULE, R_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET,
    OP_RETURN, A__TMP_12_1,

 },
},
{"_tmp_13",
 R__TMP_13,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '=' annotated_rhs
    OP_TOKEN, EQUAL,
    OP_RULE, R_ANNOTATED_RHS,
    OP_RETURN, A__TMP_13_0,

 },
},
{"_loop1_14",
 R__LOOP1_14,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // (star_targets '=')
    OP_RULE, R__TMP_144,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_tmp_15",
 R__TMP_15,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // yield_expr
    OP_RULE, R_YIELD_EXPR,
    OP_RETURN, A__TMP_15_0,

    // star_expressions
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_RETURN, A__TMP_15_1,

 },
},
{"_tmp_16",
 R__TMP_16,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // yield_expr
    OP_RULE, R_YIELD_EXPR,
    OP_RETURN, A__TMP_16_0,

    // star_expressions
    OP_RULE, R_STAR_EXPRESSIONS,
    OP_RETURN, A__TMP_16_1,

 },
},
{"_tmp_17",
 R__TMP_17,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'from' expression
    OP_TOKEN, 607,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A__TMP_17_0,

 },
},
{"_loop0_19",
 R__LOOP0_19,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // ',' NAME
    OP_TOKEN, COMMA,
    OP_NAME, 
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_18",
 R__GATHER_18,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME _loop0_19
    OP_NAME, 
    OP_RULE, R__LOOP0_19,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_21",
 R__LOOP0_21,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // ',' NAME
    OP_TOKEN, COMMA,
    OP_NAME, 
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_20",
 R__GATHER_20,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NAME _loop0_21
    OP_NAME, 
    OP_RULE, R__LOOP0_21,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_22",
 R__TMP_22,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // ';'
    OP_TOKEN, SEMI,
    OP_RETURN, A__TMP_22_0,

    // NEWLINE
    OP_TOKEN, NEWLINE,
    OP_RETURN, A__TMP_22_1,

 },
},
{"_tmp_23",
 R__TMP_23,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ',' expression
    OP_TOKEN, COMMA,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A__TMP_23_0,

 },
},
{"_loop0_24",
 R__LOOP0_24,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // ('.' | '...')
    OP_RULE, R__TMP_145,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_25",
 R__LOOP1_25,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // ('.' | '...')
    OP_RULE, R__TMP_146,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_27",
 R__LOOP0_27,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' import_from_as_name
    OP_TOKEN, COMMA,
    OP_RULE, R_IMPORT_FROM_AS_NAME,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_26",
 R__GATHER_26,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // import_from_as_name _loop0_27
    OP_RULE, R_IMPORT_FROM_AS_NAME,
    OP_RULE, R__LOOP0_27,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_28",
 R__TMP_28,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'as' NAME
    OP_TOKEN, 615,
    OP_NAME, 
    OP_RETURN, A__TMP_28_0,

 },
},
{"_loop0_30",
 R__LOOP0_30,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' dotted_as_name
    OP_TOKEN, COMMA,
    OP_RULE, R_DOTTED_AS_NAME,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_29",
 R__GATHER_29,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // dotted_as_name _loop0_30
    OP_RULE, R_DOTTED_AS_NAME,
    OP_RULE, R__LOOP0_30,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_31",
 R__TMP_31,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'as' NAME
    OP_TOKEN, 615,
    OP_NAME, 
    OP_RETURN, A__TMP_31_0,

 },
},
{"_loop1_32",
 R__LOOP1_32,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // ('@' named_expression NEWLINE)
    OP_RULE, R__TMP_147,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_tmp_33",
 R__TMP_33,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '(' arguments? ')'
    OP_TOKEN, LPAR,
    OP_RULE, R_ARGUMENTS,
    OP_OPTIONAL, 
    OP_TOKEN, RPAR,
    OP_RETURN, A__TMP_33_0,

 },
},
{"_tmp_34",
 R__TMP_34,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '->' expression
    OP_TOKEN, RARROW,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A__TMP_34_0,

 },
},
{"_tmp_35",
 R__TMP_35,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '->' expression
    OP_TOKEN, RARROW,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A__TMP_35_0,

 },
},
{"_loop0_36",
 R__LOOP0_36,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_no_default
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_37",
 R__LOOP0_37,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_with_default
    OP_RULE, R_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_38",
 R__LOOP0_38,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_with_default
    OP_RULE, R_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_39",
 R__LOOP1_39,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_no_default
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_40",
 R__LOOP0_40,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_with_default
    OP_RULE, R_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_41",
 R__LOOP1_41,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_with_default
    OP_RULE, R_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_42",
 R__LOOP1_42,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_no_default
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_43",
 R__LOOP1_43,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_no_default
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_44",
 R__LOOP0_44,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_no_default
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_45",
 R__LOOP1_45,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_with_default
    OP_RULE, R_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_46",
 R__LOOP0_46,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_no_default
    OP_RULE, R_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_47",
 R__LOOP1_47,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_with_default
    OP_RULE, R_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_48",
 R__LOOP0_48,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_maybe_default
    OP_RULE, R_PARAM_MAYBE_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_49",
 R__LOOP0_49,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_maybe_default
    OP_RULE, R_PARAM_MAYBE_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_50",
 R__LOOP1_50,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // param_maybe_default
    OP_RULE, R_PARAM_MAYBE_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_52",
 R__LOOP0_52,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' with_item
    OP_TOKEN, COMMA,
    OP_RULE, R_WITH_ITEM,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_51",
 R__GATHER_51,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // with_item _loop0_52
    OP_RULE, R_WITH_ITEM,
    OP_RULE, R__LOOP0_52,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_54",
 R__LOOP0_54,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' with_item
    OP_TOKEN, COMMA,
    OP_RULE, R_WITH_ITEM,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_53",
 R__GATHER_53,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // with_item _loop0_54
    OP_RULE, R_WITH_ITEM,
    OP_RULE, R__LOOP0_54,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_56",
 R__LOOP0_56,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' with_item
    OP_TOKEN, COMMA,
    OP_RULE, R_WITH_ITEM,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_55",
 R__GATHER_55,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // with_item _loop0_56
    OP_RULE, R_WITH_ITEM,
    OP_RULE, R__LOOP0_56,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_58",
 R__LOOP0_58,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' with_item
    OP_TOKEN, COMMA,
    OP_RULE, R_WITH_ITEM,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_57",
 R__GATHER_57,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // with_item _loop0_58
    OP_RULE, R_WITH_ITEM,
    OP_RULE, R__LOOP0_58,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_59",
 R__TMP_59,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, -1},
 {
    // ','
    OP_TOKEN, COMMA,
    OP_RETURN, A__TMP_59_0,

    // ')'
    OP_TOKEN, RPAR,
    OP_RETURN, A__TMP_59_1,

    // ':'
    OP_TOKEN, COLON,
    OP_RETURN, A__TMP_59_2,

 },
},
{"_loop1_60",
 R__LOOP1_60,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // except_block
    OP_RULE, R_EXCEPT_BLOCK,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_61",
 R__LOOP1_61,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // except_star_block
    OP_RULE, R_EXCEPT_STAR_BLOCK,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_tmp_62",
 R__TMP_62,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'as' NAME
    OP_TOKEN, 615,
    OP_NAME, 
    OP_RETURN, A__TMP_62_0,

 },
},
{"_tmp_63",
 R__TMP_63,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'as' NAME
    OP_TOKEN, 615,
    OP_NAME, 
    OP_RETURN, A__TMP_63_0,

 },
},
{"_loop1_64",
 R__LOOP1_64,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // case_block
    OP_RULE, R_CASE_BLOCK,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_66",
 R__LOOP0_66,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // '|' closed_pattern
    OP_TOKEN, VBAR,
    OP_RULE, R_CLOSED_PATTERN,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_65",
 R__GATHER_65,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // closed_pattern _loop0_66
    OP_RULE, R_CLOSED_PATTERN,
    OP_RULE, R__LOOP0_66,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_67",
 R__TMP_67,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // '+'
    OP_TOKEN, PLUS,
    OP_RETURN, A__TMP_67_0,

    // '-'
    OP_TOKEN, MINUS,
    OP_RETURN, A__TMP_67_1,

 },
},
{"_tmp_68",
 R__TMP_68,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // '+'
    OP_TOKEN, PLUS,
    OP_RETURN, A__TMP_68_0,

    // '-'
    OP_TOKEN, MINUS,
    OP_RETURN, A__TMP_68_1,

 },
},
{"_tmp_69",
 R__TMP_69,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, -1},
 {
    // '.'
    OP_TOKEN, DOT,
    OP_RETURN, A__TMP_69_0,

    // '('
    OP_TOKEN, LPAR,
    OP_RETURN, A__TMP_69_1,

    // '='
    OP_TOKEN, EQUAL,
    OP_RETURN, A__TMP_69_2,

 },
},
{"_tmp_70",
 R__TMP_70,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, -1},
 {
    // '.'
    OP_TOKEN, DOT,
    OP_RETURN, A__TMP_70_0,

    // '('
    OP_TOKEN, LPAR,
    OP_RETURN, A__TMP_70_1,

    // '='
    OP_TOKEN, EQUAL,
    OP_RETURN, A__TMP_70_2,

 },
},
{"_loop0_72",
 R__LOOP0_72,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' maybe_star_pattern
    OP_TOKEN, COMMA,
    OP_RULE, R_MAYBE_STAR_PATTERN,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_71",
 R__GATHER_71,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // maybe_star_pattern _loop0_72
    OP_RULE, R_MAYBE_STAR_PATTERN,
    OP_RULE, R__LOOP0_72,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_74",
 R__LOOP0_74,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' key_value_pattern
    OP_TOKEN, COMMA,
    OP_RULE, R_KEY_VALUE_PATTERN,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_73",
 R__GATHER_73,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // key_value_pattern _loop0_74
    OP_RULE, R_KEY_VALUE_PATTERN,
    OP_RULE, R__LOOP0_74,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_75",
 R__TMP_75,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // literal_expr
    OP_RULE, R_LITERAL_EXPR,
    OP_RETURN, A__TMP_75_0,

    // attr
    OP_RULE, R_ATTR,
    OP_RETURN, A__TMP_75_1,

 },
},
{"_loop0_77",
 R__LOOP0_77,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' pattern
    OP_TOKEN, COMMA,
    OP_RULE, R_PATTERN,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_76",
 R__GATHER_76,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // pattern _loop0_77
    OP_RULE, R_PATTERN,
    OP_RULE, R__LOOP0_77,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_79",
 R__LOOP0_79,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' keyword_pattern
    OP_TOKEN, COMMA,
    OP_RULE, R_KEYWORD_PATTERN,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_78",
 R__GATHER_78,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // keyword_pattern _loop0_79
    OP_RULE, R_KEYWORD_PATTERN,
    OP_RULE, R__LOOP0_79,
    OP_LOOP_ITERATE, 

 },
},
{"_loop1_80",
 R__LOOP1_80,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // (',' expression)
    OP_RULE, R__TMP_148,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_81",
 R__LOOP1_81,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // (',' star_expression)
    OP_RULE, R__TMP_149,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_83",
 R__LOOP0_83,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' star_named_expression
    OP_TOKEN, COMMA,
    OP_RULE, R_STAR_NAMED_EXPRESSION,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_82",
 R__GATHER_82,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // star_named_expression _loop0_83
    OP_RULE, R_STAR_NAMED_EXPRESSION,
    OP_RULE, R__LOOP0_83,
    OP_LOOP_ITERATE, 

 },
},
{"_loop1_84",
 R__LOOP1_84,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // ('or' conjunction)
    OP_RULE, R__TMP_150,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_85",
 R__LOOP1_85,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // ('and' inversion)
    OP_RULE, R__TMP_151,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_86",
 R__LOOP1_86,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // compare_op_bitwise_or_pair
    OP_RULE, R_COMPARE_OP_BITWISE_OR_PAIR,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_tmp_87",
 R__TMP_87,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '!='
    OP_TOKEN, NOTEQUAL,
    OP_RETURN, A__TMP_87_0,

 },
},
{"_loop0_89",
 R__LOOP0_89,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' (slice | starred_expression)
    OP_TOKEN, COMMA,
    OP_RULE, R__TMP_152,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_88",
 R__GATHER_88,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // (slice | starred_expression) _loop0_89
    OP_RULE, R__TMP_152,
    OP_RULE, R__LOOP0_89,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_90",
 R__TMP_90,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ':' expression?
    OP_TOKEN, COLON,
    OP_RULE, R_EXPRESSION,
    OP_OPTIONAL, 
    OP_RETURN, A__TMP_90_0,

 },
},
{"_tmp_91",
 R__TMP_91,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, -1},
 {
    // tuple
    OP_RULE, R_TUPLE,
    OP_RETURN, A__TMP_91_0,

    // group
    OP_RULE, R_GROUP,
    OP_RETURN, A__TMP_91_1,

    // genexp
    OP_RULE, R_GENEXP,
    OP_RETURN, A__TMP_91_2,

 },
},
{"_tmp_92",
 R__TMP_92,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // list
    OP_RULE, R_LIST,
    OP_RETURN, A__TMP_92_0,

    // listcomp
    OP_RULE, R_LISTCOMP,
    OP_RETURN, A__TMP_92_1,

 },
},
{"_tmp_93",
 R__TMP_93,
 0,  // memo
 0,  // leftrec
 {0, 4, 8, 12, -1},
 {
    // dict
    OP_RULE, R_DICT,
    OP_RETURN, A__TMP_93_0,

    // set
    OP_RULE, R_SET,
    OP_RETURN, A__TMP_93_1,

    // dictcomp
    OP_RULE, R_DICTCOMP,
    OP_RETURN, A__TMP_93_2,

    // setcomp
    OP_RULE, R_SETCOMP,
    OP_RETURN, A__TMP_93_3,

 },
},
{"_tmp_94",
 R__TMP_94,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // yield_expr
    OP_RULE, R_YIELD_EXPR,
    OP_RETURN, A__TMP_94_0,

    // named_expression
    OP_RULE, R_NAMED_EXPRESSION,
    OP_RETURN, A__TMP_94_1,

 },
},
{"_loop0_95",
 R__LOOP0_95,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_no_default
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_96",
 R__LOOP0_96,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_with_default
    OP_RULE, R_LAMBDA_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_97",
 R__LOOP0_97,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_with_default
    OP_RULE, R_LAMBDA_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_98",
 R__LOOP1_98,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_no_default
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_99",
 R__LOOP0_99,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_with_default
    OP_RULE, R_LAMBDA_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_100",
 R__LOOP1_100,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_with_default
    OP_RULE, R_LAMBDA_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_101",
 R__LOOP1_101,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_no_default
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_102",
 R__LOOP1_102,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_no_default
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_103",
 R__LOOP0_103,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_no_default
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_104",
 R__LOOP1_104,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_with_default
    OP_RULE, R_LAMBDA_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_105",
 R__LOOP0_105,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_no_default
    OP_RULE, R_LAMBDA_PARAM_NO_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_106",
 R__LOOP1_106,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_with_default
    OP_RULE, R_LAMBDA_PARAM_WITH_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_107",
 R__LOOP0_107,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_maybe_default
    OP_RULE, R_LAMBDA_PARAM_MAYBE_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop1_108",
 R__LOOP1_108,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // lambda_param_maybe_default
    OP_RULE, R_LAMBDA_PARAM_MAYBE_DEFAULT,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop1_109",
 R__LOOP1_109,
 0,  // memo
 0,  // leftrec
 {0, 2, -1},
 {
    // STRING
    OP_STRING, 
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_tmp_110",
 R__TMP_110,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // star_named_expression ',' star_named_expressions?
    OP_RULE, R_STAR_NAMED_EXPRESSION,
    OP_TOKEN, COMMA,
    OP_RULE, R_STAR_NAMED_EXPRESSIONS,
    OP_OPTIONAL, 
    OP_RETURN, A__TMP_110_0,

 },
},
{"_loop0_112",
 R__LOOP0_112,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' double_starred_kvpair
    OP_TOKEN, COMMA,
    OP_RULE, R_DOUBLE_STARRED_KVPAIR,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_111",
 R__GATHER_111,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // double_starred_kvpair _loop0_112
    OP_RULE, R_DOUBLE_STARRED_KVPAIR,
    OP_RULE, R__LOOP0_112,
    OP_LOOP_ITERATE, 

 },
},
{"_loop1_113",
 R__LOOP1_113,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // for_if_clause
    OP_RULE, R_FOR_IF_CLAUSE,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_loop0_114",
 R__LOOP0_114,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // ('if' disjunction)
    OP_RULE, R__TMP_153,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_115",
 R__LOOP0_115,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // ('if' disjunction)
    OP_RULE, R__TMP_154,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_tmp_116",
 R__TMP_116,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // assignment_expression
    OP_RULE, R_ASSIGNMENT_EXPRESSION,
    OP_RETURN, A__TMP_116_0,

    // expression !':='
    OP_RULE, R_EXPRESSION,
    OP_SAVE_MARK, 
    OP_TOKEN, COLONEQUAL,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A__TMP_116_1,

 },
},
{"_loop0_118",
 R__LOOP0_118,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' (starred_expression | (assignment_expression | expression !':=') !'=')
    OP_TOKEN, COMMA,
    OP_RULE, R__TMP_155,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_117",
 R__GATHER_117,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // (starred_expression | (assignment_expression | expression !':=') !'=') _loop0_118
    OP_RULE, R__TMP_155,
    OP_RULE, R__LOOP0_118,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_119",
 R__TMP_119,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ',' kwargs
    OP_TOKEN, COMMA,
    OP_RULE, R_KWARGS,
    OP_RETURN, A__TMP_119_0,

 },
},
{"_loop0_121",
 R__LOOP0_121,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' kwarg_or_starred
    OP_TOKEN, COMMA,
    OP_RULE, R_KWARG_OR_STARRED,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_120",
 R__GATHER_120,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // kwarg_or_starred _loop0_121
    OP_RULE, R_KWARG_OR_STARRED,
    OP_RULE, R__LOOP0_121,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_123",
 R__LOOP0_123,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' kwarg_or_double_starred
    OP_TOKEN, COMMA,
    OP_RULE, R_KWARG_OR_DOUBLE_STARRED,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_122",
 R__GATHER_122,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // kwarg_or_double_starred _loop0_123
    OP_RULE, R_KWARG_OR_DOUBLE_STARRED,
    OP_RULE, R__LOOP0_123,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_125",
 R__LOOP0_125,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' kwarg_or_starred
    OP_TOKEN, COMMA,
    OP_RULE, R_KWARG_OR_STARRED,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_124",
 R__GATHER_124,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // kwarg_or_starred _loop0_125
    OP_RULE, R_KWARG_OR_STARRED,
    OP_RULE, R__LOOP0_125,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_127",
 R__LOOP0_127,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' kwarg_or_double_starred
    OP_TOKEN, COMMA,
    OP_RULE, R_KWARG_OR_DOUBLE_STARRED,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_126",
 R__GATHER_126,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // kwarg_or_double_starred _loop0_127
    OP_RULE, R_KWARG_OR_DOUBLE_STARRED,
    OP_RULE, R__LOOP0_127,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_128",
 R__LOOP0_128,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // (',' star_target)
    OP_RULE, R__TMP_156,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_loop0_130",
 R__LOOP0_130,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' star_target
    OP_TOKEN, COMMA,
    OP_RULE, R_STAR_TARGET,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_129",
 R__GATHER_129,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // star_target _loop0_130
    OP_RULE, R_STAR_TARGET,
    OP_RULE, R__LOOP0_130,
    OP_LOOP_ITERATE, 

 },
},
{"_loop1_131",
 R__LOOP1_131,
 0,  // memo
 0,  // leftrec
 {0, 3, -1},
 {
    // (',' star_target)
    OP_RULE, R__TMP_157,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT_NONEMPTY, 

 },
},
{"_tmp_132",
 R__TMP_132,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // !'*' star_target
    OP_SAVE_MARK, 
    OP_TOKEN, STAR,
    OP_NEG_LOOKAHEAD, 
    OP_RULE, R_STAR_TARGET,
    OP_RETURN, A__TMP_132_0,

 },
},
{"_loop0_134",
 R__LOOP0_134,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' del_target
    OP_TOKEN, COMMA,
    OP_RULE, R_DEL_TARGET,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_133",
 R__GATHER_133,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // del_target _loop0_134
    OP_RULE, R_DEL_TARGET,
    OP_RULE, R__LOOP0_134,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_136",
 R__LOOP0_136,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' expression
    OP_TOKEN, COMMA,
    OP_RULE, R_EXPRESSION,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_135",
 R__GATHER_135,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // expression _loop0_136
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__LOOP0_136,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_138",
 R__LOOP0_138,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' expression
    OP_TOKEN, COMMA,
    OP_RULE, R_EXPRESSION,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_137",
 R__GATHER_137,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // expression _loop0_138
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__LOOP0_138,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_140",
 R__LOOP0_140,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' expression
    OP_TOKEN, COMMA,
    OP_RULE, R_EXPRESSION,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_139",
 R__GATHER_139,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // expression _loop0_140
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__LOOP0_140,
    OP_LOOP_ITERATE, 

 },
},
{"_loop0_142",
 R__LOOP0_142,
 0,  // memo
 0,  // leftrec
 {0, 5, -1},
 {
    // ',' expression
    OP_TOKEN, COMMA,
    OP_RULE, R_EXPRESSION,
    OP_LOOP_ITERATE, 

    // <Artificial alternative>
    OP_LOOP_COLLECT, 

 },
},
{"_gather_141",
 R__GATHER_141,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // expression _loop0_142
    OP_RULE, R_EXPRESSION,
    OP_RULE, R__LOOP0_142,
    OP_LOOP_ITERATE, 

 },
},
{"_tmp_143",
 R__TMP_143,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // NEWLINE INDENT
    OP_TOKEN, NEWLINE,
    OP_TOKEN, INDENT,
    OP_RETURN, A__TMP_143_0,

 },
},
{"_tmp_144",
 R__TMP_144,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // star_targets '='
    OP_RULE, R_STAR_TARGETS,
    OP_TOKEN, EQUAL,
    OP_RETURN, A__TMP_144_0,

 },
},
{"_tmp_145",
 R__TMP_145,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // '.'
    OP_TOKEN, DOT,
    OP_RETURN, A__TMP_145_0,

    // '...'
    OP_TOKEN, ELLIPSIS,
    OP_RETURN, A__TMP_145_1,

 },
},
{"_tmp_146",
 R__TMP_146,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // '.'
    OP_TOKEN, DOT,
    OP_RETURN, A__TMP_146_0,

    // '...'
    OP_TOKEN, ELLIPSIS,
    OP_RETURN, A__TMP_146_1,

 },
},
{"_tmp_147",
 R__TMP_147,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // '@' named_expression NEWLINE
    OP_TOKEN, AT,
    OP_RULE, R_NAMED_EXPRESSION,
    OP_TOKEN, NEWLINE,
    OP_RETURN, A__TMP_147_0,

 },
},
{"_tmp_148",
 R__TMP_148,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ',' expression
    OP_TOKEN, COMMA,
    OP_RULE, R_EXPRESSION,
    OP_RETURN, A__TMP_148_0,

 },
},
{"_tmp_149",
 R__TMP_149,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ',' star_expression
    OP_TOKEN, COMMA,
    OP_RULE, R_STAR_EXPRESSION,
    OP_RETURN, A__TMP_149_0,

 },
},
{"_tmp_150",
 R__TMP_150,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'or' conjunction
    OP_TOKEN, 624,
    OP_RULE, R_CONJUNCTION,
    OP_RETURN, A__TMP_150_0,

 },
},
{"_tmp_151",
 R__TMP_151,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'and' inversion
    OP_TOKEN, 625,
    OP_RULE, R_INVERSION,
    OP_RETURN, A__TMP_151_0,

 },
},
{"_tmp_152",
 R__TMP_152,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // slice
    OP_RULE, R_SLICE,
    OP_RETURN, A__TMP_152_0,

    // starred_expression
    OP_RULE, R_STARRED_EXPRESSION,
    OP_RETURN, A__TMP_152_1,

 },
},
{"_tmp_153",
 R__TMP_153,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'if' disjunction
    OP_TOKEN, 603,
    OP_RULE, R_DISJUNCTION,
    OP_RETURN, A__TMP_153_0,

 },
},
{"_tmp_154",
 R__TMP_154,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // 'if' disjunction
    OP_TOKEN, 603,
    OP_RULE, R_DISJUNCTION,
    OP_RETURN, A__TMP_154_0,

 },
},
{"_tmp_155",
 R__TMP_155,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // starred_expression
    OP_RULE, R_STARRED_EXPRESSION,
    OP_RETURN, A__TMP_155_0,

    // (assignment_expression | expression !':=') !'='
    OP_RULE, R__TMP_158,
    OP_SAVE_MARK, 
    OP_TOKEN, EQUAL,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A__TMP_155_1,

 },
},
{"_tmp_156",
 R__TMP_156,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ',' star_target
    OP_TOKEN, COMMA,
    OP_RULE, R_STAR_TARGET,
    OP_RETURN, A__TMP_156_0,

 },
},
{"_tmp_157",
 R__TMP_157,
 0,  // memo
 0,  // leftrec
 {0, -1},
 {
    // ',' star_target
    OP_TOKEN, COMMA,
    OP_RULE, R_STAR_TARGET,
    OP_RETURN, A__TMP_157_0,

 },
},
{"_tmp_158",
 R__TMP_158,
 0,  // memo
 0,  // leftrec
 {0, 4, -1},
 {
    // assignment_expression
    OP_RULE, R_ASSIGNMENT_EXPRESSION,
    OP_RETURN, A__TMP_158_0,

    // expression !':='
    OP_RULE, R_EXPRESSION,
    OP_SAVE_MARK, 
    OP_TOKEN, COLONEQUAL,
    OP_NEG_LOOKAHEAD, 
    OP_RETURN, A__TMP_158_1,

 },
},
};

static void *
call_action(Parser *p, Frame *_f, int _iaction)
{
    assert(p->mark > 0);
    Token *_t = p->tokens[_f->mark];
    int _start_lineno = _t->lineno;
    int _start_col_offset = _t->col_offset;
    _t = p->tokens[p->mark - 1];
    int _end_lineno = _t->end_lineno;
    int _end_col_offset = _t->end_col_offset;

    switch (_iaction) {
    case A_FILE_0:
        return _PyPegen_make_module ( p , _f->vals[0] );
    case A_INTERACTIVE_0:
        return _PyAST_Interactive ( ((asdl_stmt_seq*)_f->vals[0]) , p -> arena );
    case A_EVAL_0:
        return _PyAST_Expression ( ((expr_ty)_f->vals[0]) , p -> arena );
    case A_FUNC_TYPE_0:
        return _PyAST_FunctionType ( _f->vals[1] , ((expr_ty)_f->vals[4]) , p -> arena );
    case A_FSTRING_0:
    case A_ANNOTATED_RHS_0:
    case A_ANNOTATED_RHS_1:
    case A_DOTTED_NAME_1:
    case A_SUBJECT_EXPR_1:
    case A_LITERAL_EXPR_0:
    case A_LITERAL_EXPR_1:
    case A_LITERAL_EXPR_2:
    case A_SIGNED_REAL_NUMBER_0:
    case A_NAME_OR_ATTR_0:
    case A_NAME_OR_ATTR_1:
    case A_EXPRESSIONS_2:
    case A_EXPRESSION_1:
    case A_EXPRESSION_2:
    case A_STAR_EXPRESSIONS_2:
    case A_STAR_EXPRESSION_1:
    case A_STAR_NAMED_EXPRESSION_1:
    case A_NAMED_EXPRESSION_0:
    case A_NAMED_EXPRESSION_1:
    case A_DISJUNCTION_1:
    case A_CONJUNCTION_1:
    case A_INVERSION_1:
    case A_COMPARISON_1:
    case A_BITWISE_OR_1:
    case A_BITWISE_XOR_1:
    case A_BITWISE_AND_1:
    case A_SHIFT_EXPR_2:
    case A_SUM_2:
    case A_TERM_5:
    case A_FACTOR_3:
    case A_POWER_1:
    case A_AWAIT_PRIMARY_1:
    case A_PRIMARY_4:
    case A_SLICES_0:
    case A_SLICE_1:
    case A_ATOM_0:
    case A_ARGUMENTS_0:
    case A_STAR_TARGETS_0:
    case A_STAR_TARGET_1:
    case A_TARGET_WITH_STAR_ATOM_2:
    case A_SINGLE_TARGET_0:
    case A_T_PRIMARY_4:
    case A_DEL_TARGET_2:
    case A__TMP_12_1:
    case A__TMP_15_0:
    case A__TMP_15_1:
    case A__TMP_16_0:
    case A__TMP_16_1:
    case A__GATHER_18_0:
    case A__GATHER_20_0:
    case A__TMP_75_0:
    case A__TMP_75_1:
    case A__GATHER_82_0:
    case A__TMP_91_0:
    case A__TMP_91_1:
    case A__TMP_91_2:
    case A__TMP_92_0:
    case A__TMP_92_1:
    case A__TMP_93_0:
    case A__TMP_93_1:
    case A__TMP_93_2:
    case A__TMP_93_3:
    case A__TMP_94_0:
    case A__TMP_94_1:
    case A__TMP_116_0:
    case A__TMP_116_1:
    case A__GATHER_129_0:
    case A__GATHER_133_0:
    case A__GATHER_135_0:
    case A__GATHER_137_0:
    case A__GATHER_139_0:
    case A__GATHER_141_0:
    case A__TMP_144_0:
    case A__TMP_152_0:
    case A__TMP_152_1:
    case A__TMP_155_0:
    case A__TMP_158_0:
    case A__TMP_158_1:
        return ((expr_ty)_f->vals[0]);
    case A_STATEMENTS_0:
        return ( asdl_stmt_seq * ) _PyPegen_seq_flatten ( p , _f->vals[0] );
    case A_STATEMENT_0:
    case A_STATEMENT_NEWLINE_0:
    case A_SIMPLE_STMTS_0:
        return ( asdl_stmt_seq * ) _PyPegen_singleton_seq ( p , ((stmt_ty)_f->vals[0]) );
    case A_STATEMENT_1:
    case A_STATEMENT_NEWLINE_1:
    case A_SIMPLE_STMTS_1:
    case A_BLOCK_1:
        return ((asdl_stmt_seq*)_f->vals[0]);
    case A_STATEMENT_NEWLINE_2:
        return ( asdl_stmt_seq * ) _PyPegen_singleton_seq ( p , CHECK ( stmt_ty , _PyAST_Pass ( EXTRA ) ) );
    case A_STATEMENT_NEWLINE_3:
        return _PyPegen_interactive_exit ( p );
    case A_SIMPLE_STMT_0:
    case A_COMPOUND_STMT_7:
    case A_IMPORT_STMT_0:
    case A_IMPORT_STMT_1:
    case A_CLASS_DEF_1:
    case A_FUNCTION_DEF_1:
    case A__GATHER_4_0:
        return ((stmt_ty)_f->vals[0]);
    case A_SIMPLE_STMT_1:
    case A_YIELD_STMT_0:
        return _PyAST_Expr ( ((expr_ty)_f->vals[0]) , EXTRA );
    case A_SIMPLE_STMT_2:
    case A_SIMPLE_STMT_3:
    case A_SIMPLE_STMT_4:
    case A_SIMPLE_STMT_6:
    case A_SIMPLE_STMT_7:
    case A_SIMPLE_STMT_8:
    case A_SIMPLE_STMT_11:
    case A_SIMPLE_STMT_12:
    case A_COMPOUND_STMT_0:
    case A_COMPOUND_STMT_1:
    case A_COMPOUND_STMT_2:
    case A_COMPOUND_STMT_3:
    case A_COMPOUND_STMT_4:
    case A_COMPOUND_STMT_5:
    case A_COMPOUND_STMT_6:
    case A_SIGNED_NUMBER_0:
    case A_MAYBE_SEQUENCE_PATTERN_0:
    case A_ITEMS_PATTERN_0:
    case A_KEYWORD_PATTERNS_0:
    case A_ATOM_4:
    case A_ATOM_5:
    case A_ATOM_6:
    case A_ATOM_7:
    case A_ATOM_8:
    case A_DOUBLE_STARRED_KVPAIRS_0:
    case A_KWARGS_1:
    case A_KWARGS_2:
    case A_FUNC_TYPE_COMMENT_1:
    case A__TMP_7_2:
    case A__TMP_9_1:
    case A__TMP_10_1:
    case A__TMP_22_1:
    case A__GATHER_88_0:
    case A__GATHER_117_0:
    case A__TMP_132_0:
    case A__TMP_143_0:
    case A__TMP_155_1:
        return _f->vals[0];
    case A_SIMPLE_STMT_5:
        return _PyAST_Pass ( EXTRA );
    case A_SIMPLE_STMT_9:
        return _PyAST_Break ( EXTRA );
    case A_SIMPLE_STMT_10:
        return _PyAST_Continue ( EXTRA );
    case A_ASSIGNMENT_0:
        return CHECK_VERSION ( stmt_ty , 6 , "Variable annotation syntax is" , _PyAST_AnnAssign ( CHECK ( expr_ty , _PyPegen_set_expr_context ( p , ((expr_ty)_f->vals[0]) , Store ) ) , ((expr_ty)_f->vals[2]) , _f->vals[3] , 1 , EXTRA ) );
    case A_ASSIGNMENT_1:
        return CHECK_VERSION ( stmt_ty , 6 , "Variable annotations syntax is" , _PyAST_AnnAssign ( _f->vals[0] , ((expr_ty)_f->vals[2]) , _f->vals[3] , 0 , EXTRA ) );
    case A_ASSIGNMENT_2:
        return _PyAST_Assign ( ((asdl_expr_seq*)_f->vals[0]) , _f->vals[1] , NEW_TYPE_COMMENT ( p , _f->vals[2] ) , EXTRA );
    case A_ASSIGNMENT_3:
        return _PyAST_AugAssign ( ((expr_ty)_f->vals[0]) , ((AugOperator*)_f->vals[1]) -> kind , _f->vals[2] , EXTRA );
    case A_AUGASSIGN_0:
        return _PyPegen_augoperator ( p , Add );
    case A_AUGASSIGN_1:
        return _PyPegen_augoperator ( p , Sub );
    case A_AUGASSIGN_2:
        return _PyPegen_augoperator ( p , Mult );
    case A_AUGASSIGN_3:
        return CHECK_VERSION ( AugOperator * , 5 , "The '@' operator is" , _PyPegen_augoperator ( p , MatMult ) );
    case A_AUGASSIGN_4:
        return _PyPegen_augoperator ( p , Div );
    case A_AUGASSIGN_5:
        return _PyPegen_augoperator ( p , Mod );
    case A_AUGASSIGN_6:
        return _PyPegen_augoperator ( p , BitAnd );
    case A_AUGASSIGN_7:
        return _PyPegen_augoperator ( p , BitOr );
    case A_AUGASSIGN_8:
        return _PyPegen_augoperator ( p , BitXor );
    case A_AUGASSIGN_9:
        return _PyPegen_augoperator ( p , LShift );
    case A_AUGASSIGN_10:
        return _PyPegen_augoperator ( p , RShift );
    case A_AUGASSIGN_11:
        return _PyPegen_augoperator ( p , Pow );
    case A_AUGASSIGN_12:
        return _PyPegen_augoperator ( p , FloorDiv );
    case A_RETURN_STMT_0:
        return _PyAST_Return ( _f->vals[1] , EXTRA );
    case A_RAISE_STMT_0:
        return _PyAST_Raise ( ((expr_ty)_f->vals[1]) , _f->vals[2] , EXTRA );
    case A_RAISE_STMT_1:
        return _PyAST_Raise ( NULL , NULL , EXTRA );
    case A_GLOBAL_STMT_0:
        return _PyAST_Global ( CHECK ( asdl_identifier_seq * , _PyPegen_map_names_to_ids ( p , ((asdl_expr_seq*)_f->vals[1]) ) ) , EXTRA );
    case A_NONLOCAL_STMT_0:
        return _PyAST_Nonlocal ( CHECK ( asdl_identifier_seq * , _PyPegen_map_names_to_ids ( p , ((asdl_expr_seq*)_f->vals[1]) ) ) , EXTRA );
    case A_DEL_STMT_0:
        return _PyAST_Delete ( ((asdl_expr_seq*)_f->vals[1]) , EXTRA );
    case A_ASSERT_STMT_0:
        return _PyAST_Assert ( ((expr_ty)_f->vals[1]) , _f->vals[2] , EXTRA );
    case A_IMPORT_NAME_0:
        return _PyAST_Import ( ((asdl_alias_seq*)_f->vals[1]) , EXTRA );
    case A_IMPORT_FROM_0:
        return _PyAST_ImportFrom ( ((expr_ty)_f->vals[2]) -> v . Name . id , ((asdl_alias_seq*)_f->vals[4]) , _PyPegen_seq_count_dots ( _f->vals[1] ) , EXTRA );
    case A_IMPORT_FROM_1:
        return _PyAST_ImportFrom ( NULL , ((asdl_alias_seq*)_f->vals[3]) , _PyPegen_seq_count_dots ( _f->vals[1] ) , EXTRA );
    case A_IMPORT_FROM_TARGETS_0:
        return ((asdl_alias_seq*)_f->vals[1]);
    case A_IMPORT_FROM_TARGETS_1:
    case A_IMPORT_FROM_AS_NAMES_0:
    case A_DOTTED_AS_NAMES_0:
        return ((asdl_alias_seq*)_f->vals[0]);
    case A_IMPORT_FROM_TARGETS_2:
        return ( asdl_alias_seq * ) _PyPegen_singleton_seq ( p , CHECK ( alias_ty , _PyPegen_alias_for_star ( p , EXTRA ) ) );
    case A_IMPORT_FROM_AS_NAME_0:
    case A_DOTTED_AS_NAME_0:
        return _PyAST_alias ( ((expr_ty)_f->vals[0]) -> v . Name . id , ( _f->vals[1] )   ? ( ( expr_ty ) _f->vals[1] ) -> v . Name . id : NULL , EXTRA );
    case A_DOTTED_NAME_0:
        return _PyPegen_join_names_with_dot ( p , ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) );
    case A_BLOCK_0:
        return ((asdl_stmt_seq*)_f->vals[2]);
    case A_DECORATORS_0:
    case A_STAR_NAMED_EXPRESSIONS_0:
    case A_STAR_TARGETS_LIST_SEQ_0:
    case A_DEL_TARGETS_0:
    case A_TYPE_EXPRESSIONS_6:
        return ((asdl_expr_seq*)_f->vals[0]);
    case A_CLASS_DEF_0:
        return _PyPegen_class_def_decorators ( p , ((asdl_expr_seq*)_f->vals[0]) , ((stmt_ty)_f->vals[1]) );
    case A_CLASS_DEF_RAW_0:
        return _PyAST_ClassDef ( ((expr_ty)_f->vals[1]) -> v . Name . id , ( _f->vals[2] )   ? ( ( expr_ty ) _f->vals[2] ) -> v . Call . args : NULL , ( _f->vals[2] )   ? ( ( expr_ty ) _f->vals[2] ) -> v . Call . keywords : NULL , ((asdl_stmt_seq*)_f->vals[4]) , NULL , EXTRA );
    case A_FUNCTION_DEF_0:
        return _PyPegen_function_def_decorators ( p , ((asdl_expr_seq*)_f->vals[0]) , ((stmt_ty)_f->vals[1]) );
    case A_FUNCTION_DEF_RAW_0:
        return _PyAST_FunctionDef ( ((expr_ty)_f->vals[1]) -> v . Name . id , ( _f->vals[2] )   ? _f->vals[2] : CHECK ( arguments_ty , _PyPegen_empty_arguments ( p ) ) , ((asdl_stmt_seq*)_f->vals[6]) , NULL , _f->vals[4] , NEW_TYPE_COMMENT ( p , _f->vals[5] ) , EXTRA );
    case A_FUNCTION_DEF_RAW_1:
        return CHECK_VERSION ( stmt_ty , 5 , "Async functions are" , _PyAST_AsyncFunctionDef ( ((expr_ty)_f->vals[2]) -> v . Name . id , ( _f->vals[3] )   ? _f->vals[3] : CHECK ( arguments_ty , _PyPegen_empty_arguments ( p ) ) , ((asdl_stmt_seq*)_f->vals[7]) , NULL , _f->vals[5] , NEW_TYPE_COMMENT ( p , _f->vals[6] ) , EXTRA ) );
    case A_PARAMS_0:
    case A_LAMBDA_PARAMS_0:
        return ((arguments_ty)_f->vals[0]);
    case A_PARAMETERS_0:
    case A_LAMBDA_PARAMETERS_0:
        return _PyPegen_make_arguments ( p , ((asdl_arg_seq*)_f->vals[0]) , NULL , ((asdl_arg_seq*)_f->vals[1]) , _f->vals[2] , _f->vals[3] );
    case A_PARAMETERS_1:
    case A_LAMBDA_PARAMETERS_1:
        return _PyPegen_make_arguments ( p , NULL , ((SlashWithDefault*)_f->vals[0]) , NULL , _f->vals[1] , _f->vals[2] );
    case A_PARAMETERS_2:
    case A_LAMBDA_PARAMETERS_2:
        return _PyPegen_make_arguments ( p , NULL , NULL , ((asdl_arg_seq*)_f->vals[0]) , _f->vals[1] , _f->vals[2] );
    case A_PARAMETERS_3:
    case A_LAMBDA_PARAMETERS_3:
        return _PyPegen_make_arguments ( p , NULL , NULL , NULL , _f->vals[0] , _f->vals[1] );
    case A_PARAMETERS_4:
    case A_LAMBDA_PARAMETERS_4:
        return _PyPegen_make_arguments ( p , NULL , NULL , NULL , NULL , ((StarEtc*)_f->vals[0]) );
    case A_SLASH_NO_DEFAULT_0:
    case A_SLASH_NO_DEFAULT_1:
    case A_LAMBDA_SLASH_NO_DEFAULT_0:
    case A_LAMBDA_SLASH_NO_DEFAULT_1:
        return ((asdl_arg_seq*)_f->vals[0]);
    case A_SLASH_WITH_DEFAULT_0:
    case A_SLASH_WITH_DEFAULT_1:
    case A_LAMBDA_SLASH_WITH_DEFAULT_0:
    case A_LAMBDA_SLASH_WITH_DEFAULT_1:
        return _PyPegen_slash_with_default ( p , ( asdl_arg_seq * ) _f->vals[0] , _f->vals[1] );
    case A_STAR_ETC_0:
    case A_STAR_ETC_1:
    case A_LAMBDA_STAR_ETC_0:
        return _PyPegen_star_etc ( p , ((arg_ty)_f->vals[1]) , _f->vals[2] , _f->vals[3] );
    case A_STAR_ETC_2:
    case A_LAMBDA_STAR_ETC_1:
        return _PyPegen_star_etc ( p , NULL , _f->vals[2] , _f->vals[3] );
    case A_STAR_ETC_3:
    case A_LAMBDA_STAR_ETC_2:
        return _PyPegen_star_etc ( p , NULL , NULL , ((arg_ty)_f->vals[0]) );
    case A_KWDS_0:
    case A_LAMBDA_KWDS_0:
        return ((arg_ty)_f->vals[1]);
    case A_PARAM_NO_DEFAULT_0:
    case A_PARAM_NO_DEFAULT_STAR_ANNOTATION_0:
        return _PyPegen_add_type_comment_to_arg ( p , ((arg_ty)_f->vals[0]) , _f->vals[2] );
    case A_PARAM_NO_DEFAULT_1:
    case A_PARAM_NO_DEFAULT_STAR_ANNOTATION_1:
        return _PyPegen_add_type_comment_to_arg ( p , ((arg_ty)_f->vals[0]) , _f->vals[1] );
    case A_PARAM_WITH_DEFAULT_0:
        return _PyPegen_name_default_pair ( p , ((arg_ty)_f->vals[0]) , ((expr_ty)_f->vals[1]) , _f->vals[3] );
    case A_PARAM_WITH_DEFAULT_1:
        return _PyPegen_name_default_pair ( p , ((arg_ty)_f->vals[0]) , ((expr_ty)_f->vals[1]) , _f->vals[2] );
    case A_PARAM_MAYBE_DEFAULT_0:
        return _PyPegen_name_default_pair ( p , ((arg_ty)_f->vals[0]) , _f->vals[1] , _f->vals[3] );
    case A_PARAM_MAYBE_DEFAULT_1:
        return _PyPegen_name_default_pair ( p , ((arg_ty)_f->vals[0]) , _f->vals[1] , _f->vals[2] );
    case A_PARAM_0:
        return _PyAST_arg ( ((expr_ty)_f->vals[0]) -> v . Name . id , _f->vals[1] , NULL , EXTRA );
    case A_PARAM_STAR_ANNOTATION_0:
        return _PyAST_arg ( ((expr_ty)_f->vals[0]) -> v . Name . id , ((expr_ty)_f->vals[1]) , NULL , EXTRA );
    case A_ANNOTATION_0:
    case A_STAR_ANNOTATION_0:
    case A_DEFAULT_0:
    case A_GUARD_0:
    case A_DOUBLE_STAR_PATTERN_0:
    case A_SINGLE_TARGET_2:
    case A__TMP_11_0:
    case A__TMP_12_0:
    case A__TMP_13_0:
    case A__TMP_17_0:
    case A__TMP_23_0:
    case A__TMP_28_0:
    case A__TMP_31_0:
    case A__TMP_34_0:
    case A__TMP_35_0:
    case A__TMP_62_0:
    case A__TMP_63_0:
    case A__TMP_147_0:
    case A__TMP_148_0:
    case A__TMP_149_0:
    case A__TMP_150_0:
    case A__TMP_151_0:
    case A__TMP_153_0:
    case A__TMP_154_0:
    case A__TMP_156_0:
    case A__TMP_157_0:
        return ((expr_ty)_f->vals[1]);
    case A_IF_STMT_0:
    case A_ELIF_STMT_0:
        return _PyAST_If ( ((expr_ty)_f->vals[1]) , ((asdl_stmt_seq*)_f->vals[3]) , CHECK ( asdl_stmt_seq * , _PyPegen_singleton_seq ( p , ((stmt_ty)_f->vals[4]) ) ) , EXTRA );
    case A_IF_STMT_1:
    case A_ELIF_STMT_1:
        return _PyAST_If ( ((expr_ty)_f->vals[1]) , ((asdl_stmt_seq*)_f->vals[3]) , _f->vals[4] , EXTRA );
    case A_ELSE_BLOCK_0:
    case A_FINALLY_BLOCK_0:
        return ((asdl_stmt_seq*)_f->vals[1]);
    case A_WHILE_STMT_0:
        return _PyAST_While ( ((expr_ty)_f->vals[1]) , ((asdl_stmt_seq*)_f->vals[3]) , _f->vals[4] , EXTRA );
    case A_FOR_STMT_0:
        return _PyAST_For ( ((expr_ty)_f->vals[1]) , ((expr_ty)_f->vals[3]) , ((asdl_stmt_seq*)_f->vals[6]) , _f->vals[7] , NEW_TYPE_COMMENT ( p , _f->vals[5] ) , EXTRA );
    case A_FOR_STMT_1:
        return CHECK_VERSION ( stmt_ty , 5 , "Async for loops are" , _PyAST_AsyncFor ( ((expr_ty)_f->vals[2]) , ((expr_ty)_f->vals[4]) , ((asdl_stmt_seq*)_f->vals[7]) , _f->vals[8] , NEW_TYPE_COMMENT ( p , _f->vals[6] ) , EXTRA ) );
    case A_WITH_STMT_0:
        return CHECK_VERSION ( stmt_ty , 9 , "Parenthesized context managers are" , _PyAST_With ( ((asdl_withitem_seq*)_f->vals[2]) , ((asdl_stmt_seq*)_f->vals[6]) , NULL , EXTRA ) );
    case A_WITH_STMT_1:
        return _PyAST_With ( ((asdl_withitem_seq*)_f->vals[1]) , ((asdl_stmt_seq*)_f->vals[4]) , NEW_TYPE_COMMENT ( p , _f->vals[3] ) , EXTRA );
    case A_WITH_STMT_2:
        return CHECK_VERSION ( stmt_ty , 5 , "Async with statements are" , _PyAST_AsyncWith ( ((asdl_withitem_seq*)_f->vals[3]) , ((asdl_stmt_seq*)_f->vals[7]) , NULL , EXTRA ) );
    case A_WITH_STMT_3:
        return CHECK_VERSION ( stmt_ty , 5 , "Async with statements are" , _PyAST_AsyncWith ( ((asdl_withitem_seq*)_f->vals[2]) , ((asdl_stmt_seq*)_f->vals[5]) , NEW_TYPE_COMMENT ( p , _f->vals[4] ) , EXTRA ) );
    case A_WITH_ITEM_0:
        return _PyAST_withitem ( ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) , p -> arena );
    case A_WITH_ITEM_1:
        return _PyAST_withitem ( ((expr_ty)_f->vals[0]) , NULL , p -> arena );
    case A_TRY_STMT_0:
        return _PyAST_Try ( ((asdl_stmt_seq*)_f->vals[1]) , NULL , NULL , ((asdl_stmt_seq*)_f->vals[2]) , EXTRA );
    case A_TRY_STMT_1:
        return _PyAST_Try ( ((asdl_stmt_seq*)_f->vals[1]) , ((asdl_excepthandler_seq*)_f->vals[2]) , _f->vals[3] , _f->vals[4] , EXTRA );
    case A_TRY_STMT_2:
        return _PyAST_TryStar ( ((asdl_stmt_seq*)_f->vals[1]) , ((asdl_excepthandler_seq*)_f->vals[2]) , _f->vals[3] , _f->vals[4] , EXTRA );
    case A_EXCEPT_BLOCK_0:
        return _PyAST_ExceptHandler ( ((expr_ty)_f->vals[1]) , ( _f->vals[2] )   ? ( ( expr_ty ) _f->vals[2] ) -> v . Name . id : NULL , ((asdl_stmt_seq*)_f->vals[4]) , EXTRA );
    case A_EXCEPT_BLOCK_1:
        return _PyAST_ExceptHandler ( NULL , NULL , ((asdl_stmt_seq*)_f->vals[2]) , EXTRA );
    case A_EXCEPT_STAR_BLOCK_0:
        return _PyAST_ExceptHandler ( ((expr_ty)_f->vals[2]) , ( _f->vals[3] )   ? ( ( expr_ty ) _f->vals[3] ) -> v . Name . id : NULL , ((asdl_stmt_seq*)_f->vals[5]) , EXTRA );
    case A_MATCH_STMT_0:
        return CHECK_VERSION ( stmt_ty , 10 , "Pattern matching is" , _PyAST_Match ( ((expr_ty)_f->vals[1]) , ((asdl_match_case_seq*)_f->vals[5]) , EXTRA ) );
    case A_SUBJECT_EXPR_0:
        return _PyAST_Tuple ( CHECK ( asdl_expr_seq * , _PyPegen_seq_insert_in_front ( p , ((expr_ty)_f->vals[0]) , _f->vals[2] ) ) , Load , EXTRA );
    case A_CASE_BLOCK_0:
        return _PyAST_match_case ( ((pattern_ty)_f->vals[1]) , _f->vals[2] , ((asdl_stmt_seq*)_f->vals[4]) , p -> arena );
    case A_PATTERNS_0:
        return _PyAST_MatchSequence ( ((asdl_pattern_seq*)_f->vals[0]) , EXTRA );
    case A_PATTERNS_1:
    case A_PATTERN_0:
    case A_PATTERN_1:
    case A_CLOSED_PATTERN_0:
    case A_CLOSED_PATTERN_1:
    case A_CLOSED_PATTERN_2:
    case A_CLOSED_PATTERN_3:
    case A_CLOSED_PATTERN_4:
    case A_CLOSED_PATTERN_5:
    case A_CLOSED_PATTERN_6:
    case A_CLOSED_PATTERN_7:
    case A_MAYBE_STAR_PATTERN_0:
    case A_MAYBE_STAR_PATTERN_1:
    case A__GATHER_65_0:
    case A__GATHER_71_0:
    case A__GATHER_76_0:
        return ((pattern_ty)_f->vals[0]);
    case A_AS_PATTERN_0:
        return _PyAST_MatchAs ( ((pattern_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) -> v . Name . id , EXTRA );
    case A_OR_PATTERN_0:
        return asdl_seq_LEN ( ((asdl_pattern_seq*)_f->vals[0]) ) == 1   ? asdl_seq_GET ( ((asdl_pattern_seq*)_f->vals[0]) , 0 ) : _PyAST_MatchOr ( ((asdl_pattern_seq*)_f->vals[0]) , EXTRA );
    case A_LITERAL_PATTERN_0:
    case A_LITERAL_PATTERN_1:
    case A_LITERAL_PATTERN_2:
    case A_VALUE_PATTERN_0:
        return _PyAST_MatchValue ( ((expr_ty)_f->vals[0]) , EXTRA );
    case A_LITERAL_PATTERN_3:
        return _PyAST_MatchSingleton ( Py_None , EXTRA );
    case A_LITERAL_PATTERN_4:
        return _PyAST_MatchSingleton ( Py_True , EXTRA );
    case A_LITERAL_PATTERN_5:
        return _PyAST_MatchSingleton ( Py_False , EXTRA );
    case A_LITERAL_EXPR_3:
    case A_ATOM_3:
        return _PyAST_Constant ( Py_None , NULL , EXTRA );
    case A_LITERAL_EXPR_4:
    case A_ATOM_1:
        return _PyAST_Constant ( Py_True , NULL , EXTRA );
    case A_LITERAL_EXPR_5:
    case A_ATOM_2:
        return _PyAST_Constant ( Py_False , NULL , EXTRA );
    case A_COMPLEX_NUMBER_0:
    case A_SUM_0:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , Add , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_COMPLEX_NUMBER_1:
    case A_SUM_1:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , Sub , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_SIGNED_NUMBER_1:
        return _PyAST_UnaryOp ( USub , _f->vals[1] , EXTRA );
    case A_SIGNED_REAL_NUMBER_1:
    case A_FACTOR_1:
        return _PyAST_UnaryOp ( USub , ((expr_ty)_f->vals[1]) , EXTRA );
    case A_REAL_NUMBER_0:
        return _PyPegen_ensure_real ( p , _f->vals[0] );
    case A_IMAGINARY_NUMBER_0:
        return _PyPegen_ensure_imaginary ( p , _f->vals[0] );
    case A_CAPTURE_PATTERN_0:
        return _PyAST_MatchAs ( NULL , ((expr_ty)_f->vals[0]) -> v . Name . id , EXTRA );
    case A_PATTERN_CAPTURE_TARGET_0:
    case A_STAR_ATOM_0:
    case A_SINGLE_TARGET_1:
        return _PyPegen_set_expr_context ( p , ((expr_ty)_f->vals[0]) , Store );
    case A_WILDCARD_PATTERN_0:
        return _PyAST_MatchAs ( NULL , NULL , EXTRA );
    case A_ATTR_0:
    case A_PRIMARY_0:
    case A_T_PRIMARY_0:
        return _PyAST_Attribute ( ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) -> v . Name . id , Load , EXTRA );
    case A_GROUP_PATTERN_0:
        return ((pattern_ty)_f->vals[1]);
    case A_SEQUENCE_PATTERN_0:
    case A_SEQUENCE_PATTERN_1:
        return _PyAST_MatchSequence ( _f->vals[1] , EXTRA );
    case A_OPEN_SEQUENCE_PATTERN_0:
        return _PyPegen_seq_insert_in_front ( p , ((pattern_ty)_f->vals[0]) , _f->vals[2] );
    case A_STAR_PATTERN_0:
        return _PyAST_MatchStar ( ((expr_ty)_f->vals[1]) -> v . Name . id , EXTRA );
    case A_STAR_PATTERN_1:
        return _PyAST_MatchStar ( NULL , EXTRA );
    case A_MAPPING_PATTERN_0:
        return _PyAST_MatchMapping ( NULL , NULL , NULL , EXTRA );
    case A_MAPPING_PATTERN_1:
        return _PyAST_MatchMapping ( NULL , NULL , ((expr_ty)_f->vals[1]) -> v . Name . id , EXTRA );
    case A_MAPPING_PATTERN_2:
        return _PyAST_MatchMapping ( CHECK ( asdl_expr_seq * , _PyPegen_get_pattern_keys ( p , ((asdl_seq*)_f->vals[1]) ) ) , CHECK ( asdl_pattern_seq * , _PyPegen_get_patterns ( p , ((asdl_seq*)_f->vals[1]) ) ) , ((expr_ty)_f->vals[3]) -> v . Name . id , EXTRA );
    case A_MAPPING_PATTERN_3:
        return _PyAST_MatchMapping ( CHECK ( asdl_expr_seq * , _PyPegen_get_pattern_keys ( p , ((asdl_seq*)_f->vals[1]) ) ) , CHECK ( asdl_pattern_seq * , _PyPegen_get_patterns ( p , ((asdl_seq*)_f->vals[1]) ) ) , NULL , EXTRA );
    case A_KEY_VALUE_PATTERN_0:
        return _PyPegen_key_pattern_pair ( p , _f->vals[0] , ((pattern_ty)_f->vals[2]) );
    case A_CLASS_PATTERN_0:
        return _PyAST_MatchClass ( ((expr_ty)_f->vals[0]) , NULL , NULL , NULL , EXTRA );
    case A_CLASS_PATTERN_1:
        return _PyAST_MatchClass ( ((expr_ty)_f->vals[0]) , ((asdl_pattern_seq*)_f->vals[2]) , NULL , NULL , EXTRA );
    case A_CLASS_PATTERN_2:
        return _PyAST_MatchClass ( ((expr_ty)_f->vals[0]) , NULL , CHECK ( asdl_identifier_seq * , _PyPegen_map_names_to_ids ( p , CHECK ( asdl_expr_seq * , _PyPegen_get_pattern_keys ( p , ((asdl_seq*)_f->vals[2]) ) ) ) ) , CHECK ( asdl_pattern_seq * , _PyPegen_get_patterns ( p , ((asdl_seq*)_f->vals[2]) ) ) , EXTRA );
    case A_CLASS_PATTERN_3:
        return _PyAST_MatchClass ( ((expr_ty)_f->vals[0]) , ((asdl_pattern_seq*)_f->vals[2]) , CHECK ( asdl_identifier_seq * , _PyPegen_map_names_to_ids ( p , CHECK ( asdl_expr_seq * , _PyPegen_get_pattern_keys ( p , ((asdl_seq*)_f->vals[4]) ) ) ) ) , CHECK ( asdl_pattern_seq * , _PyPegen_get_patterns ( p , ((asdl_seq*)_f->vals[4]) ) ) , EXTRA );
    case A_POSITIONAL_PATTERNS_0:
        return ((asdl_pattern_seq*)_f->vals[0]);
    case A_KEYWORD_PATTERN_0:
        return _PyPegen_key_pattern_pair ( p , ((expr_ty)_f->vals[0]) , ((pattern_ty)_f->vals[2]) );
    case A_EXPRESSIONS_0:
    case A_STAR_EXPRESSIONS_0:
        return _PyAST_Tuple ( CHECK ( asdl_expr_seq * , _PyPegen_seq_insert_in_front ( p , ((expr_ty)_f->vals[0]) , _f->vals[1] ) ) , Load , EXTRA );
    case A_EXPRESSIONS_1:
    case A_STAR_EXPRESSIONS_1:
        return _PyAST_Tuple ( CHECK ( asdl_expr_seq * , _PyPegen_singleton_seq ( p , ((expr_ty)_f->vals[0]) ) ) , Load , EXTRA );
    case A_EXPRESSION_0:
        return _PyAST_IfExp ( ((expr_ty)_f->vals[2]) , ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[4]) , EXTRA );
    case A_YIELD_EXPR_0:
        return _PyAST_YieldFrom ( ((expr_ty)_f->vals[2]) , EXTRA );
    case A_YIELD_EXPR_1:
        return _PyAST_Yield ( _f->vals[1] , EXTRA );
    case A_STAR_EXPRESSION_0:
    case A_STAR_NAMED_EXPRESSION_0:
    case A_STARRED_EXPRESSION_0:
        return _PyAST_Starred ( ((expr_ty)_f->vals[1]) , Load , EXTRA );
    case A_ASSIGNMENT_EXPRESSION_0:
        return CHECK_VERSION ( expr_ty , 8 , "Assignment expressions are" , _PyAST_NamedExpr ( CHECK ( expr_ty , _PyPegen_set_expr_context ( p , ((expr_ty)_f->vals[0]) , Store ) ) , ((expr_ty)_f->vals[2]) , EXTRA ) );
    case A_DISJUNCTION_0:
        return _PyAST_BoolOp ( Or , CHECK ( asdl_expr_seq * , _PyPegen_seq_insert_in_front ( p , ((expr_ty)_f->vals[0]) , _f->vals[1] ) ) , EXTRA );
    case A_CONJUNCTION_0:
        return _PyAST_BoolOp ( And , CHECK ( asdl_expr_seq * , _PyPegen_seq_insert_in_front ( p , ((expr_ty)_f->vals[0]) , _f->vals[1] ) ) , EXTRA );
    case A_INVERSION_0:
        return _PyAST_UnaryOp ( Not , ((expr_ty)_f->vals[1]) , EXTRA );
    case A_COMPARISON_0:
        return _PyAST_Compare ( ((expr_ty)_f->vals[0]) , CHECK ( asdl_int_seq * , _PyPegen_get_cmpops ( p , _f->vals[1] ) ) , CHECK ( asdl_expr_seq * , _PyPegen_get_exprs ( p , _f->vals[1] ) ) , EXTRA );
    case A_COMPARE_OP_BITWISE_OR_PAIR_0:
    case A_COMPARE_OP_BITWISE_OR_PAIR_1:
    case A_COMPARE_OP_BITWISE_OR_PAIR_2:
    case A_COMPARE_OP_BITWISE_OR_PAIR_3:
    case A_COMPARE_OP_BITWISE_OR_PAIR_4:
    case A_COMPARE_OP_BITWISE_OR_PAIR_5:
    case A_COMPARE_OP_BITWISE_OR_PAIR_6:
    case A_COMPARE_OP_BITWISE_OR_PAIR_7:
    case A_COMPARE_OP_BITWISE_OR_PAIR_8:
    case A_COMPARE_OP_BITWISE_OR_PAIR_9:
        return ((CmpopExprPair*)_f->vals[0]);
    case A_EQ_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , Eq , ((expr_ty)_f->vals[1]) );
    case A_NOTEQ_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , NotEq , ((expr_ty)_f->vals[1]) );
    case A_LTE_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , LtE , ((expr_ty)_f->vals[1]) );
    case A_LT_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , Lt , ((expr_ty)_f->vals[1]) );
    case A_GTE_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , GtE , ((expr_ty)_f->vals[1]) );
    case A_GT_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , Gt , ((expr_ty)_f->vals[1]) );
    case A_NOTIN_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , NotIn , ((expr_ty)_f->vals[2]) );
    case A_IN_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , In , ((expr_ty)_f->vals[1]) );
    case A_ISNOT_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , IsNot , ((expr_ty)_f->vals[2]) );
    case A_IS_BITWISE_OR_0:
        return _PyPegen_cmpop_expr_pair ( p , Is , ((expr_ty)_f->vals[1]) );
    case A_BITWISE_OR_0:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , BitOr , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_BITWISE_XOR_0:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , BitXor , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_BITWISE_AND_0:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , BitAnd , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_SHIFT_EXPR_0:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , LShift , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_SHIFT_EXPR_1:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , RShift , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_TERM_0:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , Mult , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_TERM_1:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , Div , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_TERM_2:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , FloorDiv , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_TERM_3:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , Mod , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_TERM_4:
        return CHECK_VERSION ( expr_ty , 5 , "The '@' operator is" , _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , MatMult , ((expr_ty)_f->vals[2]) , EXTRA ) );
    case A_FACTOR_0:
        return _PyAST_UnaryOp ( UAdd , ((expr_ty)_f->vals[1]) , EXTRA );
    case A_FACTOR_2:
        return _PyAST_UnaryOp ( Invert , ((expr_ty)_f->vals[1]) , EXTRA );
    case A_POWER_0:
        return _PyAST_BinOp ( ((expr_ty)_f->vals[0]) , Pow , ((expr_ty)_f->vals[2]) , EXTRA );
    case A_AWAIT_PRIMARY_0:
        return CHECK_VERSION ( expr_ty , 5 , "Await expressions are" , _PyAST_Await ( ((expr_ty)_f->vals[1]) , EXTRA ) );
    case A_PRIMARY_1:
    case A_T_PRIMARY_2:
        return _PyAST_Call ( ((expr_ty)_f->vals[0]) , CHECK ( asdl_expr_seq * , ( asdl_expr_seq * ) _PyPegen_singleton_seq ( p , ((expr_ty)_f->vals[1]) ) ) , NULL , EXTRA );
    case A_PRIMARY_2:
    case A_T_PRIMARY_3:
        return _PyAST_Call ( ((expr_ty)_f->vals[0]) , ( _f->vals[2] )   ? ( ( expr_ty ) _f->vals[2] ) -> v . Call . args : NULL , ( _f->vals[2] )   ? ( ( expr_ty ) _f->vals[2] ) -> v . Call . keywords : NULL , EXTRA );
    case A_PRIMARY_3:
    case A_T_PRIMARY_1:
        return _PyAST_Subscript ( ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) , Load , EXTRA );
    case A_SLICES_1:
        return _PyAST_Tuple ( ((asdl_expr_seq*)_f->vals[0]) , Load , EXTRA );
    case A_SLICE_0:
        return _PyAST_Slice ( _f->vals[0] , _f->vals[2] , _f->vals[3] , EXTRA );
    case A_ATOM_9:
        return _PyAST_Constant ( Py_Ellipsis , NULL , EXTRA );
    case A_GROUP_0:
    case A_FUNC_TYPE_COMMENT_0:
    case A__TMP_33_0:
    case A__TMP_90_0:
        return _f->vals[1];
    case A_LAMBDEF_0:
        return _PyAST_Lambda ( ( _f->vals[1] )   ? _f->vals[1] : CHECK ( arguments_ty , _PyPegen_empty_arguments ( p ) ) , ((expr_ty)_f->vals[3]) , EXTRA );
    case A_LAMBDA_PARAM_NO_DEFAULT_0:
    case A_LAMBDA_PARAM_NO_DEFAULT_1:
        return ((arg_ty)_f->vals[0]);
    case A_LAMBDA_PARAM_WITH_DEFAULT_0:
    case A_LAMBDA_PARAM_WITH_DEFAULT_1:
        return _PyPegen_name_default_pair ( p , ((arg_ty)_f->vals[0]) , ((expr_ty)_f->vals[1]) , NULL );
    case A_LAMBDA_PARAM_MAYBE_DEFAULT_0:
    case A_LAMBDA_PARAM_MAYBE_DEFAULT_1:
        return _PyPegen_name_default_pair ( p , ((arg_ty)_f->vals[0]) , _f->vals[1] , NULL );
    case A_LAMBDA_PARAM_0:
        return _PyAST_arg ( ((expr_ty)_f->vals[0]) -> v . Name . id , NULL , NULL , EXTRA );
    case A_STRINGS_0:
        return _PyPegen_concatenate_strings ( p , _f->vals[0] );
    case A_LIST_0:
        return _PyAST_List ( _f->vals[1] , Load , EXTRA );
    case A_TUPLE_0:
        return _PyAST_Tuple ( _f->vals[1] , Load , EXTRA );
    case A_SET_0:
        return _PyAST_Set ( ((asdl_expr_seq*)_f->vals[1]) , EXTRA );
    case A_DICT_0:
        return _PyAST_Dict ( CHECK ( asdl_expr_seq * , _PyPegen_get_keys ( p , _f->vals[1] ) ) , CHECK ( asdl_expr_seq * , _PyPegen_get_values ( p , _f->vals[1] ) ) , EXTRA );
    case A_DOUBLE_STARRED_KVPAIR_0:
        return _PyPegen_key_value_pair ( p , NULL , ((expr_ty)_f->vals[1]) );
    case A_DOUBLE_STARRED_KVPAIR_1:
    case A__GATHER_111_0:
        return ((KeyValuePair*)_f->vals[0]);
    case A_KVPAIR_0:
        return _PyPegen_key_value_pair ( p , ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) );
    case A_FOR_IF_CLAUSES_0:
        return ((asdl_comprehension_seq*)_f->vals[0]);
    case A_FOR_IF_CLAUSE_0:
        return CHECK_VERSION ( comprehension_ty , 6 , "Async comprehensions are" , _PyAST_comprehension ( ((expr_ty)_f->vals[2]) , ((expr_ty)_f->vals[4]) , ((asdl_expr_seq*)_f->vals[5]) , 1 , p -> arena ) );
    case A_FOR_IF_CLAUSE_1:
        return _PyAST_comprehension ( ((expr_ty)_f->vals[1]) , ((expr_ty)_f->vals[3]) , ((asdl_expr_seq*)_f->vals[4]) , 0 , p -> arena );
    case A_LISTCOMP_0:
        return _PyAST_ListComp ( ((expr_ty)_f->vals[1]) , ((asdl_comprehension_seq*)_f->vals[2]) , EXTRA );
    case A_SETCOMP_0:
        return _PyAST_SetComp ( ((expr_ty)_f->vals[1]) , ((asdl_comprehension_seq*)_f->vals[2]) , EXTRA );
    case A_GENEXP_0:
        return _PyAST_GeneratorExp ( _f->vals[1] , ((asdl_comprehension_seq*)_f->vals[2]) , EXTRA );
    case A_DICTCOMP_0:
        return _PyAST_DictComp ( ((KeyValuePair*)_f->vals[1]) -> key , ((KeyValuePair*)_f->vals[1]) -> value , ((asdl_comprehension_seq*)_f->vals[2]) , EXTRA );
    case A_ARGS_0:
        return _PyPegen_collect_call_seqs ( p , ((asdl_expr_seq*)_f->vals[0]) , _f->vals[1] , EXTRA );
    case A_ARGS_1:
        return _PyAST_Call ( _PyPegen_dummy_name ( p ) , CHECK_NULL_ALLOWED ( asdl_expr_seq * , _PyPegen_seq_extract_starred_exprs ( p , ((asdl_seq*)_f->vals[0]) ) ) , CHECK_NULL_ALLOWED ( asdl_keyword_seq * , _PyPegen_seq_delete_starred_exprs ( p , ((asdl_seq*)_f->vals[0]) ) ) , EXTRA );
    case A_KWARGS_0:
        return _PyPegen_join_sequences ( p , _f->vals[0] , _f->vals[2] );
    case A_KWARG_OR_STARRED_0:
    case A_KWARG_OR_DOUBLE_STARRED_0:
        return _PyPegen_keyword_or_starred ( p , CHECK ( keyword_ty , _PyAST_keyword ( ((expr_ty)_f->vals[0]) -> v . Name . id , ((expr_ty)_f->vals[2]) , EXTRA ) ) , 1 );
    case A_KWARG_OR_STARRED_1:
        return _PyPegen_keyword_or_starred ( p , ((expr_ty)_f->vals[0]) , 0 );
    case A_KWARG_OR_DOUBLE_STARRED_1:
        return _PyPegen_keyword_or_starred ( p , CHECK ( keyword_ty , _PyAST_keyword ( NULL , ((expr_ty)_f->vals[1]) , EXTRA ) ) , 1 );
    case A_STAR_TARGETS_1:
        return _PyAST_Tuple ( CHECK ( asdl_expr_seq * , _PyPegen_seq_insert_in_front ( p , ((expr_ty)_f->vals[0]) , _f->vals[1] ) ) , Store , EXTRA );
    case A_STAR_TARGETS_TUPLE_SEQ_0:
        return ( asdl_expr_seq * ) _PyPegen_seq_insert_in_front ( p , ((expr_ty)_f->vals[0]) , _f->vals[1] );
    case A_STAR_TARGETS_TUPLE_SEQ_1:
        return ( asdl_expr_seq * ) _PyPegen_singleton_seq ( p , ((expr_ty)_f->vals[0]) );
    case A_STAR_TARGET_0:
        return _PyAST_Starred ( CHECK ( expr_ty , _PyPegen_set_expr_context ( p , _f->vals[1] , Store ) ) , Store , EXTRA );
    case A_TARGET_WITH_STAR_ATOM_0:
    case A_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET_0:
        return _PyAST_Attribute ( ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) -> v . Name . id , Store , EXTRA );
    case A_TARGET_WITH_STAR_ATOM_1:
    case A_SINGLE_SUBSCRIPT_ATTRIBUTE_TARGET_1:
        return _PyAST_Subscript ( ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) , Store , EXTRA );
    case A_STAR_ATOM_1:
        return _PyPegen_set_expr_context ( p , ((expr_ty)_f->vals[1]) , Store );
    case A_STAR_ATOM_2:
        return _PyAST_Tuple ( _f->vals[1] , Store , EXTRA );
    case A_STAR_ATOM_3:
        return _PyAST_List ( _f->vals[1] , Store , EXTRA );
    case A_T_LOOKAHEAD_0:
    case A_T_LOOKAHEAD_1:
    case A_T_LOOKAHEAD_2:
    case A__TMP_6_0:
    case A__TMP_6_1:
    case A__TMP_7_0:
    case A__TMP_7_1:
    case A__TMP_8_0:
    case A__TMP_8_1:
    case A__TMP_9_0:
    case A__TMP_10_0:
    case A__TMP_22_0:
    case A__TMP_59_0:
    case A__TMP_59_1:
    case A__TMP_59_2:
    case A__TMP_67_0:
    case A__TMP_67_1:
    case A__TMP_68_0:
    case A__TMP_68_1:
    case A__TMP_69_0:
    case A__TMP_69_1:
    case A__TMP_69_2:
    case A__TMP_70_0:
    case A__TMP_70_1:
    case A__TMP_70_2:
    case A__TMP_145_0:
    case A__TMP_145_1:
    case A__TMP_146_0:
    case A__TMP_146_1:
        return ((Token *)_f->vals[0]);
    case A_DEL_TARGET_0:
        return _PyAST_Attribute ( ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) -> v . Name . id , Del , EXTRA );
    case A_DEL_TARGET_1:
        return _PyAST_Subscript ( ((expr_ty)_f->vals[0]) , ((expr_ty)_f->vals[2]) , Del , EXTRA );
    case A_DEL_T_ATOM_0:
        return _PyPegen_set_expr_context ( p , ((expr_ty)_f->vals[0]) , Del );
    case A_DEL_T_ATOM_1:
        return _PyPegen_set_expr_context ( p , ((expr_ty)_f->vals[1]) , Del );
    case A_DEL_T_ATOM_2:
        return _PyAST_Tuple ( _f->vals[1] , Del , EXTRA );
    case A_DEL_T_ATOM_3:
        return _PyAST_List ( _f->vals[1] , Del , EXTRA );
    case A_TYPE_EXPRESSIONS_0:
        return ( asdl_expr_seq * ) _PyPegen_seq_append_to_end ( p , CHECK ( asdl_seq * , _PyPegen_seq_append_to_end ( p , _f->vals[0] , ((expr_ty)_f->vals[3]) ) ) , ((expr_ty)_f->vals[6]) );
    case A_TYPE_EXPRESSIONS_1:
    case A_TYPE_EXPRESSIONS_2:
        return ( asdl_expr_seq * ) _PyPegen_seq_append_to_end ( p , _f->vals[0] , ((expr_ty)_f->vals[3]) );
    case A_TYPE_EXPRESSIONS_3:
        return ( asdl_expr_seq * ) _PyPegen_seq_append_to_end ( p , CHECK ( asdl_seq * , _PyPegen_singleton_seq ( p , ((expr_ty)_f->vals[1]) ) ) , ((expr_ty)_f->vals[4]) );
    case A_TYPE_EXPRESSIONS_4:
    case A_TYPE_EXPRESSIONS_5:
        return ( asdl_expr_seq * ) _PyPegen_singleton_seq ( p , ((expr_ty)_f->vals[1]) );
    case A__GATHER_26_0:
    case A__GATHER_29_0:
        return ((alias_ty)_f->vals[0]);
    case A__GATHER_51_0:
    case A__GATHER_53_0:
    case A__GATHER_55_0:
    case A__GATHER_57_0:
        return ((withitem_ty)_f->vals[0]);
    case A__GATHER_73_0:
    case A__GATHER_78_0:
        return ((KeyPatternPair*)_f->vals[0]);
    case A__TMP_87_0:
        return _PyPegen_check_barry_as_flufl ( p , ((Token *)_f->vals[0]) )   ? NULL : ((Token *)_f->vals[0]);
    case A__TMP_110_0:
        return _PyPegen_seq_insert_in_front ( p , ((expr_ty)_f->vals[0]) , _f->vals[2] );
    case A__TMP_119_0:
        return ((asdl_seq*)_f->vals[1]);
    case A__GATHER_120_0:
    case A__GATHER_122_0:
    case A__GATHER_124_0:
    case A__GATHER_126_0:
        return ((KeywordOrStarred*)_f->vals[0]);
    default:
        assert(0);
        RAISE_SYNTAX_ERROR("invalid opcode");
        return NULL;
    }
}
