/**
 * @file visitor.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __VISITOR_H__
#define __VISITOR_H__
    
    #include "scope.h"
    #include "ast.h"
    #include "common.h"
typedef struct VISITOR_STRUCT{
    scope_t *global_scope;
    list_ast_t constants;
    bool is_for_obj;
}visitor_t;

void newVisitor(visitor_t * visitor);
void freeVisitor(visitor_t *visitor);
void freeLvisitor(void *x);

void visitor_Sweep_value(visitor_t *visitor);
void visitor_append_value(visitor_t * visitor , ast_t * value);

ast_t * visitor_visit(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_compound(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_data(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_struct_definition(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_struct(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_variable_definition(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_variable(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_function_definition(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_function_call(visitor_t * visitor,ast_t *node);

ast_t *visitor_visit_recursive_binop(visitor_t *visitor , ast_t * left , token_t *op , ast_t *right);
ast_t * visitor_visit_binop(visitor_t *visitor , ast_t *node);
ast_t * visitor_visit_unop(visitor_t * visitor , ast_t * node);

ast_t * visitor_visit_if(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_while(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_assignement(visitor_t * visitor,ast_t *node);


ast_t * visitor_visit_list_init(visitor_t * visitor,ast_t *node);

ast_t * visitor_visit_list_access(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_object_access(visitor_t * visitor,ast_t *node);

ast_t * visitor_visit_list_assignement(visitor_t * visitor,ast_t *node);

void newGb();
ast_t* visitor_visit_function(visitor_t *visitor , ast_t * func , list_ast_t * args);
ast_t* visitor_visit_method(visitor_t *visitor ,scope_t * self ,  ast_t * method , list_ast_t * args);
int  visitor_visit_code(ast_t* node);
// void freeVsisitorGB();


#endif