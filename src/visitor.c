/**
 * @file visitor.c
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

#include "include/visitor.h"
#include "include/ast.h"
#include "include/error.h"

    #include "include/bulletin.h"
    #include "include/Value.h"
    #include "include/debug.h"
    #include "include/clear.h"



#define CEFLO_GB 1




extern lexer_t lexer;
static bool visitor_error;

static ast_t *visitor_visit_get_function(scope_t *scope, ast_t *node){
    
    ast_t * functionDef = scope_get_function_definition(
        scope,
        node->function_call_name
    );
    
    if(functionDef != NULL)
        return functionDef;

    if(functionDef == NULL){
        if(scope->scope_parent != NULL ){
            return visitor_visit_get_function(scope->scope_parent,node);
        }
    }   
    return NULL; 
}


void newGb(){
    visitor_error  = false;

}



static ast_t *visitor_visit_get_struct(scope_t *scope, ast_t *node){
    
    ast_t * structdef = scope_get_struct_definition(
        scope,
        node->function_call_name
    );
    
    if(structdef != NULL)
        return structdef;

    if(structdef == NULL){
        if(scope->scope_parent != NULL ){
            return visitor_visit_get_struct(scope->scope_parent,node);
        }
    }   
    return NULL; 
}


static void newVisitorWithScope(visitor_t * visitor , scope_t *scope){
    visitor->global_scope = scope;
    newList_Ast(&visitor->constants);
}


void freeLvisitor(void *x){
    visitor_t *visitor = (visitor_t*)x;
    freeVisitor(visitor);
}

void freeVisitor(visitor_t *visitor){
    freeScope(visitor->global_scope);
}



/*

    [NOTE]
        each node is a AST NODE , and each node is assigned to a function
        VISITOR parameter save the scope of each block declaration or statement
        during a function call , new scope is defined and the function definiton save in the previous scope(depend of scope parent if this last exist) is copied
        all statement into the function definition copie is copied
        after end call, GARBAGE_FUNCTION_STRUCT clear this copie and return the result if is exist
        
        variable definition , structure definition , function definition are save in the current scope
        I dont have put secure on the multiple variable definition on different scope definition ( in a function who haves a function declaration on herself (in body function definition) )
            [the secure in the next :) ]

        the list and string value are pointers, if you update the list, the list will be updated
        yes, string is copied when this one is a parameter of function call , but not list value

        AST_OBJ for me is just a scope who a many element (variable , function ect ..)

        [HELP]
        use ctrl + f , '$' for walk to different AST branch 
*/


/**
 * @brief NEW VISITOR
 * 
 * @param visitor 
 */
void newVisitor(visitor_t * visitor){
    visitor->global_scope = newScope();
    newList_Ast(&visitor->constants);
}

void visitor_Sweep_value(visitor_t *visitor){

    int x = 0;
    for (int i = 0; i < (int)visitor->constants.size; i++){
        if(Value_decref(List_Ast_Get(&visitor->constants,i))){
            visitor->constants.list[i] = NULL;
            x++;
        }
    }
    if(x != 0){
        List_Ast_Shift_All(&visitor->constants);
        visitor->constants.size -= x;
    }
    
}

void visitor_append_value(visitor_t * visitor , ast_t * value){

    if(CEFLO_GB == 1){
        if(visitor->constants.size == 100)
            visitor_Sweep_value(visitor);
        
        List_Ast_Append(
            &visitor->constants,
            value
        );
    }


}

//                          ===================================== $COMPOUND ==============================================

