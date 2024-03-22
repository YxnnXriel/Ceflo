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
#include "include/value_expr.h"

    #include "include/bulletin.h"
    #include "include/Value.h"
    #include "include/debug.h"
    #include "include/clear.h"
    #include "include/typer.h"



#define CEFLO_GB 1


extern lexer_t lexer;

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

static ast_t *getStruct(scope_t *scope, const char *struct_name){
    ast_t * structdef = scope_get_struct_definition(
        scope,
        struct_name
    );
    
    if(structdef != NULL)
        return structdef;

    if(structdef == NULL){
        if(scope->scope_parent != NULL ){
            return getStruct(scope->scope_parent,struct_name);
        }
    }   
    return NULL; 
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


static bool ismethodHaveSelf(ast_t * method){
    for (int i = 0; i < (int)method->function_defintion_args.size; i++){
        ast_t  * item = List_Ast_Get(&method->function_defintion_args,i);
        if(strcmp(item->variable_name,"self") == 0)
            return true;
    }
    return false;
}

static void newVisitorWithScope(visitor_t * visitor , scope_t *scope){
    visitor->global_scope = scope;
    visitor->is_for_obj = scope->is_for_obj;
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
    visitor->is_for_obj = false;
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
        if(visitor->constants.size == 100){

            visitor_Sweep_value(visitor);
        }
        
        List_Ast_Append(
            &visitor->constants,
            value
        );
    }


}


