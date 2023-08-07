/**
 * @file ast.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/ast.h"
#include "include/scope.h"
#include "include/visitor.h"
#include "include/debug.h"
#include "include/lexer.h"


                                                // FIRST AST
static ast_t *var;
static ast_t *prev;

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
    prev = NULL;
}

void __init_first_ast(){
    ast_t *first = GetFirstAst();
    first = NULL;
}



                                // NEW AST


/**
 * @brief NEW AST
 * 
 * @param type 
 * @return ast_t* 
 */
ast_t *newAst(uint8_t type){

    ast_t *new = calloc(1,sizeof(*new));

    new->type = type;
    new->variable_definition_name = NULL;
    new->variable_definition_value = NULL;
    new->variable_definition_static_type = NONE;
    new->variable_definition_static_type_name = NULL;
    new->variable_name = NULL;
    new->variable_forGlobal = false;

    new->function_definition_name = NULL;
    // new->function_defintion_args = NULL;
    new->function_definition_return_type = NONE;
    new->function_definition_body = NULL;
    new->function_definition_return_type_name = NULL;

    new->function_call_name = NULL;
    // new->function_call_args = NULL;
    new->list_compound = NULL;
    new->type_compound = R_FUNCTION_CALL_TYPE;


    new->marked_count = 0;
    new->is_for_variable = false;

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
    new->prev = NULL;

    new->is_func_typer = false;


    // LIST
    new->ref_list = NULL;
    new->ref_list_index = -1;

    if(prev == NULL){

        new->next = NULL;
        var = new;
        prev = new;

    }else{

        var->prev = new;
        new->next = var;
        var = new;
    }
    



    return new;
}


ast_t *newAstWithLine(uint8_t type, int line , int colone){
    ast_t * new = newAst(type);
    new->line = line;
    new->colone = colone;

    return new;
}




                                        // VALUE

/**
 * @brief NEW LIST AST
 * 
 * @param list 
 */
void newList_Ast(list_ast_t *list){
    list->size = 0;
    for (int i = 0; i < LIST_AST_COUNT; i++){
        list->list[i] = NULL;
    }
}


/**
 * @brief append ast node in list ast
 * 
 * @param list 
 * @param ast 
 * @return int 
 */
int List_Ast_Append(list_ast_t *list , ast_t *ast){

    if(list->size >= LIST_AST_COUNT){
        fprintf(stderr,"%s: Capaciter depacer\n",__func__);
        return 0;
    }

    list->list[list->size] = ast;
    list->size += 1;

    return 1;
}


/**
 * @brief Get node ASt in list ast
 * 
 * @param list 
 * @param index 
 * @return ast_t* 
 */
ast_t * List_Ast_Get(list_ast_t *list , int index){
    if(list->size == 0 || index > (int)list->size)
        return NULL;
    return list->list[index];
}

void List_Ast_Sup(list_ast_t *list , int index){
    if(list->size == 0 || index > (int)list->size)
        return;
    list->list[index] = NULL;
    list->size -= 1;
}

void List_ast_Shift(list_ast_t *list , int index){
    for (int i = index; i < (int)list->size -1 ; i++){
        list->list[i] = list->list[i+1];
    }
}


void List_Ast_Shift_All(list_ast_t * list){
    for (int i = 0; i < (int)list->size; i++){
        if(list->list[i] == NULL){
            List_ast_Shift(list,i);
        }
    }
}

list_ast_t List_Ast_Cpy(list_ast_t *list){
    list_ast_t new;
    newList_Ast(&new);

    for (int i = 0; i < (int)list->size; i++){
        List_Ast_Append(
            &new,List_Ast_Get(list,i)
        );
    }
    
    return new;
}

list_ast_t List_Ast_Cpy_Without_Index(list_ast_t * list , int index){
    list_ast_t new;
    newList_Ast(&new);

    for (int i = 0; i < (int)list->size; i++){
        if(i != index){
            List_Ast_Append(
                &new, ast_copy(List_Ast_Get(list,i),NULL)
            );
        }
    }
    
    return new;
}


/**
 * @brief NEW OBJECT
 * 
 * @param object 
 */
void newObject(Obj_t * object){
    object->self = NULL;
    object->obj_name = NULL;
}

bool ObjectIsSame(Obj_t *self , const char *name){
    return (self->obj_name != NULL && strcmp(name,self->obj_name) == 0);
}