/**
 * @brief big switch statment on node of tree
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit(visitor_t * visitor,ast_t *node){

    if(node == (void*)0 || visitor_error == true){
        // printf("NODE NULL \n");
        return NULL;
    }
    // printf("[< %s >] \n",AST_TO_STRING[node->type]);

    // if(node->type == AST_FUNCTION_CALL){
    //     printf("=== %s === \n",node->function_call_name);
    // }



    switch (node->type){
        case AST_CHAR:
        case AST_BOOL:
        case AST_NUMBER:
        case AST_STRING:
        case AST_NONE:
        case AST_OBJ:
        case AST_RETURN:
        case AST_BREAK:
        case AST_LFUNC:
            return visitor_visit_data(visitor,node);break;
        case AST_COMPOUND: return visitor_visit_compound(visitor,node);break;
        case AST_STRUCT_DEFINITION: return visitor_visit_struct_definition(visitor,node);break;
        case AST_STRUCT: return visitor_visit_struct(visitor,node);break;
        case AST_VARIABLE_DEFINITION: return visitor_visit_variable_definition(visitor,node);break;
        case AST_VARIABLE: return visitor_visit_variable(visitor,node);break;
        case AST_FUNCTION_CALL: return visitor_visit_function_call(visitor,node);break;
        case AST_FUNCTION_DEFINITION: return visitor_visit_function_definition(visitor,node);break;
        case AST_BINOP: return visitor_visit_binop(visitor,node);break;
        case AST_IF: return visitor_visit_if(visitor,node);break;
        case AST_WHILE: return visitor_visit_while(visitor,node);break;
        case AST_ASSIGNEMENT: return visitor_visit_assignement(visitor,node);break;
        case AST_LIST: return visitor_visit_list_init(visitor,node);break;
        case AST_LIST_ASSIGNEMENT: return visitor_visit_list_assignement(visitor,node);break;
        case AST_LIST_ACCESS: return visitor_visit_list_access(visitor,node);break;
        case AST_OBJ_ACCESS:return visitor_visit_object_access(visitor,node);break;
        case AST_NOOP:
            return node;
        default:
            printf("%s: pas implementer \n",AST_TO_STRING[node->type]);
            Clear_Ceflo(&lexer);
            visitor_error = true;
            exit(visitor_error);
            break;
    }

    return node;
}








/**
 * @brief visit list node of current branch 
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_compound(visitor_t * visitor,ast_t *node){

    for (int i = 0; i < node->list_compound->Size; i++){
        ast_t *child = List_Get(node->list_compound,i);
        ast_t *noeud =  visitor_visit(visitor,child);


        if(noeud ){
            if(noeud->type == AST_BREAK)
                return noeud;
            if(noeud ->type == AST_RETURN){
                if(noeud ->return_expr != NULL){
                    

                    ast_t * return_value = visitor_visit(visitor,noeud->return_expr); 
                    ast_t *value =  ast_copy(return_value,node->scope);  
                    value->is_return_value = true; 
                    // if(value->countScope <= -1)
                    //     value->countScope = visitor->global_scope->countScope -1;
                    // Debug_Print_Ast_Type(value);
                    // GbFree();
                    // printf("========= ");
                    // printValueAst(value,visitor);;
                    // printf("========= \n");

                    return value;
                }
            }


            // if(noeud->is_return_value)
            //     printf("noued : %d    visitor : %d (%.2p) \n",noeud->countScope-1,visitor->global_scope->countScope , visitor->global_scope );

            if((noeud->is_return_value == true) 
                &&
                noeud->countScope - 1 != visitor->global_scope->countScope

            ){
                if(visitor->global_scope->countScope == 0){
                    // printf("Yes Yes \n");
                    noeud->is_return_value =  false;

                }else{
                    // printf("No \n");
                    // Debug_Print_Ast_Type(noeud);
                    return noeud ;
                }
            }else{
                if(noeud->is_return_value)
                    // printf("Yes \n");
                noeud->is_return_value =  false;
            }

        }
    }
    
    // GbFree();
    
    return node;
}




















//                          ===================================== $LIST ==============================================


/**
 * @brief visit list Value
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_list_init(visitor_t * visitor,ast_t *node){
    Value_t * val = &node->value;
    for (int i = 0; i < val->as.listValue->Size; i++){
        val->as.listValue->Item[i] =  visitor_visit(visitor,val->as.listValue->Item[i]);
    }

    return node;
    
}



/**
 * @brief try Get an access for the list
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_list_access(visitor_t * visitor,ast_t *node){


    ast_t * variable = visitor_visit(visitor,node->left);
    ast_t * index = visitor_visit(visitor,node->list_ptr);

    if(variable->type == AST_LIST){
        if(index->type == AST_NUMBER){
            if(index->value.as.numberValue > variable->value.as.listValue->Size){
                Print_visitor_error(
                variable->line,
                variable->colone,
                "./Ceflo",
                "%s[%g] n'existe pas \n",
                variable->variable_name,
                index->value.as.numberValue
                );
                Clear_Ceflo(&lexer);
                visitor_error = true;
                exit(visitor_error);
            }

            ast_t * res = List_Get(variable->value.as.listValue,(int) index->value.as.numberValue);
            res->ref_list = variable->value.as.listValue;
            res->ref_list_index = (int) index->value.as.numberValue;
            return res;
        }
    }

    if(variable->type == AST_STRING){
        if(index->type == AST_NUMBER){
            if(index->value.as.numberValue > variable->value.as.stringValue->lenght){
                Print_visitor_error(
                variable->line,
                variable->colone,
                "./Ceflo",
                "%s[%g] n'existe pas \n",
                variable->variable_name,
                index->value.as.numberValue
                );
                Clear_Ceflo(&lexer);
                visitor_error = true;
                exit(visitor_error);
            }
            ast_t * value =  newAst(AST_CHAR);
            newValue(&value->value,CHAR);
            value->value.as.charValue = String_index(variable->value.as.stringValue,(int)index->value.as.numberValue);
            value->list_ptr = index;
            value->ref_string = variable;
            value->is_string_modif = true;
            return value;
        }
    }


    return variable;
}



/**
 * @brief modifier the list at index
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_list_assignement(visitor_t * visitor,ast_t *node){

    ast_t * variable = visitor_visit(visitor,node->assignement_left);

    ast_t * value = visitor_visit(visitor,node->assignement_value);


    if(node->op.type != TOKEN_EQUAL){
        ast_t * binop = newAst(AST_BINOP);
        binop->left = variable;
        binop->right = value;
        binop->op = node->op;
        value = visitor_visit(visitor,binop);
    }
        
    modifValue(&variable,value);
    

    
    if(variable->is_string_modif){
        string_t * str = variable->ref_string->value.as.stringValue;
        if(value->type == AST_CHAR){
            String_Set_char(str,(int)variable->list_ptr->value.as.numberValue,value->value.as.charValue);
        }
        else if(value->type == AST_NUMBER)
            String_Set_char(str,(int)variable->list_ptr->value.as.numberValue,(char) value->value.as.numberValue);
        else{
            variable->ref_string->type = AST_NONE;
        }
        variable->is_string_modif = false;
    }
    

    return value;
}
















//                          ===================================== $OBJECT ==============================================


/**
 * @brief save structure defintion
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */

ast_t * visitor_visit_struct_definition(visitor_t * visitor,ast_t *node){
    scope_append_struct_definition(
        visitor->global_scope,
        node->struct_definition_name,
        node
    );
    return node;
}



/**
 * @brief transform new + structName in AST_OBJ
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_struct(visitor_t * visitor,ast_t *node){



    ast_t *structDef = scope_get_struct_definition(
        visitor->global_scope,
        node->function_call_name
    );

    if(structDef == NULL){
        if(visitor->global_scope->scope_parent != NULL){
            structDef = visitor_visit_get_struct(visitor->global_scope->scope_parent,node);
        }
    }

    if(structDef == NULL){
        
        Print_visitor_error(
            node->line,
            node->colone,
            "./Ceflo",
            "struct %s n'est pas définit \n",
            node->function_call_name
        );
        Clear_Ceflo(&lexer);
        visitor_error = true;   
        exit(visitor_error);
        
    }


    ast_t *obj = newAst(AST_OBJ);
    newValue(&obj->value,OBJECT);
    newObject(&obj->value.as.ObjectValue);
    Obj_t * newObj = &obj->value.as.ObjectValue;
    newObj->self = newScope();
    newObj->self->countScope = node->countScope;
    allowChar(&newObj->obj_name,node->function_call_name);
    obj->countScope = node->countScope;


    visitor_t vs;
    newObj->self->scope_parent = visitor->global_scope;
    newVisitorWithScope(&vs,newObj->self);

    ast_t * init = NULL;
    
    for (int i = 0; i < structDef->struct_definition_item->Size; i++){
        ast_t *item = List_Get(structDef->struct_definition_item,i);
        if(item->type == AST_FUNCTION_DEFINITION){
            if(strcmp("__init__",item->function_definition_name) == 0){
                init = item;
            }
        }
        visitor_visit(
            &vs,
            ast_copy(item,vs.global_scope)
        );

    }


    if(init != NULL){

        free(node->function_call_name);
        allowChar(&node->function_call_name,"__init__");
        node->type = AST_FUNCTION_CALL;
        ast_t *val = visitor_visit(
            &vs,
            node
        );


        if(val == NULL)
            return val;
        

    }

    obj->is_new_obj =  true;


    return obj;

}


/**
 * @brief try get/update/call element of value OBJ
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_object_access(visitor_t * visitor,ast_t *node){

    ast_t * variable = visitor_visit(visitor,node->left);
    ast_t * res = NULL; 
    ast_t * right = node->right;

    // Debug_Print_Ast_Type(right);

    if(variable->type == AST_OBJ){
        visitor_t vs;
        newVisitorWithScope(&vs,variable->value.as.ObjectValue.self);
        return visitor_visit(&vs,node->right);
    }


    if(variable->type == AST_LIST){

        res = value_list_switch_function(visitor,variable->value.as.listValue,right);
        return res!= NULL ? res : variable;
    }

    if(variable->type == AST_STRING){

        res = value_str_switch_function(visitor,variable->value.as.stringValue,right);
        return res!= NULL ? res : variable;
    }

    return node;
}


























//                          ===================================== $VARIABLE ==============================================


/**
 * @brief save variable definition in the current scope
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_variable_definition(visitor_t * visitor,ast_t *node){

    ast_t * value = visitor_visit(visitor,node->variable_definition_value);

    if(value->type == AST_COMPOUND){
        value = newAst(AST_NONE);
    }

    value->variable_definition_static_type = node->variable_definition_static_type;

    value->countScope = visitor->global_scope->countScope;

    if(value->marked_count == 0)
        Value_incref(value);

    value->is_for_variable = true;
    
 
    if(node->variable_definition_static_type_name)
        allowChar(&value->variable_definition_static_type_name,node->variable_definition_static_type_name);

    if(value->type == AST_OBJ ){
        // je fais une copie
        if(value->is_new_obj == false){
            value = ast_copy(value,visitor->global_scope);
        }
        else
            value->is_new_obj = false;    
    }

    visitor_append_value(
        visitor,
        value
    );


    scope_append_variable_definition(
        visitor->global_scope,
        node->variable_definition_name,
        value
    );


    return node;
}



static ast_t* getVariable(visitor_t * visitor , ast_t * node){
        visitor_t visi;
        newVisitorWithScope(&visi,visitor->global_scope->scope_parent);
        return visitor_visit_variable(&visi,node);
}

/**
 * @brief Get variable definition by name ( this one return the value of the variable )
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_variable(visitor_t * visitor,ast_t *node){


    ast_t * value = scope_get_variable_definition(
        visitor->global_scope,
        node->variable_name
    );

    if(value != NULL){
        // Debug_Print_Ast_Type(value);
        value->is_for_variable = true;
        return value;
    }

    if(value == NULL){
        
        if(node->variable_forGlobal == false)
            if(visitor->global_scope->scope_parent != NULL && visitor->global_scope->scope_parent->countScope != 0 ){
                return getVariable(visitor,node);
            }
        else{
            if(visitor->global_scope->scope_parent != NULL){
                return getVariable(visitor,node);
            }
        }

    }

    if(value ==  NULL){
        Print_visitor_error(
            node->line,
            node->colone,
            "./Ceflo",
            "var %s n'est pas définit \n",
            node->variable_name
        );
        Clear_Ceflo(&lexer);
        visitor_error = true;
        exit(visitor_error);

    }

    return NULL;

}























//                          ===================================== $FUNCTION ==============================================


/**
 * @brief save function defintion in the current scope
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_function_definition(visitor_t * visitor,ast_t *node){
    scope_append_function_definition(
        visitor->global_scope,
        node->function_definition_name,
        node
    );


    return node;
}


/**
 * @brief visit a function by name
 * 
 * @param visitor 
 * @param func 
 * @param args 
 * @return ast_t* 
 */