int  visitor_visit_code(ast_t* node){
    ResetFirstScope();
    visitor_t visitor;
	newVisitor(&visitor);
	visitor.global_scope->countScope = 0;
	visitor_visit(&visitor,node);

    clear_ceflo_clean();

    return 0;

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

    if(node == (void*)0){
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
        case AST_UNOP: return visitor_visit_unop(visitor,node);break;
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
            clear_ceflo_clean();
            exit(1);
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
                    return_value->is_return_value = true;
                    return return_value;
                }
            }

            if((noeud->is_return_value == true) 
                &&
                noeud->countScope - 1 != visitor->global_scope->countScope

            ){
                if(visitor->global_scope->countScope == 0){
                    noeud->is_return_value =  false;
                }
                else{
                    return noeud ;
                }
            }else{
                if(noeud->is_return_value){
                    noeud->is_return_value =  false;
                }
            }

        }
    }
    
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
                clear_ceflo_clean();
                exit(1);
            }

            ast_t * res = List_Get(variable->value.as.listValue,(int) index->value.as.numberValue);

            if(res == NULL){
                Print_visitor_error(
                variable->line,
                variable->colone,
                "./Ceflo",
                "%s[%g] n'existe pas \n",
                variable->variable_name,
                index->value.as.numberValue
                );
                clear_ceflo_clean();
                exit(1);
            }
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
                clear_ceflo_clean();
                exit(1);
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


    return index;
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
        clear_ceflo_clean();
        exit(1);
        
    }


    // if(strcmp(structDef->struct_definition_name,"DefinePointName")==0){

    //     Print_Scope(visitor->global_scope,"PREMIER SCOPE");

    //     for (int i = 0; i < node->function_call_args.size; i++){
    //         ast_t * item = List_Ast_Get(&node->function_call_args,i);
    //         if(Debug_Print_Ast_Type(item) == AST_VARIABLE){
    //             printf("%s\n",item->variable_name);
    //         }
    //     }
        
    // }

    ast_t *obj = newAst(AST_OBJ);
    newValue(&obj->value,OBJECT);
    
    Obj_t * newObj = &obj->value.as.ObjectValue;
    newObject(newObj);

    newObj->self = newScope();
    newObj->self->countScope = node->countScope;
    newObj->self->is_for_obj = true;
    obj->is_new_obj =  true;

    obj->countScope = node->countScope;

    newObj->obj_name = node->function_call_name;

    newObj->self->is_for_obj = true;

    visitor_t vs;
    newVisitorWithScope(&vs,newObj->self);
    newObj->self->scope_parent = visitor->global_scope;
    ast_t * init = NULL;

    obj->scope = newObj->self;

    //TODO: ecrire la methode getattr pour les object

    /*LOGIC**/


    for (int i = 0; i < structDef->struct_definition_item->Size; i++){
        ast_t *item = List_Get(structDef->struct_definition_item,i);
        if(item->type == AST_FUNCTION_DEFINITION){
            if(strcmp("__init__",item->function_definition_name) == 0 && init == NULL){
                // if(node->function_call_args.size == item->function_defintion_args.size)
                    init = item;
                continue;

            }

            if(strcmp("__str__",item->function_definition_name)==0){
                newObj->__str__ = item;
                continue;
            }

            if(strcmp("__cast__",item->function_definition_name)== 0){
                newObj->__cast__ = item;
                continue;
            }

        }

        visitor_visit(
            &vs,
            ast_copy(item,vs.global_scope)
        );
    }


    if(init != NULL){

        // free(node->function_call_name);
        // allowChar(&node->function_call_name,"__init__");
        // node->type = AST_FUNCTION_CALL;
        // ast_t *val = visitor_visit(
        //     &vs,
        //     node
        // );
        
        Obj_t *init_self = NULL;
        if(init->is_define_self)
            init_self = newObj; 

        // printf("    [< %s >]    \n",node->function_call_name);
        visitor_visit_method(
            visitor,
            init_self,
            init,
            &node->function_call_args 
        );

        // je reset la portée de depart
        newObj->self->scope_parent = NULL;
        newObj->self->is_for_obj = true;


    }



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
    

    if(variable->type == AST_OBJ){
        Obj_t * self = &variable->value.as.ObjectValue;
        visitor_t vs;
        newVisitorWithScope(&vs,self->self);
        
        if(right->type == AST_FUNCTION_CALL){
            
            ast_t *method = scope_get_function_definition(
                self->self,
                right->function_call_name
            );

            if(method == NULL){
                method = ObjectGetAttr(self,right->function_call_name,1);
            }

            if(method == NULL){
                Print_visitor_error(
                    right->line,
                    right->colone,
                    "./Ceflo","La methode `%s` n'est pas definit ou `%s` n'est pas un object\n",
                    right->function_call_name,
                    variable->assignement_variable_name
                );
            }
            if(!method->is_define_self){
                self = NULL;
            }

            res = visitor_visit_method(
                visitor,
                self,
                method,
                &right->function_call_args
            );

        }else{
            res = visitor_visit(&vs,right);
        }

        return res;
    }


    if(variable->type == AST_LIST){

        res = value_list_switch_function(visitor,variable->value.as.listValue,right);
        if(res != NULL)
            return res;
    }

    if(variable->type == AST_STRING){

        res = value_str_switch_function(visitor,variable->value.as.stringValue,right);
        if(res != NULL)
            return res;
    }


    if(right->type == AST_FUNCTION_CALL){
        Print_visitor_error(
            right->line,
            right->colone,
            "./Ceflo","La methode `%s` n'est pas definit ou `%s` n'est pas un object\n",
            right->function_call_name,
            variable->assignement_variable_name
        );
    }
    if(right->type == AST_VARIABLE){
        Print_visitor_error(
            right->line,
            right->colone,
            "./Ceflo","L'attribut `%s` n'est pas definit ou `%s` n'est pas un object\n",
            right->variable_name,
            variable->assignement_variable_name
        );
    }


    clear_ceflo_clean();
    exit(1);

    return NULL;
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

    if(node->variable_definition_static_type != NONE){
        ast_t *res = typer_caste_value(node->variable_definition_static_type + 4,&value);

        if(res == NULL){
            Print_visitor_error(
                node->variable_definition_value->line,
                node->variable_definition_value->colone,
                "./Ceflo",
                "casting du type impossible !\n"
            );
            clear_ceflo_clean();
            exit(1);
        }
    }



    // if(value->marked_count == 0)
    //     Value_incref(value);

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
    


    // visitor_append_value(
    //     visitor,
    //     value
    // );


    scope_append_variable_definition(
        visitor->global_scope,
        node->variable_definition_name,
        value
    );

    if(node->left != NULL){
        visitor_visit_variable_definition(
            visitor,
            node->left
        );
    }


    return value;
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
        value->is_for_variable = true;
        value->scope = visitor->global_scope;
        value->assignement_variable_name = node->variable_name;
        return value;
    }


    if(value == NULL && visitor->is_for_obj == false ){
        // else{
            if(visitor->global_scope != NULL && visitor->global_scope->scope_parent != NULL){
                return getVariable(visitor,node);
        //         return getVariable(visitor,node);
            }
        // }
    }

        Print_visitor_error(
            node->line,
            node->colone,
            "./Ceflo",
            "var %s n'est pas définit \n",
            node->variable_name
        );

        clear_ceflo_clean();
        exit(1);

}























