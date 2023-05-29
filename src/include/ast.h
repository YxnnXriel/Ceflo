/**
 * @file ast.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */


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
        AST(AST_LIST)   \
        AST(AST_OBJ)   \
        AST(AST_LFUNC)   \
        AST(AST_OBJ_ACCESS)   \
        AST(AST_LIST_ACCESS)   \
        AST(AST_LIST_ASSIGNEMENT)   \
        AST(AST_COMPOUND)  \
        AST(AST_BINOP)  \
        AST(AST_RETURN)  \
        AST(AST_UNOP)  \
        AST(AST_IF)  \
        AST(AST_WHILE)  \
        AST(AST_BREAK)  \
        AST(AST_ASSIGNEMENT)  \
        AST(AST_STRUCT_DEFINITION)  \
        AST(AST_STRUCT)  \
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
    LIST,
    OBJECT,
    LFUNC
}type_ceflo;

struct AST_STRUCT;
struct SCOPE_STRUCT;
struct VISITOR_STRUCT;


#define LIST_AST_COUNT 255
typedef struct LIST_AST_STRUCT{
    struct AST_STRUCT *list[LIST_AST_COUNT];
    size_t size;
}list_ast_t;

typedef struct OBJECT_STRUCT {
    char *obj_name;
    struct SCOPE_STRUCT *self;
}Obj_t;


typedef struct AST_TYPE_VALUE{
    type_ceflo type;
    union {
        bool booleanValue;
        char charValue;
        double numberValue;
        string_t *stringValue;
        list_t *listValue;
        Obj_t ObjectValue;
        struct AST_STRUCT *lfuncValue;
    }as;
}Value_t;


    #define AS_BOOL(value) ((value).as.booleanValue)
    #define AS_NUMBER(value) ((value).as.numberValue)
    #define AS_CHAR(value) ((value).as.charValue)
    #define AS_STRING(value) ((value).as.stringValue->str)
    #define AS_LIST(value) ((value).as.listValue)
    #define AS_OBJ(Value) ((value).as.ObjectValue)


    #define OBJECT_SELF(ast) ((ast->value.as.ObjectValue.self))


typedef struct AST_STRUCT{

    enum{
        FOREACH_AST(GENERATE_AST_ENUM)
    }type;



    /* ======  AST_VARIABLE_DEFINITION ====== */
        char * variable_definition_name;
        struct AST_STRUCT *variable_definition_value;
        type_ceflo variable_definition_static_type;
        char *variable_definition_static_type_name;


    /* ======  AST_VARIABLE ====== */
        char *variable_name;
        bool variable_forGlobal;








    /* ======  AST_FUNCTION_DEFINITION ====== */
        char *function_definition_name;
        list_ast_t function_defintion_args;
        struct AST_STRUCT* function_definition_body;
        type_ceflo function_definition_return_type;
        char *function_definition_return_type_name;



    /* ======  AST_FUNCTION_CALL ====== */
        char *function_call_name;
        list_ast_t function_call_args;






    /* ======  AST_VALUE ====== */
        Value_t value;
        int marked_count;
        bool is_for_variable;








    /* ======  scope ====== */
        struct SCOPE_STRUCT *scope;
        int countScope;

    

    /* ======  binop ====== */
        struct AST_STRUCT *left;
        struct AST_STRUCT *right;
        token_t op;



    /* ======  return ====== */
        struct AST_STRUCT *return_expr;
        bool is_return_value;




    /* ======  if ====== */
        struct AST_STRUCT *if_expr;
        struct AST_STRUCT *if_body;
        struct AST_STRUCT *if_other;
        struct AST_STRUCT *else_body;



    /* ======  while ====== */
        struct AST_STRUCT *while_expr;
        struct AST_STRUCT *while_body;


    /* ======  assignement ====== */
        struct AST_STRUCT *assignement_left;
        struct AST_STRUCT *assignement_value;
        const char *assignement_variable_name;





    /* ====== List_ACCES ====== */
        struct AST_STRUCT *list_ptr;




    // ======  STRUCT DEFINITION ======
        char *struct_definition_name;
        list_t *struct_definition_item; // ======  function , variable ect ... ======




    /* ======  OBJ ====== */
        bool is_new_obj;
        // vas de pair avec value. Si il est false c'est que value de type AST_OBJ doit etre copier





    /* ======  STRING ====== */
        bool is_string_modif;// utiliser pour modifier un caractere dans une chaine
        struct AST_STRUCT *ref_string; // pointeur sur la chaine en question






    /* ======  LIST ====== */
        list_t *ref_list;
        int ref_list_index;



    /* ======  AST_COMPOUND ====== */
        list_t *list_compound;
        Return_Type type_compound;


    /* ======  AST_COMPOUND ====== */
        bool is_func_typer;


    /* ======== NEXT AST ======== */
        struct AST_STRUCT *next;
        struct AST_STRUCT *prev;

        int line;
        int colone;
        // utiliser pour connaitre le prochaine object AST
        // utiliser pour la suppression de l'arbre en atente d'un GB

}ast_t;