/**
 * @brief NEW VALUE
 * 
 * @param value 
 * @param type 
 */
void newValue(Value_t * value , uint8_t type){
    value->type = type;
}



/**
 * @brief print OBJ
 * 
 * @param value 
 * @return true 
 * @return false 
 */
bool printObj(ast_t * value){
    ast_t *obj = NULL; 

    if(GetFunction(value->value.as.ObjectValue.self,"__str__",&obj) == true){
        visitor_t visi;
        visi.global_scope = value->value.as.ObjectValue.self;
        printValueAst(
            visitor_visit_function(
                &visi,
                obj,
                NULL
            ),
            &visi
        );
        return true;
    }

    return false;
}


/**
 * @brief print list Value
 * 
 * @param value 
 * @param visitor 
 * @return true 
 * @return false 
 */
bool printlist(ast_t * value , struct VISITOR_STRUCT * visitor){
    printf("[ ");
    if(value->value.as.listValue->Size == 0){
        printf("Vide !");
    }else{
        for (int i = 0; i < value->value.as.listValue->Size; i++){
            ast_t * child = List_Get(value->value.as.listValue,i);

            child = visitor_visit(visitor,child);
            printValueAst(child,visitor);
            if(i !=  value->value.as.listValue->Size - 1 )
                printf(" , ");

        }
    }
    printf(" ]");
    return true;

}


bool printlistByeValue(Value_t * value , struct VISITOR_STRUCT * visitor){
    ast_t v;
    v.value = *value;
    printlist(&v,visitor);
    return true;
}

bool printlistByeList(list_t * list , struct VISITOR_STRUCT * visitor){

    Value_t val;
    newValue(&val,LIST);
    val.as.listValue = list;
    printlistByeValue(&val,visitor);
    return true;
}



/**
 * @brief Print Value
 * 
 * @param value 
 * @param visitor 
 */
void printValueAst(ast_t * value,struct VISITOR_STRUCT * visitor){

    if(value == NULL)
        return;

    switch (value->type){
            case AST_NONE : printf("none");break;
            case AST_CHAR : printf("%c",value->value.as.charValue);break;
            case AST_NUMBER : printf("%g",value->value.as.numberValue);break;
            case AST_STRING : printf("%s",value->value.as.stringValue->str);break;
            case AST_BOOL : printf("%s",value->value.as.booleanValue == 1 ? "true" : "false");break;
            case AST_LIST: printlist(value,visitor);break;
            case AST_OBJ:{
                if(printObj(value) == false){
                    printf("obj");
                    if(value->value.as.ObjectValue.obj_name != NULL)
                        printf("(%s)",value->value.as.ObjectValue.obj_name);
                    printf(":<%p>",(void *)value);
                }
            }break;
            default:
                printf("<%p>",(void *)value);
            break;
    }
    
}


/**
 * @brief Modif Value
 * 
 * @param value 
 * @param new 
 */
void modifValue(ast_t ** ref , ast_t * new){
    
    ast_t * value = *ref;
    
    if(value->type == AST_OBJ){
        freeScope(value->value.as.ObjectValue.self);
    }

    if(new->type == AST_STRING){

        value->value.type = STRING;
        if(value->type == AST_STRING){
            String_Set(value->value.as.stringValue,new->value.as.stringValue->str);
        }else{
            value->value.as.stringValue = new_String(new->value.as.stringValue->str);
            value->type = AST_STRING; 
        }

    }else if(new->type == AST_OBJ){

        value->type = AST_OBJ;


        if(new->is_new_obj)
            new->is_new_obj = false;
        
        // printf("new[  %p  ]\n",(void*)new->value.as.ObjectValue.self);
        // // value->value = ast_copy(new,NULL)->value;
        // printf("value[  %p  ]\n",(void*)value->value.as.ObjectValue.self);

        value->ref_list->Item[value->ref_list_index] = ast_copy(new,NULL);



    }else{
        value->value = new->value;
        value->type = new->type;
    }
    Value_incref(value);
    new->marked_count--;

}

void Value_incref(ast_t * value){
    value->marked_count++;
}

bool Value_decref(ast_t * value){
    if(value == NULL)
        return false;

    value->marked_count--;

    if(value->marked_count <= 0 && value->is_for_variable == false ){
        // printf("                        (%p)\n",(void*)value);
        freeSingleAst(value);
        return true;
    }
    return false;
}