//                          ===================================== $FUNCTION ==============================================



/** TODO: Voir la partie OBJ , je cherche le resolveur de OBJ , ou je cherche la fonction qui vas caster en objet
*/


static ast_t *is_function_return_type(visitor_t *visitor, ast_t *function , ast_t *value){
    if(function->function_definition_return_type != NONE){
        if(function->function_definition_return_type != OBJECT){
            return typer_caste_value(function->function_definition_return_type + 4, &value);
        }else{

            if(value->type != AST_OBJ) return NULL;
        

            const char *name = function->function_definition_return_type_name;
            ast_t *structt = getStruct(visitor->global_scope,name);

            bool is_sameObj = ObjectIsSame(&value->value.as.ObjectValue,name) == 1;
            if(is_sameObj){
                return value;
            }else{
                // je recuperere déja la structure par son som

                ast_t * structure = getStruct(visitor->global_scope,name);

                if(structure != NULL){

                    ast_t * function = GetInStruct(
                        structure,
                        "FUNC",
                        "__cast__"
                    );
                    
                    if(function != NULL){

                        /**
                         * TODO: reparer la fonction __init__
                         * 
                         */

                        list_ast_t args;
                        newList_Ast(&args);
                        List_Ast_Append(&args,value);
                        return visitor_visit_function(visitor,function,&args);
                    }

                }

                return NULL;
            }
        }
    }

    return value;
}

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



ast_t* visitor_visit_method(visitor_t *visitor ,Obj_t * self ,  ast_t * method , list_ast_t * args){
    
    scope_t * newscope = newScope();
    visitor_t visit;
    newVisitorWithScope(&visit,newscope);


    visit.global_scope->countScope = method->function_definition_body->countScope; 
    if(visitor != NULL)    
        visit.global_scope->scope_parent = visitor->global_scope;


    if(self != NULL && method->is_define_self == true){
        ast_t * self_to_ast = create_self_ast(self);
        scope_append_variable_definition(
            newscope,
            "self",
            self_to_ast
        );
    }



    if(args != NULL){

        if(args->size != method->function_defintion_args.size){
            Print_visitor_error(
                method->line,
                method->colone,
                "./Ceflo",
                "func %s prends %d argument(s) \n",
                method->function_definition_name,
                method->function_defintion_args.size
            );

            clear_ceflo_clean();
            exit(1);
        }




        // printf("[%s]\n",func->function_definition_name);
        for (int i = 0; i < (int) args->size; i++){
            ast_t *args1 = List_Ast_Get(&method->function_defintion_args,i);
            ast_t * value = NULL; 

            char * variable_name = args1->variable_name;

            // TODO : mettre self dans une chaine static
 
            value = visitor_visit(visitor,List_Ast_Get(args,i));
            
            // printf("       {%s} ",variable_name);
     
            if(args1->variable_definition_static_type != NONE){
                ast_t *res = typer_caste_value(args1->variable_definition_static_type + 4,&value);

                if(res == NULL && args1->variable_definition_static_type + 4 == AST_OBJ ){
                    res = typer_caste_value_try_obj(AST_OBJ,&value,args1);
                }

                if(res == NULL){
                    Print_visitor_error(
                    args1->line,
                    args1->colone,
                    "./Ceflo",
                    "casting du type impossible !\n"
                );
                
                clear_ceflo_clean();
                exit(1);
            }
        }


            
            scope_append_variable_definition(
                newscope,
                variable_name,
                value
            );
        }
    }
    


    ast_t *returnValue =  visitor_visit(&visit,method->function_definition_body);   

    freeSingleScope(newscope);

    returnValue = is_function_return_type(visitor,method,returnValue);

    if(returnValue == NULL){
        Print_visitor_error(
            method->line,
            method->colone,
            "./Ceflo",
            "func %s : retourne une value precise !\n",
            method->function_definition_name
        );
        clear_ceflo_clean();
        exit(1);
    }


    return returnValue;
}