static const char *AST_TO_STRING[] ={
    FOREACH_AST(GENERATE_AST_ENUM_STRING)
};

// NEW AST
ast_t *newAst(uint8_t type);
ast_t *newAstWithLine(uint8_t type, int line , int colone);



// VALUE
void newValue(Value_t * value , uint8_t type);
void modifValue(ast_t ** ref  , ast_t * new);
void newObject(Obj_t * object);
bool ObjectIsSame(Obj_t *self , const char *name);
void printValueAst(ast_t * value,struct VISITOR_STRUCT * visitor);
bool printObj(ast_t * value);
bool printlist(ast_t * value , struct VISITOR_STRUCT * visitor);
bool printlistByeValue(Value_t * value , struct VISITOR_STRUCT * visitor);
bool printlistByeList(list_t * list , struct VISITOR_STRUCT * visitor);

void Value_incref(ast_t * value);
bool Value_decref(ast_t * value);

void newList_Ast(list_ast_t *list);
int List_Ast_Append(list_ast_t *list , ast_t *ast);
ast_t * List_Ast_Get(list_ast_t *list , int index);
ast_t * List_Ast_Get(list_ast_t *list , int index);
void List_Ast_Sup(list_ast_t *list , int index);
void List_ast_Shift(list_ast_t *list , int index);
void List_Ast_Shift_All(list_ast_t * list);
list_ast_t List_Ast_Cpy(list_ast_t *list);
list_ast_t List_Ast_Cpy_Without_Index(list_ast_t * list , int index);
bool isValue(ast_t* self);



// FIRST AST
void __init_first_ast();
ast_t * GetFirstAst();
void ModifFirstAst(ast_t * new);
void ResetFirstAst();
void clearAstNode(ast_t * node);
void freeAst(ast_t *node);
void clearAst();
void freeSingleAst(ast_t * ast);
void PrintAst();


// FUNCTION AST
bool GetFunction(struct SCOPE_STRUCT *scope , const char *funcName , ast_t ** res);


// COPY AST


typedef ast_t * (*cpy_ast_fn)(ast_t * , struct SCOPE_STRUCT * );

typedef struct {
    cpy_ast_fn function;
}cpy_ast_t;

ast_t * ast_copy(ast_t *ast,struct SCOPE_STRUCT * scope);
ast_t * ast_cpy_variable_definition_lit(char *name , ast_t * value);
ast_t * ast_copy(ast_t *ast,struct SCOPE_STRUCT * scope);
ast_t *ast_cpy_Value(ast_t * ast, struct SCOPE_STRUCT * scope);
ast_t * ast_cpy_variable(ast_t * ast, struct SCOPE_STRUCT * scope);
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
ast_t *ast_cpy_list_access(ast_t *ast , struct SCOPE_STRUCT * scope);
ast_t * ast_cpy_struct_definition(ast_t *ast,struct SCOPE_STRUCT *scope);








#endif