/**
 * @brief Check if Ast is Value type
 * 
 * @param self 
 * @return true 
 * @return false 
 */
bool isValue(ast_t* self){
    if(
        self->type == AST_BOOL ||
        self->type == AST_CHAR ||
        self->type == AST_NUMBER ||
        self->type == AST_STRING ||
        self->type == AST_LIST ||
        self->type == AST_OBJ
    )
        return true;
    
    return false;
}





                                            // FUNCTION AST

/**
 * @brief Get the Function object
 * 
 * @param scope 
 * @param funcName 
 * @param res 
 * @return true 
 * @return false 
 */
bool GetFunction(scope_t *scope , const char *funcName , ast_t ** res){
    *res = scope_get_function_definition(
        scope,
        funcName
    );

    return *res != NULL ? true : false;
}


                                        // FIRST AST

/**
 * @brief Del ast node
 * 
 * @param node 
 */
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
    
    if(node->function_definition_return_type_name != NULL){
        free(node->function_definition_return_type_name);
    }

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
        // freeScope(
        //     node->value.as.ObjectValue.self
        // );

        if( node->value.as.ObjectValue.obj_name != NULL){
            free(node->value.as.ObjectValue.obj_name);
        }
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

}


void clearAst(){
    ast_t *node = GetFirstAst();

    // printf("BEGIN \n");
    while (node != NULL){
        // printf("%s\n",AST_TO_STRING[node->type]);
        // if(node->type == AST_OBJ){
        //     printf("    %p\n",(void*)node->value.as.ObjectValue.self);
        // }
        // if(node->type == AST_VARIABLE_DEFINITION){
        //     printf("    %s\n",node->variable_definition_name);
        // }
        // if(node->type == AST_STRING){
        //     printf("    %p\n",node->value.as.stringValue);
        // }
        ast_t * sup = node;
        node = node->next;
        freeAst(sup);
        free(sup);
    }
    // printf("END \n");
    

}

void freeSingleAst(ast_t * ast){

    if(ast != NULL){

        if(ast->next == NULL){
            prev = ast->prev;
            prev->next = NULL;
        }else if(ast->prev == NULL){
            var = ast->next;
            var->prev = NULL;
        }else{
            ast->next->prev = ast->prev;
            ast->prev->next = ast->next;
        }
        freeAst(ast);
        free(ast);
    }

}


void clearAstNode(ast_t * node){

    while (node != NULL){
        // printf("%s\n",AST_TO_STRING[node->type]);
        ast_t * sup = node;
        node = node->next;
        freeAst(sup);
        free(sup);
    }
    // printf("END \n");

}

void PrintAst(){
    ast_t * node = var;
    while (node != NULL){
        printValueAst(node,NULL);
        printf("\n");
        node = node->next;
    }
    
}




                                    // COPY AST



cpy_ast_t list_cpy_ast[] = {
    
    [AST_CHAR] = {&ast_cpy_Value},
    [AST_BOOL] = {&ast_cpy_Value},
    [AST_NUMBER] = {&ast_cpy_Value},
    [AST_STRING] = {&ast_cpy_Value},
    [AST_OBJ] = {&ast_cpy_Value},
    [AST_NONE] = {&ast_cpy_Value},
    [AST_LIST] = {&ast_cpy_Value},
    [AST_COMPOUND] = {&ast_cpy_compound},
    [AST_VARIABLE_DEFINITION] = {&ast_cpy_variable_definition},
    [AST_STRUCT_DEFINITION] = {&ast_cpy_struct_definition},
    [AST_VARIABLE] = {&ast_cpy_variable},    
    [AST_STRUCT] = {&ast_cpy_function_call},
    [AST_FUNCTION_CALL] = {&ast_cpy_function_call},
    [AST_FUNCTION_DEFINITION] = {&ast_cpy_function_definition},
    [AST_LFUNC] = {&ast_cpy_Value},
    [AST_OBJ_ACCESS] = {&ast_cpy_binop},
    [AST_BINOP] = {&ast_cpy_binop},
    [AST_UNOP] = {&ast_cpy_binop},
    [AST_RETURN] = {&ast_cpy_return},
    [AST_IF] = {&ast_cpy_if},
    [AST_WHILE] = {&ast_cpy_while},
    [AST_BREAK] = {&ast_cpy_break},
    [AST_LIST_ACCESS] = {&ast_cpy_list_access},
    [AST_LIST_ASSIGNEMENT] = {&ast_cpy_variable_assignement},
    [AST_ASSIGNEMENT] = {&ast_cpy_variable_assignement},

    [AST_NOOP] = {NULL }
};