ast_t* visitor_visit_function(visitor_t *visitor , ast_t * func , ast_t * args){
    
    // ast_t *first = GetFirstAst();
    // ResetFirstAst();

    scope_t * newscope = newScope();
    ast_t *newFunc = ast_copy(func,newscope);

    if(args != NULL){

        if(args->function_call_args.size != func->function_defintion_args.size){
            Print_visitor_error(
                func->line,
                func->colone,
                "./Ceflo",
                "func %s prends %d argument(s) \n",
                func->function_definition_name,
                func->function_defintion_args.size
            );

            freeScope(newscope);
            freeSingleAst(newFunc);
            clearAst();
            exit(1);

        }

        for (int i = 0; i < (int) args->function_call_args.size; i++){
            char * variable_name = List_Ast_Get(&newFunc->function_defintion_args,i)->variable_name;
            ast_t * value = visitor_visit(visitor,List_Ast_Get(&args->function_call_args,i));
            scope_append_variable_definition(
                newscope,
                variable_name,
                value
            );
        }
    }
    
    visitor_t visit;
    newVisitorWithScope(&visit,newscope);
    visit.global_scope->countScope = newFunc->function_definition_body->countScope; 
    visit.global_scope->scope_parent = visitor->global_scope;

    ast_t *returnValue =  visitor_visit(&visit,newFunc->function_definition_body);   

    freeScope(newscope);
    freeSingleAst(newFunc);
    // GbPush(newscope,GetFirstAst());
    // ModifFirstAst(first);   


    return returnValue;
}


/**
 * @brief visit function call
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_function_call(visitor_t * visitor,ast_t *node){

    // printf("ICI %s\n",node->function_call_name);
    ast_t * res = bulletin_switch_function(
        visitor,node,
        &node->function_call_args
    );


    if(res != NULL){
        return res;
    }

    ast_t * functionDef = NULL;

    if(node->left != NULL){
        functionDef = visitor_visit(visitor,node->left);
    } 

    if(functionDef != NULL && functionDef->type == AST_LFUNC){
        functionDef = functionDef->value.as.lfuncValue;
    }

    if(functionDef == NULL)
        functionDef =  scope_get_function_definition(
            visitor->global_scope,
            node->function_call_name
        );
    

    if(functionDef == NULL){
        if(visitor->global_scope->scope_parent != NULL ){
            functionDef = visitor_visit_get_function(visitor->global_scope->scope_parent,node);
        }
    }    

    if(functionDef == NULL){
        Print_visitor_error(
            node->line,
            node->colone,
            "./Ceflo",
            "func %s n'est pas définit \n",
            node->function_call_name
        );

            clearAst();
            exit(1);
    }


    // ast_t *first = GetFirstAst();
    // ResetFirstAst();

    scope_t * newscope = newScope();
    int visitorScopeCount = visitor->global_scope->countScope;
    if(visitorScopeCount <= 1){
        newscope->countScope = functionDef->function_definition_body->countScope + visitorScopeCount;
    }
    ast_t *newFunction = ast_copy(functionDef,newscope);
    // printf("            [ %d ] \n",visitor->global_scope->countScope);
    
    if(node->function_call_args.size != functionDef->function_defintion_args.size){
        Print_visitor_error(
            node->line,
            node->colone,
            "./Ceflo",
            "func %s prends %d argument(s) \n",
            node->function_call_name,
            functionDef->function_defintion_args.size
        );
        

        freeScope(newscope);
        // freeSingleAst(newFunction);
        Clear_Ceflo(&lexer);
        exit(visitor_error);
        exit(1);
    }

    for (int i = 0; i < (int) node->function_call_args.size; i++){
        char * variable_name = List_Ast_Get(&functionDef->function_defintion_args,i)->variable_name;
            ast_t * value = visitor_visit(visitor,List_Ast_Get(&node->function_call_args,i));

        scope_append_variable_definition(
            newscope,
            variable_name,
            value
        );
    }

    // printf("visitor : %d \n",visitor->global_scope->countScope);

    visitor_t visit;
    newVisitorWithScope(&visit,newscope);
    visit.global_scope->countScope = newFunction->function_definition_body->countScope; 
    
    visit.global_scope->scope_parent = visitor->global_scope;

    ast_t *returnValue =  visitor_visit(&visit,newFunction->function_definition_body);   
    
    // GbPush(newscope,newFunction);

    freeScope(newscope);
    freeSingleAst(newFunction);

    // ModifFirstAst(first);   


    return returnValue;
}

















//                          ===================================== $VALUE ==============================================




/**
 * @brief walk the tree of expr node
 * 
 * @param visitor 
 * @param left 
 * @param op 
 * @param right 
 * @return ast_t* 
 */
