#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/visitor.h"
#include "include/ast.h"
#include "include/error.h"



typedef struct GARBAGE_FUNCTION_STRUCT{
    scope_t *sScope;
    ast_t *sAst;
    struct GARBAGE_FUNCTION_STRUCT *next;
}Gb_t;

static Gb_t *gg;


void newGb(){
    gg = NULL;
}

static void GbPush(scope_t *scope , ast_t * node){
    Gb_t * g = calloc(1,sizeof(*g));
    g->sAst = node;
    g->sScope = scope;
    g->next = gg;
    gg = g;
}

static void GbFree(){

    while (gg != NULL){
        Gb_t * del = gg;
        gg = gg->next;
        freeScope(del->sScope);
        clearAstNode(del->sAst);
        free(del);
    }
    
}


static ast_t * bulletin_println(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    
    for (int i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);

        child = visitor_visit(visitor,child);



        switch (child->type){
            case AST_NONE : printf("none");break;
            case AST_CHAR : printf("%c",child->value.as.charValue);break;
            case AST_NUMBER : printf("%g",child->value.as.numberValue);break;
            case AST_STRING : printf("%s",child->value.as.stringValue->str);break;
            case AST_BOOL : printf("%s",child->value.as.booleanValue == 1 ? "true" : "false");break;
            default:
                printf("%p\n",child);
            break;
        }
    }
    printf("\n");

    return node; 
}

void newVisitor(visitor_t * visitor){
    visitor->global_scope = newScope();
}

static void newVisitorWithScope(visitor_t * visitor , scope_t *scope){
    visitor->global_scope = scope;
}
ast_t * visitor_visit(visitor_t * visitor,ast_t *node){


    // if(node->type == AST_FUNCTION_CALL){
    //     printf("%s \n",node->function_call_name);
    // }

    if(node == (void*)0)
        return NULL;

    // printf("[< %s >] \n",AST_TO_STRING[node->type]);


    switch (node->type){
        case AST_CHAR:
        case AST_BOOL:
        case AST_NUMBER:
        case AST_STRING:
        case AST_NONE:
        case AST_RETURN:
            return visitor_visit_data(visitor,node);break;
        case AST_COMPOUND: return visitor_visit_compound(visitor,node);break;
        case AST_VARIABLE_DEFINITION: return visitor_visit_variable_definition(visitor,node);break;
        case AST_VARIABLE: return visitor_visit_variable(visitor,node);break;
        case AST_FUNCTION_CALL: return visitor_visit_function_call(visitor,node);break;
        case AST_FUNCTION_DEFINITION: return visitor_visit_function_definition(visitor,node);break;
        case AST_BINOP: return visitor_visit_binop(visitor,node);break;

        case AST_NOOP:
            return node;
        default:
            printf("%s: pas implementer \n",AST_TO_STRING[node->type]);
            exit(1);
            break;
    }

    return node;
}

ast_t * visitor_visit_compound(visitor_t * visitor,ast_t *node){
    
    // printf("\tCompound Size : %d\n",node->list_compound->Size);

    for (int i = 0; i < node->list_compound->Size; i++){
        ast_t *child = List_Get(node->list_compound,i);
        ast_t *noeud =  visitor_visit(visitor,child);


        if(noeud ){
            if(noeud ->type == AST_RETURN){
                if(noeud ->return_expr != NULL){
                    
                    // printf("----\n");
                    ast_t * return_value = visitor_visit(visitor,noeud->return_expr); 
                    ast_t *value =  ast_copy(return_value,node->scope);  
                    // printf("<%g> \n",value->value.as.numberValue);
                    
                    value->is_return_value = true; 
                    GbFree();
                    return value;
                }
            }

            else if(noeud->is_return_value == true &&
                (noeud->countScope > node->countScope && visitor->global_scope->countScope != 0 ) &&
                node->type_compound == R_FUNCTION_CALL_TYPE
            ){
                return noeud ;
            }else{
                noeud->is_return_value =  false;
            }

        }
    }
    
    GbFree();

    return node;
}

ast_t * visitor_visit_data(visitor_t * visitor,ast_t *node){
    return node;
}

ast_t * visitor_visit_variable_definition(visitor_t * visitor,ast_t *node){

    ast_t * value = visitor_visit(visitor,node->variable_definition_value);

    value->variable_definition_static_type = node->variable_definition_static_type;
    scope_append_variable_definition(
        visitor->global_scope,
        node->variable_definition_name,
        value
    );


    return node;
}

ast_t * visitor_visit_variable(visitor_t * visitor,ast_t *node){
    ast_t * value = scope_get_variable_definition(
        visitor->global_scope,
        node->variable_name
    );

    if(value != NULL)
        return value;

    if(value == NULL){
        if(visitor->global_scope->scope_parent != NULL){
            visitor_t visi;
            newVisitorWithScope(&visi,visitor->global_scope->scope_parent);
            return visitor_visit_variable(&visi,node);
        }
    }

    if(value ==  NULL){
        Print_visitor_error(
            0,
            0,
            "./Ceflo",
            "var %s n'est pas définit \n",
            node->variable_name
        );
    }

}

ast_t * visitor_visit_function_definition(visitor_t * visitor,ast_t *node){
    scope_append_function_definition(
        visitor->global_scope,
        node->function_definition_name,
        node
    );

    // printf("[%s] == %d \n",node->function_definition_name,node->countScope);


    return node;
}