static void cpy_line(ast_t *a , ast_t *ast){
    a->line = ast->line;
    a->colone = ast->colone;
    a->countScope = ast->countScope;
}

static ast_t *ast_r_scope(ast_t *ast , struct SCOPE_STRUCT * scope){
    ast->scope = scope;
    
    if(scope != NULL)
        ast->countScope = scope->countScope;
    
    return ast;
}

static cpy_ast_t *Get_Function_cpy(uint8_t type){
    return &list_cpy_ast[type];
}

ast_t * ast_copy(ast_t *ast,struct SCOPE_STRUCT * scope){

    if(ast == NULL){
        return NULL;
    }

    if(ast->type == AST_NOOP)
        return ast;

    // printf("CPY %s \n",AST_TO_STRING[ast->type]);
    ast_t *res = list_cpy_ast[ast->type].function(ast,scope);

    ast_r_scope(res,scope);

    return res;
}


ast_t *ast_cpy_list_access(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(AST_LIST_ACCESS);

    a->left = ast_copy(ast->left, scope);
    a->list_ptr = ast_copy(ast->list_ptr, scope);
    if(scope == NULL){
        cpy_line(a,ast);
    }

    
    return a;
}

ast_t *ast_cpy_variable_assignement(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(ast->type);
    a->assignement_left = ast_copy(ast->assignement_left,scope);
    a->assignement_value = ast_copy(ast->assignement_value,scope);
    a->op = ast->op;
    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;
}

ast_t *ast_cpy_break(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t *a = newAst(ast->type);
    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;

}

ast_t *ast_cpy_while(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(ast->type);
    a->while_expr = ast_copy(ast->while_expr,scope);
    a->while_body = ast_copy(ast->while_body,scope);
    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;

}

ast_t *ast_cpy_if(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t * a = newAst(ast->type);
    
    a->if_expr = ast_copy(ast->if_expr,scope);
    a->if_body = ast_copy(ast->if_body,scope);
    a->else_body = ast_copy(ast->else_body,scope);
    a->if_other = ast_copy(ast->if_other,scope);

    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;
}   

ast_t *ast_cpy_Value(ast_t * ast, struct SCOPE_STRUCT * scope){
    ast_t * a = newAst(ast->type);

    if(ast->type == AST_STRING){
        newValue(&a->value,STRING);
        a->value.as.stringValue = new_String(ast->value.as.stringValue->str);
    }else if (ast->type == AST_OBJ){

        a->is_new_obj = ast->is_new_obj;
        newValue(&a->value,OBJECT);
        newObject(&a->value.as.ObjectValue);
        Obj_t *obj = &a->value.as.ObjectValue;
        obj->self = newScope();
        
        if(ast->value.as.ObjectValue.obj_name != NULL){
            allowChar(&a->value.as.ObjectValue.obj_name,ast->value.as.ObjectValue.obj_name);
        }
        
        scope_cpy_with_scope(
            obj->self,
            ast->value.as.ObjectValue.self
        );

    }else if(ast->type == AST_LIST){
        newValue(&a->value,LIST);
        a->value.as.listValue = __init__list__(sizeof(ast_t*));

        for (int i = 0; i < ast->value.as.listValue->Size; i++){
            List_Append(
                a->value.as.listValue,
                ast_copy((ast_t*)List_Get(ast->value.as.listValue,i),scope)
            );
        }
    }else if(ast->type == AST_LFUNC){

        newValue(&a->value,LFUNC);
        a->value.as.lfuncValue = ast_copy(ast->value.as.lfuncValue,scope);
    }else{
        a->value = ast->value;
    }

    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;
}

ast_t * ast_cpy_variable(ast_t * ast,struct SCOPE_STRUCT * scope){
    ast_t * a = newAst(AST_VARIABLE);
    a->variable_forGlobal = ast->variable_forGlobal; 
    a->is_func_typer = ast->is_func_typer;
    allowChar(&a->variable_name,ast->variable_name);

    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;
}