ast_t *visitor_visit_recursive_binop(visitor_t *visitor , ast_t * left , token_t *op , ast_t *right){

    #define BINOP(l,o,r)    l(left->value) o r(right->value)

    ast_t * value = NULL;
    
  
 switch (left->type){

        case AST_CHAR:
            value = newAst(AST_CHAR);
            value->countScope = visitor->global_scope->countScope;
            switch (right->type){
                    case AST_NUMBER:
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.charValue = left->value.as.charValue + right->value.as.numberValue;break;
                            case TOKEN_MINUS : value->value.as.charValue = left->value.as.charValue - right->value.as.numberValue;break;
                            case TOKEN_MULT : value->value.as.charValue = left->value.as.charValue * right->value.as.numberValue;break;
                            case TOKEN_DIV :  value->value.as.charValue = left->value.as.charValue / right->value.as.numberValue;break;
                            case TOKEN_MODULO :  value->value.as.charValue = left->value.as.charValue % (int)right->value.as.numberValue;break;
                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue < right->value.as.numberValue;break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue > right->value.as.numberValue;break;
                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue <= right->value.as.numberValue;break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue >= right->value.as.numberValue;break;
                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue == right->value.as.numberValue;break;
                            case TOKEN_OR : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue || right->value.as.numberValue;break;
                
                            case TOKEN_AND : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue && right->value.as.numberValue;break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue != right->value.as.numberValue;break;
                            default : value->value.as.charValue = left->value.as.charValue + right->value.as.numberValue;break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_NONE:
                        value->type = AST_BOOL;
                        switch (op->type){
                            case TOKEN_AND : value->value.as.booleanValue = left->value.type && right->value.type;break;
                            case TOKEN_OR : value->value.as.booleanValue = left->value.type || right->value.type;break;
                            case TOKEN_EQUAL_EQUAL : value->value.as.booleanValue = left->value.type == right->value.type;break;
                            case TOKEN_DIF : value->value.as.booleanValue = left->value.type != right->value.type;break;
                        }
                        Value_incref(value);
                        return value;
                        break;


                    case AST_CHAR:
                        value->type = AST_CHAR;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.charValue = left->value.as.charValue+ right->value.as.charValue;break;
                            case TOKEN_MINUS : value->value.as.charValue= left->value.as.charValue- right->value.as.charValue;break;
                            case TOKEN_MULT : value->value.as.charValue = left->value.as.charValue* right->value.as.charValue;break;
                            case TOKEN_DIV : value->value.as.charValue  = left->value.as.charValue/ right->value.as.numberValue;break;
                            case TOKEN_MODULO :value->type = AST_CHAR;value->value.as.charValue=left->value.as.charValue% right->value.as.charValue;break;
                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue< right->value.as.charValue;break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue> right->value.as.charValue;break;
                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue <= right->value.as.charValue;break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.charValue >= right->value.as.charValue;break;
                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue== right->value.as.charValue;break;
                            case TOKEN_OR : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue || right->value.as.charValue;break;
                            case TOKEN_AND : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue&& right->value.as.charValue;break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.charValue!= right->value.as.charValue;break;
                            default : value->value.as.charValue = left->value.as.charValue + right->value.as.charValue;break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_LFUNC:
                        value = visitor_visit_recursive_binop(
                            visitor,
                            left,
                            op,
                            visitor_visit_function(visitor,right->value.as.lfuncValue,NULL)
                        );
                        Value_incref(value);
                        return value;
                        break;

                    case AST_STRING:
                        value->type = AST_STRING;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.stringValue = String_cat("%c%s",left->value.as.charValue,right->value.as.stringValue->str);break;
                        }
                        Value_incref(value);
                        return value;
                        break;
            }
            break;





        case AST_NUMBER:
            value = newAst(AST_NUMBER);
            value->countScope = visitor->global_scope->countScope;
            switch (right->type){
                    case AST_CHAR:
                        value->type = AST_NUMBER;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.numberValue = left->value.as.numberValue+ right->value.as.charValue;break;
                            case TOKEN_MINUS : value->value.as.numberValue = left->value.as.numberValue- right->value.as.charValue;break;
                            case TOKEN_MULT : value->value.as.numberValue = left->value.as.numberValue* right->value.as.charValue;break;
                            case TOKEN_DIV : value->value.as.numberValue  = left->value.as.numberValue/ right->value.as.numberValue;break;
                            case TOKEN_MODULO :value->type = AST_NUMBER;value->value.as.numberValue = (int)left->value.as.numberValue % right->value.as.charValue;break;
                            
                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue < right->value.as.charValue;break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue> right->value.as.charValue;break;

                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue <= right->value.as.charValue;break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue >= right->value.as.charValue;break;

                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue== right->value.as.charValue;break;
                            case TOKEN_AND : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue && right->value.as.charValue;break;
                            case TOKEN_OR : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue || right->value.as.charValue;break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue!= right->value.as.charValue;break;
                            default : value->value.as.numberValue = left->value.as.numberValue + right->value.as.charValue;break;
                        }
                        Value_incref(value);
                        return value;
                        break;
                    case AST_NUMBER:
                        switch (op->type){
                            case TOKEN_PLUS :value->value.as.numberValue = left->value.as.numberValue + right->value.as.numberValue;break;
                            case TOKEN_MINUS : value->value.as.numberValue = left->value.as.numberValue - right->value.as.numberValue;break;
                            case TOKEN_MULT : value->value.as.numberValue = left->value.as.numberValue * right->value.as.numberValue;break;
                            case TOKEN_DIV :  value->value.as.numberValue = left->value.as.numberValue / right->value.as.numberValue;break;
                            case TOKEN_MODULO :  value->value.as.numberValue = (int)left->value.as.numberValue % (int)right->value.as.numberValue;break;

                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue < right->value.as.numberValue;break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue > right->value.as.numberValue;break;
                            
                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue <= right->value.as.numberValue;break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.numberValue >= right->value.as.numberValue;break;

                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue == right->value.as.numberValue;break;
                            case TOKEN_AND : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue && right->value.as.numberValue;break;
                            case TOKEN_OR : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue || right->value.as.numberValue;break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.numberValue != right->value.as.numberValue;break;
                            default : value->value.as.numberValue = left->value.as.numberValue + right->value.as.numberValue;break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_NONE:
                        value->type = AST_BOOL;
                        switch (op->type){
                            case TOKEN_AND : value->value.as.booleanValue = left->value.type && right->value.type;break;
                            case TOKEN_OR : value->value.as.booleanValue = left->value.type || right->value.type;break;
                            case TOKEN_EQUAL_EQUAL : value->value.as.booleanValue = left->value.type == right->value.type;break;
                            case TOKEN_DIF : value->value.as.booleanValue = left->value.type != right->value.type;break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_LFUNC:
                        value = visitor_visit_recursive_binop(
                            visitor,
                            left,
                            op,
                            visitor_visit_function(visitor,right->value.as.lfuncValue,NULL)
                        );
                        Value_incref(value);
                        return value;
                        break;
                    case AST_STRING:
                        value->type = AST_STRING;
                        switch (op->type){
                            case TOKEN_PLUS :
                            value->value.as.stringValue = String_cat("%g%s",left->value.as.numberValue,right->value.as.stringValue->str);break;
                        }
                        Value_incref(value);
                        return value;
                        break;
            }
            break;

        case AST_BOOL:
            value = newAst(AST_BOOL);
            value->countScope = visitor->global_scope->countScope;
            switch (right->type){


                    case AST_CHAR:
                        value->type = AST_CHAR;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.charValue = left->value.as.booleanValue + right->value.as.charValue;break;
                            case TOKEN_MINUS : value->value.as.charValue= left->value.as.booleanValue - right->value.as.charValue;break;
                            case TOKEN_MULT : value->value.as.charValue = left->value.as.booleanValue * right->value.as.charValue;break;
                            case TOKEN_DIV : value->value.as.charValue  = left->value.as.booleanValue / right->value.as.numberValue;break;
                            case TOKEN_MODULO :value->type = AST_NUMBER;value->value.as.booleanValue  =left->value.as.booleanValue % right->value.as.charValue;break;
                            
                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue  < right->value.as.charValue;break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue > right->value.as.charValue;break;
                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue  <= right->value.as.charValue;break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue >= right->value.as.charValue;break;

                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue == right->value.as.charValue;break;
                            case TOKEN_OR : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue  || right->value.as.charValue;break;
                            case TOKEN_AND : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue  && right->value.as.charValue;break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue != right->value.as.charValue;break;
                            default : value->value.as.charValue  = left->value.as.booleanValue  + right->value.as.charValue;break;
                        }
                        Value_incref(value);
                        return value;
                        break;


                    case AST_NUMBER:
                    value->type = AST_NUMBER;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.numberValue = left->value.as.booleanValue + right->value.as.numberValue;break;
                            case TOKEN_MINUS : value->value.as.numberValue = left->value.as.booleanValue - right->value.as.numberValue;break;
                            case TOKEN_MULT : value->value.as.numberValue = left->value.as.booleanValue * right->value.as.numberValue;break;
                            case TOKEN_DIV :  value->value.as.numberValue = left->value.as.booleanValue / right->value.as.numberValue;break;
                            case TOKEN_MODULO :  value->value.as.numberValue = left->value.as.booleanValue % (int)right->value.as.numberValue;break;



                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue < right->value.as.numberValue;break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue > right->value.as.numberValue;break;

                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue <= right->value.as.numberValue;break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue >= right->value.as.numberValue;break;

                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue == right->value.as.numberValue;break;
                            case TOKEN_OR : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue || right->value.as.numberValue;break;
                            case TOKEN_AND : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue && right->value.as.numberValue;break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue != right->value.as.numberValue;break;
                            default : value->value.as.numberValue = left->value.as.booleanValue + right->value.as.numberValue;break;
                        }
                        Value_incref(value);
                        return value;
                        break;
                    case AST_BOOL:
                    value->type = AST_BOOL;
                        switch (op->type){
                            case TOKEN_MODULO :  value->value.as.booleanValue = left->value.as.booleanValue % right->value.as.booleanValue;break;

                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue < right->value.as.booleanValue;break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue > right->value.as.booleanValue;break;
                            
                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue <= right->value.as.booleanValue;break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = left->value.as.booleanValue >= right->value.as.booleanValue;break;
                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue == right->value.as.booleanValue;break;
                            case TOKEN_AND : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue && right->value.as.booleanValue;break;
                            case TOKEN_OR : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue || right->value.as.booleanValue;break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = left->value.as.booleanValue != right->value.as.booleanValue;break;
                            default : value->value.as.booleanValue = left->value.as.booleanValue + right->value.as.booleanValue;break;
                        }
                        Value_incref(value);
                        return value;
                        break;
                    case AST_NONE:
                        value->type = AST_BOOL;
                        switch (op->type){
                            case TOKEN_AND : value->value.as.booleanValue = left->type && right->type;break;
                            case TOKEN_OR : value->value.as.booleanValue = left->type || right->type;break;
                            case TOKEN_EQUAL_EQUAL : value->value.as.booleanValue = left->type == right->type;break;
                            case TOKEN_DIF : value->value.as.booleanValue = left->type != right->type;break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_LFUNC:
                        value = visitor_visit_recursive_binop(
                            visitor,
                            left,
                            op,
                            visitor_visit_function(visitor,right->value.as.lfuncValue,NULL)
                        );
                        Value_incref(value);
                        return value;
                        break;
                    
                    case AST_STRING:
                        value->type = AST_STRING;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.stringValue = String_cat("%s%s",left->value.as.booleanValue ? "true":"false",right->value.as.stringValue->str);break;
                        }
                        Value_incref(value);
                        return value;
                        break;
            }
            break;

        case AST_STRING:
            value = newAst(AST_STRING);
            value->countScope = visitor->global_scope->countScope;
            switch (right->type){
                    case AST_NUMBER:
                        switch (op->type){
                            case TOKEN_PLUS :value->value.as.stringValue = String_cat("%s%g",left->value.as.stringValue->str,right->value.as.numberValue);break;
                            case TOKEN_MULT :{
                                listString_t * list = new_ListString();
                                for (int i = 0; i < right->value.as.numberValue; i++){
                                    ListString_Append(list,left->value.as.stringValue->str);
                                }
                                value->value.as.stringValue = String_Remont(list,NULL);
                                ListString_Clear(list);
                            }
                                break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_NONE:
                        value->type = AST_BOOL;
                        switch (op->type){
                            case TOKEN_AND : value->value.as.booleanValue = left->type && right->type;break;
                            case TOKEN_OR : value->value.as.booleanValue = left->type || right->type;break;
                            case TOKEN_EQUAL_EQUAL : value->value.as.booleanValue = left->type == right->type;break;
                            case TOKEN_DIF : value->value.as.booleanValue = left->type != right->type;break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_CHAR:
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.stringValue = String_cat("%s%c",left->value.as.stringValue->str,right->value.as.charValue);break;
                            // case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.charValue) < 0 ? 1 : 0; break;
                            // case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.charValue ) > 0 ? 1 : 0; break;
                            // case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.charValue) == 0 ? 1 : 0; break;
                            // case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.charValue) != 0 ? 1 : 0; break;
                        }
                        Value_incref(value);
                        return value;
                        break;

                    case AST_LFUNC:
                        value = visitor_visit_recursive_binop(
                            visitor,
                            left,
                            op,
                            visitor_visit_function(visitor,right->value.as.lfuncValue,NULL)
                        );
                        Value_incref(value);
                        return value;
                        break;

                    case AST_STRING:
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.stringValue = String_cat("%s%s",left->value.as.stringValue->str,right->value.as.stringValue->str);break;
                            
                            case TOKEN_INF : value->type = AST_BOOL ;value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.stringValue->str) < 0 ? 1 : 0; break;
                            case TOKEN_SUP : value->type = AST_BOOL ;value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.stringValue->str) > 0 ? 1 : 0; break;
                            
                            case TOKEN_INF_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.stringValue->str) <= 0 ? 1 : 0; break;
                            case TOKEN_SUP_EQUAL : value->type = AST_BOOL ;value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.stringValue->str) >= 0 ? 1 : 0; break;

                            case TOKEN_EQUAL_EQUAL : value->type = AST_BOOL ; value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.stringValue->str) == 0 ? 1 : 0; break;
                            case TOKEN_DIF : value->type = AST_BOOL ; value->value.as.booleanValue = strcmp(left->value.as.stringValue->str,right->value.as.stringValue->str) != 0 ? 1 : 0; break;
                        }
                        Value_incref(value);
                        return value;
                        break;
            }
            break;

        case AST_NONE:
                value = newAst(AST_BOOL);
                value->countScope = visitor->global_scope->countScope;
                switch (op->type){
                    case TOKEN_AND : value->value.as.booleanValue = left->type && right->type;break;
                    case TOKEN_EQUAL_EQUAL : value->value.as.booleanValue = left->type == right->type;break;
                    case TOKEN_DIF : value->value.as.booleanValue = left->type != right->type;break;
                    case TOKEN_OR : value->value.as.booleanValue = left->type || right->type;break;

                }
                Value_incref(value);
                return value;
            break;


        case AST_LFUNC:
            value = visitor_visit_recursive_binop(
                visitor,
                visitor_visit_function(visitor,left->value.as.lfuncValue,NULL),
                op,
                right
            );
            Value_incref(value);
            return value;
        break;

    }
    


    // printf("ça na pas marcher !\n");

    return value;

}

