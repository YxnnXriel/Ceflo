#include <stdio.h>
#include <stdlib.h>
#include "include/ast.h"

static ast_t *var;

static ast_t *iniAst(){
    return var;
}

ast_t * GetFirstAst(){
    return iniAst();
}

void ModifFirstAst(ast_t * new){
    var = new;
}
void ResetFirstAst(){
    var = NULL;
}

void __init_first_ast(){
    ast_t *first = GetFirstAst();
    first = NULL;
}

ast_t *newAst(uint8_t type){
    ast_t *new = calloc(1,sizeof(*new));

    new->type = type;
    new->variable_definition_name = NULL;
    new->variable_definition_value = NULL;
    new->variable_definition_static_type = NONE;


    new->variable_name = NULL;

    new->function_definition_name = NULL;
    // new->function_defintion_args = NULL;
    new->function_definition_return_type = NONE;
    new->function_definition_body = NULL;

    new->function_call_name = NULL;
    // new->function_call_args = NULL;
    new->list_compound = NULL;

    new->scope = NULL;

    // binop

    new->left = NULL;
    new->right = NULL;

    new->next = var;
    var = new;

    return new;
}

void newList_Ast(list_ast_t *list){
    list->size = 0;
    for (int i = 0; i < LIST_AST_COUNT; i++){
        list->list[i] = NULL;
    }
}

int List_Ast_Append(list_ast_t *list , ast_t *ast){

    if(list->size >= LIST_AST_COUNT){
        fprintf(stderr,"%s: Capaciter depacer\n",__func__);
        return 0;
    }

    list->list[list->size] = ast;
    list->size += 1;

    return 1;
}

ast_t * List_Ast_Get(list_ast_t *list , int index){
    if(list->size == 0 || index > (int)list->size)
        return NULL;
    return list->list[index];
}

void newObject(Obj_t * object){
    newList_Ast(object->constructor_list);
    object->next = NULL;
    object->self = NULL;
}

void newValue(Value_t * value , uint8_t type){
    value->type = type;
}

void freeAst(ast_t *node){
    if(node->variable_definition_name != NULL){
        free(node->variable_definition_name);
    }

    if(node->variable_name != NULL)
        free(node->variable_name);

    if(node->function_definition_name != NULL)
        free(node->function_definition_name);
    
    if(node->function_call_name != NULL)
        free(node->function_call_name);
    if(node->value.type == STRING){
        String_free(node->value.as.stringValue);
    }

    if(node->list_compound != NULL){
        free(node->list_compound->Item);
        free(node->list_compound);
    }
    

    // if(node->scope){
    //     if(node->type == AST_COMPOUND)
    //         free(node->scope);
    // }
}


void clearAst(){
    ast_t *node = GetFirstAst();

    // printf("BEGIN \n");
    while (node != NULL){
        // printf("%s\n",AST_TO_STRING[node->type]);
        ast_t * sup = node;
        node = node->next;
        freeAst(sup);
        free(sup);
    }
    // printf("END \n");
    

}


static ast_t *ast_r_scope(ast_t *ast , struct SCOPE_STRUCT * scope){
    ast->scope = scope;

    return ast;
}

ast_t * ast_copy(ast_t *ast,struct SCOPE_STRUCT * scope){
    if(!ast)
        return NULL;
    
    switch (ast->type){
        case AST_CHAR:
        case AST_BOOL:
        case AST_NUMBER:
        case AST_STRING:
        case AST_NONE:
            return ast_r_scope(ast_cpy_Value(ast),scope);break;
        case AST_COMPOUND: return ast_r_scope(ast_cpy_compound(ast,scope),scope);break;
        case AST_VARIABLE_DEFINITION: ast_r_scope(ast_cpy_variable_definition(ast,scope),scope);break;
        case AST_VARIABLE: return ast_r_scope(ast_cpy_variable(ast),scope);break;
        case AST_FUNCTION_CALL: return ast_r_scope(ast_cpy_function_call(ast,scope),scope);break;
        case AST_FUNCTION_DEFINITION: return ast_r_scope(ast_cpy_function_definition(ast,scope),scope);break;
        case AST_BINOP:
            return ast_r_scope(ast_cpy_binop(ast,scope),scope);break;
        case AST_NOOP:
            return NULL;
    }
}


ast_t *ast_cpy_Value(ast_t * ast){
    ast_t * a = newAst(ast->type);

    if(a->type == AST_STRING){
        newValue(&a->value,STRING);
        a->value.as.stringValue = new_String(ast->value.as.stringValue->str);
    }else{
        a->value = ast->value;
    }

    return a;
}

ast_t * ast_cpy_variable(ast_t * ast){
    ast_t * variable = newAst(AST_VARIABLE);
    allowChar(&variable->variable_name,ast->variable_name);
    return variable;
}

ast_t *ast_cpy_binop(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t *a = newAst(ast->type);

    a->left = ast_copy(ast->left,scope);
    a->op = ast->op;
    a->right = ast_copy(ast->right,scope);

    return a;
}

ast_t *ast_cpy_compound(ast_t *ast , struct SCOPE_STRUCT * scope){
    ast_t * compound = newAst(AST_COMPOUND);
    compound->list_compound = __init__list__(sizeof(ast_t*));

    for (int i = 0; i < ast->list_compound->Size; i++){
        ast_t *child = List_Get(ast->list_compound,i);
        child = ast_copy(child,scope);
        List_Append(
            compound->list_compound,
            child
        );
    }
    
    return compound;
}

ast_t *ast_cpy_function_call(ast_t *ast ,struct SCOPE_STRUCT * scope ){
    ast_t * function = newAst(AST_FUNCTION_CALL);
    allowChar(&function->function_call_name,ast->function_call_name);

    newList_Ast(&function->function_call_args);

    for (int i = 0; i < (int) ast->function_call_args.size; i++){
        ast_t *arg = List_Ast_Get(&ast->function_call_args,i);
        arg = ast_copy(arg,scope);
        List_Ast_Append(&function->function_call_args,arg);
    }
    
    return function;
}

ast_t * ast_cpy_function_definition(ast_t *function , struct SCOPE_STRUCT * scope){
    
    ast_t *newFunction = newAst(AST_FUNCTION_DEFINITION);
    allowChar(&newFunction->function_definition_name,function->function_definition_name);
    newFunction->function_definition_return_type = function->function_definition_return_type;
    newFunction->scope = scope;

    newList_Ast(&newFunction->function_defintion_args);
    for (int i = 0; i < (int)function->function_defintion_args.size; i++){
        ast_t *arg = List_Ast_Get(&function->function_defintion_args,i);
        arg = ast_copy(arg,scope);
        List_Ast_Append(&newFunction->function_defintion_args,arg);
    }
    newFunction->function_definition_body = ast_copy(function->function_definition_body,scope);

    return newFunction;
}


void freeSingleAst(ast_t * node){

    ast_t * value = node;

    if(node->next == NULL){
        freeAst(node);
    }
    else{
        node = node->next;
        freeAst(value);
    }

}

ast_t * ast_cpy_variable_definition_lit(char *name , ast_t * value){
    ast_t * variable = newAst(AST_VARIABLE_DEFINITION);
    allowChar(&variable->variable_definition_name,name);
    variable->variable_definition_value = value;

    return variable;
}


ast_t * ast_cpy_variable_definition(ast_t *ast,struct SCOPE_STRUCT *scope){
    ast_t *variable = ast_cpy_variable_definition_lit(ast->variable_definition_name,NULL);
    variable->variable_definition_value = ast_copy(ast->variable_definition_value,scope);
    return variable;
}