ast_t *ast_cpy_binop(ast_t *ast , struct SCOPE_STRUCT * scope){

    ast_t *a = newAst(ast->type);

    a->left = ast_copy(ast->left,scope);
    a->op = ast->op;
    a->right = ast_copy(ast->right,scope);

    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;
}

ast_t *ast_cpy_return(ast_t *ast , struct SCOPE_STRUCT * scope){
    ast_t* a = newAst(ast->type);
    a->return_expr = ast_copy(ast->return_expr,scope);

    if(scope == NULL){
        cpy_line(a,ast);
    }
    return a;
}

ast_t *ast_cpy_compound(ast_t *ast , struct SCOPE_STRUCT * scope){
    ast_t * a = newAst(AST_COMPOUND);
    a->list_compound = __init__list__(sizeof(ast_t*));
    a->type_compound = ast->type_compound;

    for (int i = 0; i < ast->list_compound->Size; i++){
        ast_t *child = List_Get(ast->list_compound,i);
        child = ast_copy(child,scope);
        List_Append(
            a->list_compound,
            child
        );
    }

    if(scope == NULL){
        cpy_line(a,ast);
    }

    return a;
}

ast_t *ast_cpy_function_call(ast_t *ast ,struct SCOPE_STRUCT * scope ){
    ast_t * a = newAst(ast->type);


    allowChar(&a->function_call_name,ast->function_call_name);
    a->is_func_typer = ast->is_func_typer;

    newList_Ast(&a->function_call_args);


    for (int i = 0; i < (int) ast->function_call_args.size; i++){
        ast_t*  arg = ast_copy( List_Ast_Get(&ast->function_call_args,i),scope);
        List_Ast_Append(&a->function_call_args,arg);
        // Debug_Print_Ast_Type(arg);
    }

    if(scope == NULL){
        cpy_line(a,ast);
    }
    return a;
}

ast_t * ast_cpy_function_definition(ast_t *function , struct SCOPE_STRUCT * scope){
    
    ast_t *a = newAst(function->type);

    a->function_definition_return_type = function->function_definition_return_type;

    if(function->function_definition_name){
        allowChar(&a->function_definition_name,function->function_definition_name);
    }
    
    if(function->function_definition_return_type_name){
        
        allowChar(&a->function_definition_return_type_name,function->function_definition_return_type_name);
    }

    a->function_definition_return_type = function->function_definition_return_type;

    newList_Ast(&a->function_defintion_args);
    for (int i = 0; i < (int)function->function_defintion_args.size; i++){
        ast_t *arg = List_Ast_Get(&function->function_defintion_args,i);
        arg = ast_copy(arg,scope);
        List_Ast_Append(&a->function_defintion_args,arg);
    }
    


    a->function_definition_body = ast_copy(function->function_definition_body,scope);
    cpy_line(a,function);


    return a;
}



ast_t * ast_cpy_variable_definition_lit(char *name , ast_t * value){
    ast_t * variable = newAst(AST_VARIABLE_DEFINITION);
    allowChar(&variable->variable_definition_name,name);
    variable->variable_definition_value = value;
    
    return variable;
}


ast_t * ast_cpy_variable_definition(ast_t *ast,struct SCOPE_STRUCT *scope){
    ast_t *a = ast_cpy_variable_definition_lit(ast->variable_definition_name,NULL);
    a->variable_definition_value = ast_copy(ast->variable_definition_value,scope);
    if(ast->variable_definition_static_type_name != NULL)
        allowChar(
            &a->variable_definition_static_type_name,
            ast->variable_definition_static_type_name
        );

    if(scope == NULL){
        cpy_line(a,ast);
    }

    if(ast->left != NULL){
        a->left = ast_cpy_variable_definition(ast->left,scope);
    }

        
    return a;
}


ast_t * ast_cpy_struct_definition(ast_t *ast,struct SCOPE_STRUCT *scope){

    ast_t * a = newAst(AST_STRUCT_DEFINITION);
    allowChar(&a->struct_definition_name,ast->struct_definition_name);
    a->struct_definition_item = __init__list__(sizeof(ast_t*));

    for (int i = 0; i < ast->struct_definition_item->Size; i++){
        ast_t * item = List_Get(ast->struct_definition_item,i);
        List_Append(
            a->struct_definition_item,
            ast_copy(item,scope)
        );
    }


    if(scope == NULL){
        cpy_line(a,ast);
    }


    return a;
    
}