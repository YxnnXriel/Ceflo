#ifndef __VISITOR_H__
#define __VISITOR_H__
    
    #include "scope.h"
    #include "ast.h"

typedef struct VISITOR_STRUCT{
    scope_t *global_scope;
}visitor_t;

void newVisitor(visitor_t * visitor);

ast_t * visitor_visit(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_compound(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_data(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_variable_definition(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_variable(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_function_definition(visitor_t * visitor,ast_t *node);
ast_t * visitor_visit_function_call(visitor_t * visitor,ast_t *node);


ast_t *visitor_visit_recursive_binop(visitor_t *visitor , ast_t * left , token_t *op , ast_t *right);
ast_t * visitor_visit_binop(visitor_t *visitor , ast_t *node);



#endif