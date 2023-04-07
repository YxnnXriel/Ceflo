#include <stdio.h>
#include <stdlib.h>
#include "include/ast.h"
#include "include/scope.h"

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
    new->variable_definition_static_type_name = NULL;
    new->variable_name = NULL;

    new->function_definition_name = NULL;
    // new->function_defintion_args = NULL;
    new->function_definition_return_type = NONE;
    new->function_definition_body = NULL;

    new->function_call_name = NULL;
    // new->function_call_args = NULL;
    new->list_compound = NULL;
    new->type_compound = R_FUNCTION_CALL_TYPE;

    new->scope = NULL;
    new->countScope = -1;
    // binop

    new->left = NULL;
    new->right = NULL;
    new->is_return_value = false;
    // return
    new->return_expr = NULL;

    // if
    new->if_body = NULL;
    new->if_expr = NULL;
    new->else_body = NULL;
    new->if_other = NULL;

    // while
    new->while_expr = NULL;
    new->while_body = NULL;

    // TYPE DEFINITION
    new->struct_definition_item = NULL;
    new->struct_definition_name = NULL;


    new->is_new_obj = false;

    // STRING
    new->is_string_modif = false;
    new->ref_string = NULL;
    new->assignement_left = NULL;
    new->assignement_value = NULL;
    new->assignement_variable_name = NULL;
    new->list_ptr = NULL;
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
    object->self = NULL;
}

void newValue(Value_t * value , uint8_t type){
    value->type = type;
}

void modifValue(ast_t * value , ast_t * new){
    
    if(new->type == AST_STRING){

        value->value.type = STRING;
        if(value->type == AST_STRING){
            String_Set(value->value.as.stringValue,new->value.as.stringValue->str);
        }else{
            value->value.as.stringValue = new_String(new->value.as.stringValue->str);
            value->type = AST_STRING; 
        }

    }else if(new->type == AST_OBJ){
        printf("YEAP\n");
    }else{
        value->value = new->value;
        value->type = new->type;
    }
}

