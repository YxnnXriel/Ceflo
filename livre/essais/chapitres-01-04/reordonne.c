#include "ast.h"
#include <stdio.h>
struct R{
const char *file;
char *variable_definition_name;
struct R *variable_definition_value;
char *variable_definition_static_type_name;
char *variable_name;
char *function_definition_name;
list_t *function_definition_args;
struct R *function_definition_body;
char *function_definition_return_type_name;
char *function_call_name;
list_t *function_call_args;
double number_value;
char *string_value;
struct R *left;
struct R *right;
struct R *return_expr;
struct R *if_expr;
struct R *if_body;
struct R *if_other;
struct R *else_body;
struct R *while_expr;
struct R *while_body;
struct R *for_init;
struct R *for_condition;
struct R *for_solve;
struct R *for_body;
struct R *assignement_left;
struct R *assignement_value;
struct R *list_ptr;
char *struct_definition_name;
list_t *struct_definition_item;
list_t *list_compound;
struct R *next;
ast_type type;
int line;
int colone;
int string_lenght;
TokenEnum op;
bool is_const;
bool is_define_self;
bool bool_value;
char char_value;
bool is_dll;
bool is_import_all;
};
int main(void){printf("%zu %zu\n",sizeof(struct R),sizeof(ast_t));}