/**
 * @brief visit a function by name
 * 
 * @param visitor 
 * @param func 
 * @param args 
 * @return ast_t* 
 */
ast_t* visitor_visit_function(visitor_t *visitor , ast_t * func , list_ast_t * args){
    
    scope_t * newscope = newScope();
    visitor_t visit;
    newVisitorWithScope(&visit,newscope);


    visit.global_scope->countScope = func->function_definition_body->countScope; 
    visit.global_scope->scope_parent = visitor->global_scope;



    if(args != NULL){

        if(args->size != func->function_defintion_args.size){
            Print_visitor_error(
                func->line,
                func->colone,
                "./Ceflo",
                "func %s prends %d argument(s) \n",
                func->function_definition_name,
                func->function_defintion_args.size
            );

            clear_ceflo_clean();
            exit(1);
        }

        // printf("[%s]\n",func->function_definition_name);


        for (int i = 0; i < (int) args->size ; i++){

            ast_t *args1 = List_Ast_Get(&func->function_defintion_args,i);

            char * variable_name = args1->variable_name;
            ast_t * val = List_Ast_Get(args,i);
            ast_t * value = NULL;

            value = visitor_visit(visitor,val);

            if(args1->variable_definition_static_type != NONE){
                    ast_t *res = typer_caste_value(args1->variable_definition_static_type + 4,&value);

                    if(res == NULL && args1->variable_definition_static_type + 4 == AST_OBJ ){
                        res = typer_caste_value_try_obj(AST_OBJ,&value,args1);
                    }

                    if(res == NULL){
                        Print_visitor_error(
                        args1->line,
                        args1->colone,
                        "./Ceflo",
                        "casting du type impossible !\n"
                    );
                    
                    clear_ceflo_clean();
                    exit(1);
                }
            }


            scope_append_variable_definition(
                newscope,
                variable_name,
                value
            );
        }
        
    }
    



    ast_t *returnValue =  visitor_visit(&visit,func->function_definition_body);   

    freeSingleScope(newscope);

    returnValue = is_function_return_type(visitor,func,returnValue);

    if(returnValue == NULL){
        Print_visitor_error(
            func->line,
            func->colone,
            "./Ceflo",
            "func %s : retourne une value precise !\n",
            func->function_definition_name
        );
        clear_ceflo_clean();
        exit(1);
    }


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

    if(node->is_func_typer){
        res = typer_typer(visitor,node->function_call_name,&node->function_call_args);
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

        clear_ceflo_clean();
        exit(1);
    }


    // scope_t * newscope = newScope();
    // int visitorScopeCount = visitor->global_scope->countScope;
    // if(visitorScopeCount <= 1){
    //     newscope->countScope = functionDef->function_definition_body->countScope + visitorScopeCount;
    // }
    
    // je faisait une copie , c'est plus valid
    // ast_t *newFunction = ast_copy(functionDef,newscope);
    
    if(node->function_call_args.size != functionDef->function_defintion_args.size){
        Print_visitor_error(
            node->line,
            node->colone,
            "./Ceflo",
            "func %s prends %d argument(s) \n",
            node->function_call_name,
            functionDef->function_defintion_args.size
        );
        

        clear_ceflo_clean();
        exit(1);
    }

    return visitor_visit_function(visitor,functionDef,&node->function_call_args);
    // for (int i = 0; i < (int) node->function_call_args.size; i++){
        
    //     ast_t *args = List_Ast_Get(&functionDef->function_defintion_args,i);

    //     char * variable_name = args->variable_name;
    //     ast_t * value =  visitor_visit(visitor,List_Ast_Get(&node->function_call_args,i));;
    //     ast_t * expr = List_Ast_Get(&node->function_call_args,i);

    //     if(args->variable_definition_static_type != NONE){

    //         ast_t *res = typer_caste_value(args->variable_definition_static_type + 4,&value);
    //         if(res == NULL && args->variable_definition_static_type + 4 == AST_OBJ ){
    //             res = typer_caste_value_try_obj(AST_OBJ,&value,args);
    //         }
    //         if(res == NULL){
    //             Print_visitor_error(
    //                 args->line,
    //                 args->colone,
    //                 "./Ceflo",
    //                 "casting du type impossible !\n"
    //             );

    //             clear_ceflo_clean();
    //             exit(1);
    //         }
    //     }

        

    //     scope_append_variable_definition(
    //         newscope,
    //         variable_name,
    //         value
    //     );
    // }

    // visitor_t visit;
    // newVisitorWithScope(&visit,newscope);
    // // je faisait une copie or je pouvais parcourir cette fonction avec une nouvelle porter de definition de varaible
    // visit.global_scope->countScope = functionDef->function_definition_body->countScope; 
    // // visit.global_scope->countScope = newscope->countScope;
    // visit.global_scope->scope_parent = visitor->global_scope;

    // ast_t *returnValue =  visitor_visit(&visit,functionDef->function_definition_body);   
    
    // freeSingleScope(newscope);
    // // freeSingleAst(newFunction);

    // returnValue = is_function_return_type(visitor,functionDef,returnValue);

    // if(returnValue == NULL){
    //     Print_visitor_error(
    //         node->line,
    //         node->colone,
    //         "./Ceflo",
    //         "func %s : retourne une value precise !\n",
    //         functionDef->function_definition_name
    //     );
    //     clear_ceflo_clean();
    //     exit(1);
    // }



    // return returnValue;
}