void freeAst(ast_t *node){

    if(node->variable_definition_name != NULL){
        free(node->variable_definition_name);
    }
    if(node->variable_definition_static_type_name != NULL){
        free(node->variable_definition_static_type_name);
    }
    if(node->variable_name != NULL)
        free(node->variable_name);

    if(node->function_definition_name != NULL)
        free(node->function_definition_name);
    
    if(node->function_call_name != NULL)
        free(node->function_call_name);

    if(node->struct_definition_name != NULL)
        free(node->struct_definition_name);

    if(node->type == AST_STRING){
        // if(node->value.as.stringValue != NULL){
            String_free(node->value.as.stringValue);
        // }
    }
    if(node->type == AST_OBJ){
        freeScope(
            node->value.as.ObjectValue.self
        );
    }

    if(node->type == AST_LIST){
        free(node->value.as.listValue->Item);
        free(node->value.as.listValue);
    }

    if(node->list_compound != NULL){
        free(node->list_compound->Item);
        free(node->list_compound);
    }

    if(node->struct_definition_item != NULL){
        free(node->struct_definition_item->Item);
        free(node->struct_definition_item);
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

void clearAstNode(ast_t * val){
    ast_t *node = val;

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
    if(ast == NULL)
        return NULL;
    
    switch (ast->type){
        case AST_CHAR:
        case AST_BOOL:
        case AST_NUMBER:
        case AST_STRING:
        case AST_OBJ:
        case AST_NONE:
            return ast_r_scope(ast_cpy_Value(ast),scope);break;
        case AST_COMPOUND: return ast_r_scope(ast_cpy_compound(ast,scope),scope);break;
        case AST_VARIABLE_DEFINITION: ast_r_scope(ast_cpy_variable_definition(ast,scope),scope);break;
        case AST_STRUCT_DEFINITION: ast_r_scope(ast_cpy_struct_definition(ast,scope),scope);break;
        case AST_VARIABLE: return ast_r_scope(ast_cpy_variable(ast),scope);break;
        case AST_STRUCT:
        case AST_FUNCTION_CALL: return ast_r_scope(ast_cpy_function_call(ast,scope),scope);break;
        case AST_FUNCTION_DEFINITION: return ast_r_scope(ast_cpy_function_definition(ast,scope),scope);break;
        case AST_OBJ_ACCESS:
        case AST_BINOP: return ast_r_scope(ast_cpy_binop(ast,scope),scope);break;
        case AST_RETURN: return ast_r_scope(ast_cpy_return(ast,scope),scope);break;
        case AST_IF: return ast_r_scope(ast_cpy_if(ast,scope),scope);break;
        case AST_WHILE: return ast_r_scope(ast_cpy_while(ast,scope),scope);break;
        case AST_BREAK: return ast_r_scope(ast_cpy_break(ast,scope),scope);break;
        case AST_LIST_ACCESS: return ast_r_scope(ast_cpy_list_access(ast,scope),scope);break;
        case AST_LIST_ASSIGNEMENT:
        case AST_ASSIGNEMENT: return ast_r_scope(ast_cpy_variable_assignement(ast,scope),scope);break;
        case AST_NOOP:
            return NULL;
    }
}


ast_t *ast_cpy_list_access(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(AST_LIST_ACCESS);

    a->left = ast_copy(ast->left, scope);
    a->list_ptr = ast_copy(ast->list_ptr, scope);
    a->countScope = ast->countScope;
    
    return a;
}

ast_t *ast_cpy_variable_assignement(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(ast->type);
    a->assignement_left = ast_copy(ast->assignement_left,scope);
    a->assignement_value = ast_copy(ast->assignement_value,scope);
    a->op = ast->op;
    a->countScope = ast->countScope;
    return a;
}

ast_t *ast_cpy_break(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t *a = newAst(ast->type);
    a->countScope = ast->countScope;
    return a;

}

ast_t *ast_cpy_while(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(ast->type);
    a->countScope = ast->countScope;
    a->while_expr = ast_copy(ast->while_expr,scope);
    a->while_body = ast_copy(ast->while_body,scope);

    return a;

}

ast_t *ast_cpy_if(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(ast->type);
    
    a->countScope = ast->countScope;
    a->if_expr = ast_copy(ast->if_expr,scope);
    a->if_body = ast_copy(ast->if_body,scope);
    a->else_body = ast_copy(ast->else_body,scope);
    a->if_other = ast_copy(ast->if_other,scope);

    return a;
}   

ast_t *ast_cpy_Value(ast_t * ast){
    ast_t * a = newAst(ast->type);
    
    a->countScope = ast->countScope;

    if(ast->type == AST_STRING){
        newValue(&a->value,STRING);
        a->value.as.stringValue = new_String(ast->value.as.stringValue->str);
    }else if (ast->type == AST_OBJ){

        a->is_new_obj = ast->is_new_obj;
        newValue(&a->value,OBJECT);
        newObject(&a->value.as.ObjectValue);
        Obj_t *obj = &a->value.as.ObjectValue;
        obj->self = newScope();
        
        scope_cpy_with_scope(
            obj->self,
            ast->value.as.ObjectValue.self
        );

    }else{
        a->value = ast->value;
    }

    return a;
}

ast_t * ast_cpy_variable(ast_t * ast){
    ast_t * variable = newAst(AST_VARIABLE);
    variable->countScope = ast->countScope;
    allowChar(&variable->variable_name,ast->variable_name);
    return variable;
}

ast_t *ast_cpy_binop(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t *a = newAst(ast->type);
    a->countScope = ast->countScope;

    a->left = ast_copy(ast->left,scope);
    a->op = ast->op;
    a->right = ast_copy(ast->right,scope);

    return a;
}

ast_t *ast_cpy_return(ast_t *ast , struct SCOPE_STRUCT * scope){
    ast_t* a = newAst(ast->type);
    a->countScope = ast->countScope;
    a->return_expr = ast_copy(ast->return_expr,scope);

    return a;
}

ast_t *ast_cpy_compound(ast_t *ast , struct SCOPE_STRUCT * scope){
    ast_t * compound = newAst(AST_COMPOUND);
    compound->countScope = ast->countScope;
    compound->list_compound = __init__list__(sizeof(ast_t*));
    compound->type_compound = ast->type_compound;

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
    ast_t * function = newAst(ast->type);

    function->countScope = ast->countScope;

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
    newFunction->countScope = function->countScope;

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
    if(ast->variable_definition_static_type_name != NULL)
        allowChar(
            &variable->variable_definition_static_type_name,
            ast->variable_definition_static_type_name
        );
        
    variable->countScope = ast->countScope;
    return variable;
}


ast_t * ast_cpy_struct_definition(ast_t *ast,struct SCOPE_STRUCT *scope){

    ast_t * a = newAst(AST_STRUCT_DEFINITION);
    a->countScope = ast->countScope;
    allowChar(&a->struct_definition_name,ast->struct_definition_name);
    a->struct_definition_item = __init__list__(sizeof(ast_t*));

    for (int i = 0; i < ast->struct_definition_item->Size; i++){
        ast_t * item = List_Get(ast->struct_definition_item,i);
        List_Append(
            a->struct_definition_item,
            ast_copy(item,scope)
        );
    }

    return a;
    
}