/**
 * @brief join result of walk of tree expr
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_binop(visitor_t *visitor , ast_t *node){

    ast_t *value = NULL;

    ast_t *left = visitor_visit(visitor,node->left);
    ast_t *right = visitor_visit(visitor,node->right);


    value = visitor_visit_recursive_binop(
        visitor,
        left,
        &node->op,
        right
    );

    // for marked

    // printf("            %d\n ",visitor->constants.size);
    visitor_append_value(
        visitor,
        value
    );


    return value;
}

/**
 * @brief evalue value
 * 
 * @param value 
 * @return int 
 */
static int visitor_visit_evaluate_expr(ast_t *value){

    switch (value->type){
        case AST_BOOL: return AS_BOOL(value->value) > 0;
        case AST_NUMBER: return AS_NUMBER(value->value) > 0;
        case AST_STRING: return value->value.as.stringValue->lenght > 0;
        case AST_CHAR: return AS_CHAR(value->value) > 0;
    }

    return 0;
}


/**
 * @brief value
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_data(visitor_t * visitor,ast_t *node){
    return node;
}














//                          ===================================== $CONTROLE ==============================================


/**
 * @brief for if node of tree
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */


ast_t * visitor_visit_if(visitor_t * visitor,ast_t *node){

    // printf("if - %d \n",visitor->global_scope->countScope);

    if(!node->if_expr)
        return visitor_visit(visitor,node->if_body);

    if(visitor_visit_evaluate_expr(visitor_visit(visitor,node->if_expr))){
        return visitor_visit(visitor,node->if_body);
    }else{
        if(node->if_other){
            return visitor_visit(visitor,node->if_other);
        }
        if(node->else_body){
            return visitor_visit(visitor,node->else_body);
        }
    }

    return node;
}

/**
 * @brief for while node of tree
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_while(visitor_t * visitor,ast_t *node){

    
    while (visitor_visit_evaluate_expr(visitor_visit(visitor,node->while_expr))){
        ast_t *res =  visitor_visit(visitor,node->while_body);
        if(res->is_return_value)
            return res;
        if(res->type == AST_BREAK){
            break;
        }

    }
    
    return node;
}




















//                          ===================================== $MODIFIER ==============================================




ast_t * visitor_visit_assignement(visitor_t * visitor,ast_t *node){

    ast_t * variable = visitor_visit(visitor,node->assignement_left);
    scope_t *scope = variable->scope;

    ast_t * value = visitor_visit(visitor,node->assignement_value);

    if(node->op.type != TOKEN_ID){
        ast_t * binop = newAst(AST_BINOP);
        binop->left = variable;
        binop->right = value;
        binop->op = node->op;
        value =  visitor_visit(visitor,binop);

    }

    if(value->type == AST_OBJ ){
        // je fais une copie
        if(value->is_new_obj == false){
            value = ast_copy(value,visitor->global_scope);
        }
        else
            value->is_new_obj = false;    
    }


    scope_append_variable_definition(
        scope,
        variable->assignement_variable_name,
        value
    );
    

    return value;
}