//                          ===================================== $VALUE ==============================================




void visitor_visit_tree(visitor_t *visitor,Value_t * value, Value_t * left , token_t * op, Value_t * right){

    switch (left->type){

        case CHAR:
            performe_visit_tree_char(value,left,op,right);
        break;

        case NUMBER:
            performe_visit_tree_number(value,left,op,right);
        break;

        case BOOL : 
            performe_visit_tree_bool(value,left,op,right);
        break;
        case STRING:
            performe_visit_tree_str(value,left,op,right);
            break;
        case NONE:
            performe_visit_tree_none(value,left,op,right);
            break;
        default :
            Print_visitor_error(
                0,
                0,
                "./Ceflo",
                " operator '-' n'est pas compatible avec l'expression qui suit \n"
            );
            clear_ceflo_clean();
            exit(1);
        break;
    }
}



/**
 * @brief join result of walk of tree expr
 * 
 * @param visitor 
 * @param node 
 * @return ast_t* 
 */
ast_t * visitor_visit_binop(visitor_t *visitor , ast_t *node){


    Value_t buffer;

    ast_t *left = visitor_visit(visitor,node->left);
    ast_t *right = visitor_visit(visitor,node->right);


    visitor_visit_tree(
        visitor,
        &buffer,
        &left->value,
        &node->op,
        &right->value
    );


    ast_t * value = newAst(value_to_type_ast(&buffer));
    value->value = buffer;
    return value;
}





ast_t * visitor_visit_unop(visitor_t * visitor , ast_t * node){


    ast_t * left = visitor_visit(visitor,node->right);

    if(node->op.type == TOKEN_PLUS){
        return left;
    }else if(node->op.type == TOKEN_MINUS){

        if(left->type == AST_NUMBER){
            left->value.as.numberValue *= -1; 
        }else if(left->type == AST_CHAR){
            left->value.as.charValue *= -1; 
        }else{
            Print_visitor_error(
                node->line,
                node->colone,
                "./Ceflo",
                " operator '-' n'est pas compatible avec l'expression qui suit \n"
            );
            clear_ceflo_clean();
            exit(1);
        }
    }

    return left;

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



static void visitor_visit_assignement_value(ast_t * node , ast_t *value){
    value->is_for_variable = node->is_for_variable;
    if(node->variable_definition_static_type != NONE)
        value->variable_definition_static_type = node->variable_definition_static_type;

}

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

    if(variable->variable_definition_static_type != NONE){
        ast_t * res = typer_caste_value(variable->variable_definition_static_type + 4,&value);

        if(res == NULL){
            Print_visitor_error(
                node->line,
                node->colone,
                "./Ceflo",
                "casting du type impossible !\n"
            );
            clear_ceflo_clean();
            exit(1);
        }

        value->variable_definition_static_type = variable->variable_definition_static_type;
    }

    if(value->type == AST_OBJ ){
        // je fais une copie
        if(value->is_new_obj == false){
            value = ast_copy(value,visitor->global_scope);
        }
        else
            value->is_new_obj = false;    
    }

    visitor_visit_assignement_value(variable,value);

    scope_append_variable_definition(
        scope,
        variable->assignement_variable_name,
        value
    );
    

    return value;
}