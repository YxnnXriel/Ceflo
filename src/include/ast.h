#ifndef __AST_H__
#define __AST_H__

    #include "common.h"
    #include "str.h"
    #include "list.h"
    #include "token.h"

    #define FOREACH_AST(AST)    \
        AST(AST_VARIABLE_DEFINITION)   \
        AST(AST_VARIABLE)  \
        AST(AST_FUNCTION_DEFINITION)   \
        AST(AST_FUNCTION_CALL) \
        AST(AST_NONE)  \
        AST(AST_BOOL)  \
        AST(AST_CHAR)  \
        AST(AST_NUMBER)    \
        AST(AST_STRING)    \
        AST(AST_OBJ)   \
        AST(AST_LIST)   \
        AST(AST_COMPOUND)  \
        AST(AST_BINOP)  \
        AST(AST_RETURN)  \
        AST(AST_UNOP)  \
        AST(AST_IF)  \
        AST(AST_WHILE)  \
        AST(AST_BREAK)  \
        AST(AST_ASSIGNEMENT)  \
        AST(AST_NOOP)  

    #define GENERATE_AST_ENUM(AST)  AST,
    #define GENERATE_AST_ENUM_STRING(AST)  #AST,



typedef enum{
    R_FUNCTION_CALL_TYPE,
    R_IF_TYPE,
    R_WHILE_TYPE,
}Return_Type;


typedef enum{
    NONE,
    BOOL,
    CHAR,
    NUMBER,
    STRING,
    OBJECT,
}type_ceflo;

struct AST_STRUCT;
struct SCOPE_STRUCT;

#define LIST_AST_COUNT 255
typedef struct LIST_AST_STRUCT{
    struct AST_STRUCT *list[LIST_AST_COUNT];
    size_t size;
}list_ast_t;

typedef struct OBJECT_STRUCT {
    list_ast_t *constructor_list;
    struct SCOPE_STRUCT *self;
    struct OBJECT_STRUCT *next;
}Obj_t;


typedef struct AST_TYPE_VALUE{
    type_ceflo type;
    union {
        bool booleanValue;
        double numberValue;
        char charValue;
        string_t *stringValue;
        Obj_t *ObjectValue; // la j'alloue un object
    }as;
}Value_t;

#define AS_BOOL(value) ((value).as.booleanValue)
#define AS_NUMBER(value) ((value).as.numberValue)
#define AS_CHAR(value) ((value).as.charValue)
#define AS_STRING(value) ((value).as.stringValue->str)



typedef struct AST_STRUCT{

    enum{
        FOREACH_AST(GENERATE_AST_ENUM)
    }type;

    // AST_VARIABLE_DEFINITION
    char * variable_definition_name;
    struct AST_STRUCT *variable_definition_value;
    type_ceflo variable_definition_static_type;


    // AST_VARIABLE
    char *variable_name;


    // AST_FUNCTION_DEFINITION
    char *function_definition_name;
    list_ast_t function_defintion_args;
    struct AST_STRUCT* function_definition_body;
    type_ceflo function_definition_return_type;


    // AST_FUNCTION_CALL
    char *function_call_name;
    list_ast_t function_call_args;

    // AST_VALUE
    Value_t value;

    struct AST_STRUCT *next;

    // AST_COMPOUND
    list_t *list_compound;
    Return_Type type_compound;
    // scope
    struct SCOPE_STRUCT *scope;
    int countScope;

    

    // binop
    struct AST_STRUCT *left;
    struct AST_STRUCT *right;
    token_t op;

    // return
    struct AST_STRUCT *return_expr;
    bool is_return_value;

    // if
    struct AST_STRUCT *if_expr;
    struct AST_STRUCT *if_body;
    struct AST_STRUCT *if_other;
    struct AST_STRUCT *else_body;

    // while
    struct AST_STRUCT *while_expr;
    struct AST_STRUCT *while_body;


    // assignement
    struct AST_STRUCT *assignement_left;
    struct AST_STRUCT *assignement_value;
    const char *assignement_variable_name;




}ast_t;


static const char *AST_TO_STRING[] ={
    FOREACH_AST(GENERATE_AST_ENUM_STRING)
};

void __init_first_ast();
ast_t *newAst(uint8_t type);

void newList_Ast(list_ast_t *list);
void newObject(Obj_t * object);
void newValue(Value_t * value , uint8_t type);

int List_Ast_Append(list_ast_t *list , ast_t *ast);
ast_t * List_Ast_Get(list_ast_t *list , int index);
ast_t * List_Ast_Get(list_ast_t *list , int index);

ast_t * GetFirstAst();
void ModifFirstAst(ast_t * new);
void ResetFirstAst();
void clearAstNode(ast_t * val);
void freeAst(ast_t *node);
void clearAst();
void freeSingleAst(ast_t * node);

ast_t * ast_copy(ast_t *ast,struct SCOPE_STRUCT * scope);
ast_t * ast_cpy_variable_definition_lit(char *name , ast_t * value);
ast_t * ast_copy(ast_t *ast,struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_Value(ast_t * ast);
ast_t * ast_cpy_variable(ast_t * ast);
ast_t *ast_cpy_compound(ast_t *ast , struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_function_call(ast_t *ast ,struct SCOPE_STRUCT * scope );
ast_t * ast_cpy_function_definition(ast_t *function , struct SCOPE_STRUCT * scope);
ast_t * ast_cpy_variable_definition(ast_t *ast,struct SCOPE_STRUCT *scope);
ast_t *ast_cpy_binop(ast_t *ast , struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_return(ast_t *ast , struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_if(ast_t *ast , struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_while(ast_t *ast , struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_break(ast_t *ast , struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_variable_assignement(ast_t *ast , struct SCOPE_STRUCT * scope);







#endif