ast_t * visitor_visit_function_call(visitor_t * visitor,ast_t *node){

    // printf("ICI %s\n",node->function_call_name);

    if(strcmp("println",node->function_call_name) == 0){

        return bulletin_println(visitor,node,&node->function_call_args);
    }

    ast_t * functionDef = scope_get_function_definition(
        visitor->global_scope,
        node->function_call_name
    );
    

    if(functionDef == NULL){
        if(visitor->global_scope->scope_parent != NULL){
            visitor_t visi;
            newVisitorWithScope(&visi,visitor->global_scope->scope_parent);
            return visitor_visit_function_call(&visi,node);
        }
    }    

    if(functionDef == NULL){
        Print_visitor_error(
            0,
            0,
            "./Ceflo",
            "func %s n'est pas définit \n",
            node->function_call_name
        );
    }

    // printf("%d \n", node->countScope);
    ast_t *first = GetFirstAst();
    // printf("first save : %p\n",first);
    ResetFirstAst();
    // printf("reset first ast : %p\n",GetFirstAst());
    
    scope_t * newscope = newScope();

    ast_t *newFunction = ast_cpy_function_definition(functionDef,newscope);

    // printf("%s\n",newFunction->function_definition_name);

    for (int i = 0; i < (int) node->function_call_args.size; i++){
        char * variable_name = List_Ast_Get(&functionDef->function_defintion_args,i)->variable_name;
        ast_t * value = List_Ast_Get(&node->function_call_args,i);
        value = visitor_visit(visitor,value);

        scope_append_variable_definition(
            newscope,
            variable_name,
            value
        );
    }


    visitor_t visit;
    visit.global_scope = newscope;
    visit.global_scope->countScope = newFunction->function_definition_body->countScope; 
    
    if(visitor->global_scope->countScope == 0){
        // on etait dans le main
        // on peut copier les fonction qui sont a 0
        visit.global_scope->scope_parent = NULL;

        scope_cpy_with_scope(
            visit.global_scope,
            visitor->global_scope
        );

        // printf("---------------\n");

    }else{
        // on est dans une fonction donc on copie toutes les valeurs (fonction , variable) inferieur
        // je parcours toutes les variables et fonction , des que tombe sur 0 j'arrete
        visit.global_scope->scope_parent = visitor->global_scope;
    }

    // printf("%d \n", newFunction->function_definition_body->countScope);

    // printf("VISIT : function  %s \n",newFunction->function_definition_name);

    ast_t *returnValue =  visitor_visit(&visit,newFunction->function_definition_body);   
    // freeScope(newscope);

    // printf("end first ast : %p\n",GetFirstAst());
    // clearAst(GetFirstAst());
        // printf("ICI ICI \n");
    GbPush(newscope,GetFirstAst());

    ModifFirstAst(first);   
    // printf("modif first ast : %p\n",GetFirstAst());


    // printf("___________________\n");
    return returnValue;
}

ast_t *visitor_visit_recursive_binop(visitor_t *visitor , ast_t * left , token_t *op , ast_t *right){

    #define BINOP(l,o,r)    l(left->value) o r(right->value)

    ast_t * value = NULL;
  
 switch (left->type){

        case AST_CHAR:
            value = newAst(AST_CHAR);
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
                        return value;
                        break;


                    case AST_STRING:
                        value->type = AST_STRING;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.stringValue = String_cat("%c%s",left->value.as.charValue,right->value.as.stringValue->str);break;
                        }
                        return value;
                        break;
            }
            break;





        case AST_NUMBER:
            value = newAst(AST_NUMBER);
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
                        return value;
                        break;

                    case AST_STRING:
                        value->type = AST_STRING;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.stringValue = String_cat("%d%s",left->value.as.numberValue,right->value.as.stringValue->str);break;
                        }
                        return value;
                        break;
            }
            break;

        case AST_BOOL:
            value = newAst(AST_BOOL);
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
                        return value;
                        break;

                    case AST_STRING:
                        value->type = AST_STRING;
                        switch (op->type){
                            case TOKEN_PLUS : value->value.as.stringValue = String_cat("%s%s",left->value.as.booleanValue ? "true":"false",right->value.as.stringValue->str);break;
                        }
                        return value;
                        break;
            }
            break;

        case AST_STRING:
            value = newAst(AST_STRING);
            switch (right->type){
                    case AST_NUMBER:
                        switch (op->type){
                            case TOKEN_PLUS :value->value.as.stringValue = String_cat("%s%d",left->value.as.stringValue->str,right->value.as.numberValue);break;
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
                        return value;
                        break;
            }
            break;

            case AST_NONE:
                value = newAst(AST_BOOL);;
                switch (op->type){
                    case TOKEN_AND : value->value.as.booleanValue = left->type && right->type;break;
                    case TOKEN_EQUAL_EQUAL : value->value.as.booleanValue = left->type == right->type;break;
                    case TOKEN_DIF : value->value.as.booleanValue = left->type != right->type;break;
                    case TOKEN_OR : value->value.as.booleanValue = left->type || right->type;break;

                }
                return value;
            break;

    }
    return value;
}

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

    return value;
}