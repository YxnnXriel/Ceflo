#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "include/parser.h"
#include "include/error.h"
#include "include/common.h"



static Return_Type Rt;

static parser_t *GetCurrentParser(){
    static parser_t parser;
    return &parser;
}

static int *ScopeCount(){
    static int l;
    return &l;
}
static void ScopeReset(){
    int *l = ScopeCount();
    *l = 0;
}

static void ScopeSet(int value){
    int * l = ScopeCount();
    *l = value;
}

static void ScopePlus(){
    int *p = ScopeCount();
    *p += 1;
}

static void ScopeMinus(){
    int *p = ScopeCount();
    *p -= 1;
}

static int GetScope(){
    int * val = ScopeCount();
    return *val;
}

void newParser(parser_t *parser,lexer_t *lexer){
    parser->lexer = lexer;
    parser->current = lexer_get_next_token(parser->lexer);
    parser->prev = AS_TOKEN(TOKEN_EOF,"EOF") ;

}

int parser_eat(TokenEnum type,errorE errorType,char *fmt , ...){

    parser_t *parser = GetCurrentParser();

    va_list args;
    va_start(args,fmt);

    if(type != parser->current.type){
        Print_parser_error(
            parser->lexer->line,
            parser->lexer->colone,
            // "./Ceflo",
            parser->lexer->fileName->str,
            errorType,
            fmt,
            args
        );
        va_end(args);
        exit(1);
    }

    freeToken(&parser->prev);
    parser->prev = parser->current;
    parser->current = lexer_get_next_token(parser->lexer);
    return 1;
}


ast_t *parser_parse(lexer_t *lexer){

    parser_t *parser = GetCurrentParser();
    newParser(parser,lexer);

    __init_first_ast();
    Rt = R_FUNCTION_CALL_TYPE;
    ScopeReset();
    
    ast_t *node =  parser_parse_statments(parser,NULL);

    return node;
}

ast_t *parser_parse_statments(parser_t *parser , scope_t * scope){

    return parser_parse_compound(parser,scope);

}

ast_t *parser_parse_compound(parser_t *parser , scope_t * scope){


    ast_t * compound = newAst(AST_COMPOUND);

    compound->scope  = scope;
    compound->countScope = GetScope();
    compound->type_compound = R_FUNCTION_CALL_TYPE;

    #define COMPOUND compound->list_compound

    COMPOUND = __init__list__(sizeof(struct AST_STRUCT*));
	

    ast_t *child = parser_parse_statment(parser,scope);

    List_Append(COMPOUND,child);

    while (parser->current.type == TOKEN_SEMI || child->type != AST_NOOP){
        
        if(parser->current.type == TOKEN_SEMI){
            parser_eat(TOKEN_SEMI,SYNTAX_ERROR,"%s manque a l'appel",TOKEN_TO_STRING[parser->current.type]);
        }
        child = parser_parse_statment(parser,scope);
        List_Append(COMPOUND,child);

    }
    
    return compound;

    #undef COMPOUND
}

ast_t *parser_parse_statment(parser_t *parser , scope_t * scope){

    #define type parser->current.type

    // printf("ICI \n");
    // printf("%s  %s   %s \n",__func__,TOKEN_TO_STRING[type],parser->current.value);

    switch (type)
    {
        case TOKEN_VAR : return parser_parse_variable_definition(parser,scope);
        case TOKEN_FUNC : return parser_parse_function_definition(parser,scope);
        case TOKEN_STRUCT : return parser_parse_struct_definition(parser,scope);
        case TOKEN_ID : return parser_parse_variable(parser,scope);
        case TOKEN_RETURN : return parser_parse_return(parser,scope);
        case TOKEN_IF : return parser_parse_if(parser,scope);
        case TOKEN_WHILE : return parser_parse_while(parser,scope);
        case TOKEN_BREAK : return parser_parse_break(parser,scope);
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
        case TOKEN_CHAR:
        case TOKEN_STRING:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parser_parse_expr(parser,scope);
    }

    return newAst(AST_NOOP);

    #undef type
}

static type_ceflo GetType(char *name){
    
    if(strcmp(name,"none") == 0)
        return NONE;
    if(strcmp(name,"float")==0)
        return NUMBER;
    if(strcmp(name,"int")==0)
        return NUMBER;
    if(strcmp(name,"char")==0)
        return CHAR;
    if(strcmp(name,"str")==0)
        return STRING;
    if(strcmp(name,"bool")==0)
        return BOOL;
    if(strcmp(name,"list") == 0)
        return LIST;

    return OBJECT;
}


ast_t *parser_parse_variable_definition(parser_t *parser , scope_t * scope){

    // var k::int = l;

    ast_t *variable = newAst(AST_VARIABLE_DEFINITION);
    variable->countScope = GetScope();
    variable->scope = scope;
    parser_eat(TOKEN_VAR,VARIABLE_DEFINITION_ERROR,"definition de varaible echouer \n");

    allowChar(&variable->variable_definition_name,parser->current.value);
    parser_eat(TOKEN_ID,VARIABLE_DEFINITION_ERROR,"definition de varaible echouer \n");
    
    if(parser->current.type == TOKEN_DOUBLE_COLON){
        parser_eat(TOKEN_DOUBLE_COLON,VARIABLE_DEFINITION_ERROR,"definition de varaible echouer \n");
        variable->variable_definition_static_type = GetType(parser->current.value);
        allowChar(&variable->variable_definition_static_type_name,parser->current.value);
        parser_eat(TOKEN_ID,VARIABLE_DEFINITION_ERROR,"definition de varaible echouer \n");
    }

    if(parser->current.type == TOKEN_EQUAL){
        parser_eat(TOKEN_EQUAL,VARIABLE_DEFINITION_ERROR,"definition de varaible echouer \n");
        variable->variable_definition_value = parser_parse_expr(parser,scope);
    }else{
        variable->variable_definition_value = newAst(AST_NONE);
    }



    return variable;
}


static char * GetOp(TokenEnum type){
    switch (type){
        case TOKEN_PLUS_EQUAL: return "+=";
        case TOKEN_MINUS_EQUAL: return "-=";
        case TOKEN_MULT_EQUAL: return "*=";
        case TOKEN_DIV_EQUAL: return "/=";
    }
    return "=";
}

ast_t *parser_parse_variable(parser_t *parser , scope_t * scope){

    ast_t * variable = newAst(AST_VARIABLE);
    variable->countScope = GetScope();
    allowChar(&variable->variable_name,parser->current.value);
    parser_eat(TOKEN_ID,SYNTAX_ERROR,"ID %s inconnue \n",parser->current.value);
    // printf("[%s]\n",parser->prev.value);
    
    if(parser->current.type == TOKEN_LPARENT){
        return parser_parse_function_call(parser,scope);
    }


    if(parser->current.type == TOKEN_LBRACE){

        while (parser->current.type == TOKEN_LBRACE){
        
            ast_t * list_acces = newAst(AST_LIST_ACCESS);
            list_acces->left = variable;
            list_acces->scope = scope;
            list_acces->countScope = GetScope();
            parser_eat(TOKEN_LBRACE,SYNTAX_ERROR,"ID %s inconnue \n",parser->current.value);
            list_acces->list_ptr = parser_parse_expr(parser,scope);
            parser_eat(TOKEN_RBRACE,SYNTAX_ERROR,"ID %s inconnue \n",parser->current.value);
            variable = list_acces;
        }
        
        if(parser->current.type == TOKEN_EQUAL ||
            parser->current.type == TOKEN_PLUS_EQUAL ||
            parser->current.type == TOKEN_MINUS_EQUAL ||
            parser->current.type == TOKEN_DIV_EQUAL ||
            parser->current.type == TOKEN_MULT_EQUAL
        ){
            ast_t * list_assign = newAst(AST_LIST_ASSIGNEMENT);
            list_assign->scope = scope;
            list_assign->countScope = GetScope();
            list_assign->op = parser->current.type == TOKEN_EQUAL ? parser->current :
                AS_TOKEN(parser->current.type -1 ,GetOp(parser->current.type - 1));
            parser_eat(parser->current.type,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
            list_assign->assignement_left = variable;
            list_assign->assignement_value = parser_parse_expr(parser,scope);

            return list_assign;
        }

    }

    if(parser->current.type == TOKEN_DOT){
        parser_eat(TOKEN_DOT,SYNTAX_ERROR,"ID %s inconnue \n",parser->current.value);
        ast_t * obj_acces = newAst(AST_OBJ_ACCESS);
        obj_acces->left = variable;
        obj_acces->right = parser_parse_expr(parser,scope);

        variable = obj_acces;
    }

    if(parser->current.type == TOKEN_EQUAL){
        parser_eat(TOKEN_EQUAL,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        ast_t * assign = newAst(AST_ASSIGNEMENT);
        assign->assignement_left = variable;
        assign->assignement_value = parser_parse_expr(parser,scope);
        assign->countScope = GetScope();
        assign->scope = scope;
        return assign;     
    }

    if(
        parser->current.type == TOKEN_PLUS_EQUAL ||
        parser->current.type == TOKEN_MINUS_EQUAL ||
        parser->current.type == TOKEN_DIV_EQUAL ||
        parser->current.type == TOKEN_MULT_EQUAL
    ){
        ast_t * modifier = newAst(AST_ASSIGNEMENT);
        modifier->countScope = GetScope();
        modifier->scope = scope;
        modifier->assignement_left = variable;
        modifier->op = AS_TOKEN(parser->current.type -1 ,GetOp(parser->current.type - 1));
        parser_eat(parser->current.type,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        modifier->assignement_value = parser_parse_expr(parser,scope);
        return modifier;
    }


    variable->scope = scope;
    
    return variable;

}



ast_t *parser_parse_function_definition(parser_t *parser , scope_t * scope){

    ast_t *function = newAst(AST_FUNCTION_DEFINITION);
    newList_Ast(&function->function_defintion_args); 
    function->countScope = GetScope();

    parser_eat(TOKEN_FUNC,FUNCTION_DEFINITION_ERROR,"definition de function echouer \n");
    allowChar(&function->function_definition_name,parser->current.value);
    parser_eat(TOKEN_ID,SYNTAX_ERROR,"ID %s inconnue \n",parser->current.value);

    parser_eat(TOKEN_LPARENT,SYNTAX_ERROR," %s inconnue \n",parser->current.value);
    ScopePlus();
    
    if(parser->current.type != TOKEN_RPARENT){
        ast_t *arg = parser_parse_variable(parser,scope);
        List_Ast_Append(&function->function_defintion_args,arg);
        while (parser->current.type == TOKEN_COMA){
            parser_eat(TOKEN_COMA,SYNTAX_ERROR," %s demander \n",parser->current.value);
            arg = parser_parse_variable(parser,scope);
            List_Ast_Append(&function->function_defintion_args,arg);
        }
        
    }
    parser_eat(TOKEN_RPARENT,SYNTAX_ERROR," %s inconnue \n",parser->current.value);

    if(parser->current.type == TOKEN_ID){
        function->function_definition_return_type = GetType(parser->current.value);
        parser_eat(TOKEN_ID,SYNTAX_ERROR,"ID %s inconnue \n",parser->current.value);
    }

    parser_eat(TOKEN_LBRACKET,SYNTAX_ERROR," %s inconnue \n",parser->current.value);

    if(parser->current.type != TOKEN_RBRACKET){

        function->function_definition_body = parser_parse_statments(parser,NULL);
        function->function_definition_body->scope = NULL;

    }

    function->scope = scope;
    ScopeMinus();
    parser_eat(TOKEN_RBRACKET,SYNTAX_ERROR," %s inconnue \n",parser->current.value);

    return function;

}

ast_t *parser_parse_function_call(parser_t *parser , scope_t * scope){
    ast_t *function = newAst(AST_FUNCTION_CALL);
    function->countScope = GetScope();
    allowChar(&function->function_call_name,parser->prev.value);

    parser_eat(TOKEN_LPARENT,SYNTAX_ERROR," %s inconnue \n",parser->current.value);
    newList_Ast(&function->function_call_args);
    if(parser->current.type != TOKEN_RPARENT){
        ast_t * arg = parser_parse_expr(parser,scope);
        List_Ast_Append(&function->function_call_args,arg);

        while (parser->current.type == TOKEN_COMA){
            parser_eat(TOKEN_COMA,SYNTAX_ERROR," %s inconnue \n",parser->current.value);
            arg = parser_parse_expr(parser,scope);
            List_Ast_Append(&function->function_call_args,arg);
        }
    }

    parser_eat(TOKEN_RPARENT,SYNTAX_ERROR," %s inconnue \n",parser->current.value);

    function->scope = scope;
    return function;
}

ast_t *parser_parse_number(parser_t *parser , scope_t * scope){

    ast_t *number = newAst(
        AST_NUMBER
    );

    newValue(&number->value,NUMBER);

    number->value.as.numberValue = strtod(parser->current.value,NULL);
    parser_eat(parser->current.type,TYPE_ERROR," %s inconue \n",parser->current.value);

    number->scope = scope;
    number->countScope = GetScope();

    return number;
}
ast_t *parser_parse_char(parser_t *parser , scope_t * scope){
    ast_t * ast_char = newAst(AST_CHAR);

    if(strlen(parser->current.value) != 1){
        parser_eat(TOKEN_EOF,CHAR_ERROR," %s.lenght >= 1 \n",parser->current.value);
    }

    newValue(&ast_char->value,CHAR);
    ast_char->value.as.charValue = parser->current.value[0];
    parser_eat(TOKEN_CHAR,CHAR_ERROR," %s inconnue \n",parser->current.value);
    ast_char->countScope = GetScope();
    ast_char->scope = scope;

    return ast_char;
}
ast_t *parser_parse_bool(parser_t *parser , scope_t * scope){

    ast_t *boolean = newAst(AST_BOOL);

    newValue(&boolean->value,BOOL);
    boolean->value.as.booleanValue = parser->current.type == TOKEN_TRUE ? 1:0;
    parser_eat(parser->current.type,BOOL_ERROR," %s inconnue \n",parser->current.value);

    boolean->countScope = GetScope();
    boolean->scope = scope;

    return boolean;

}
ast_t *parser_parse_string(parser_t *parser , scope_t * scope){

    ast_t *string = newAst(AST_STRING);

    newValue(&string->value,STRING);
    string->value.as.stringValue = new_String(parser->current.value);
    parser_eat(TOKEN_STRING,STRING_ERROR," %s inconnue \n",parser->current.value);

    string->countScope = GetScope();

    string->scope = scope;

    return string;
}

static ast_t *parser_parse_none(parser_t *parser , scope_t * scope){

    ast_t *none = newAst(AST_NONE);
    parser_eat(TOKEN_NONE,STRING_ERROR," %s inconnue \n",parser->current.value);
    none->countScope = GetScope();

    none->scope = scope;

    return none;
}

ast_t *parser_parse_expr(parser_t *parser , scope_t *scope){

    ast_t * value = parser_parse_expression(parser,scope);
    value->countScope = GetScope();

    
    while (
            parser->current.type == TOKEN_OR ||
            parser->current.type == TOKEN_AND 
    ){
        ast_t *binop = newAst(AST_BINOP);
        binop->countScope = GetScope();
        binop->left = value;
        binop->scope = scope;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(parser->current.type,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        binop->right = parser_parse_expression(parser,scope);
        value = binop; 
    }
    
    return value;

}

ast_t *parser_parse_expression(parser_t *parser , scope_t *scope){
    ast_t * value = parser_parse_term(parser,scope);
    value->countScope = GetScope();
    while (
            parser->current.type == TOKEN_PLUS ||
            parser->current.type == TOKEN_MINUS ||
            parser->current.type == TOKEN_EQUAL_EQUAL ||
            parser->current.type == TOKEN_EQUAL_EQUAL ||
            parser->current.type == TOKEN_DIF 
    ){
        ast_t *binop = newAst(AST_BINOP);
        binop->countScope = GetScope();
        binop->left = value;
        binop->scope = scope;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(parser->current.type,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        binop->right = parser_parse_term(parser,scope);
        value = binop; 
    }

    while (
            parser->current.type == TOKEN_SUP ||
            parser->current.type == TOKEN_SUP_EQUAL ||
            parser->current.type == TOKEN_INF ||
            parser->current.type == TOKEN_INF_EQUAL
    ){
        ast_t *binop = newAst(AST_BINOP);
        binop->countScope = GetScope();
        binop->left = value;
        binop->scope = scope;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(parser->current.type,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        binop->right = parser_parse_expression(parser,scope);
        value = binop; 
    }
    
    
    
    return value;

}

ast_t *parser_parse_term(parser_t *parser , scope_t *scope){
    ast_t * value = parser_parse_factor(parser,scope);
    value->countScope = GetScope();

    while (
            parser->current.type == TOKEN_MULT ||
            parser->current.type == TOKEN_DIV ||
            parser->current.type == TOKEN_MODULO 
    ){
        ast_t *binop = newAst(AST_BINOP);
        binop->countScope = GetScope();
        binop->left = value;
        binop->scope = scope;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(parser->current.type,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        binop->right = parser_parse_factor(parser,scope);
        value = binop; 
    }


    return value;
}

ast_t *parser_parse_factor(parser_t *parser , scope_t *scope){

    if(parser->current.type == TOKEN_LPARENT){

        parser_eat(parser->current.type,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        ast_t * group = parser_parse_expr(parser,scope);
        group->countScope = GetScope();
        parser_eat(TOKEN_RPARENT,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        return group;
    }

    if(parser->current.type == TOKEN_NEW){
        parser_eat(TOKEN_NEW,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        parser_eat(TOKEN_ID,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        ast_t * func = parser_parse_function_call(parser,scope);
        func->type = AST_STRUCT;
        
        return func;
    }

    if(parser->current.type == TOKEN_NONE){
        return parser_parse_none(parser,scope);
    }

    if(parser->current.type == TOKEN_ID){
        return parser_parse_variable(parser,scope);
    }


    switch (parser->current.type){
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
            return parser_parse_number(parser,scope);break;
        case TOKEN_CHAR:
            return parser_parse_char(parser,scope);break;
        case TOKEN_STRING:
            return parser_parse_string(parser,scope);break;
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parser_parse_bool(parser,scope);break;
        case TOKEN_LBRACE:
            return parser_parse_list(parser,scope);
    }

    return newAst(AST_NONE);
}


ast_t *parser_parse_return(parser_t *parser , scope_t *scope){

    if(GetScope() == 0){
        parser_eat(TOKEN_SEMI,RETURN_ERROR,"return dans le main impossible \n");
    }

    ast_t *ast_return = newAst(AST_RETURN);
    ast_return->countScope = GetScope();
    ast_return->scope = scope;
    parser_eat(TOKEN_RETURN,RETURN_ERROR,"%s inconnue \n",parser->current.value);

    if(parser->current.type != TOKEN_SEMI){
        ast_return->return_expr = parser_parse_expr(parser,scope);
    }

    return ast_return;
}

ast_t *parser_parse_if(parser_t *parser , scope_t *scope){
    ast_t *ast_if = newAst(AST_IF);
    ast_if->scope = scope;
    ast_if->countScope = GetScope();

    parser_eat(TOKEN_IF,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);

    ast_if->if_expr = parser_parse_expr(parser,scope);

    parser_eat(TOKEN_LBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
    if(parser->current.type != TOKEN_RBRACKET){
        ast_if->if_body = parser_parse_statments(parser,scope);
        ast_if->if_body->type_compound = R_IF_TYPE;
    }
    parser_eat(TOKEN_RBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);

    if(parser->current.type == TOKEN_ELSE){
        parser_eat(TOKEN_ELSE,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        
        if(parser->current.type == TOKEN_IF){
            ast_if->if_other = parser_parse_if(parser,scope);
            ast_if->if_other->scope = scope;
        }else{  
            parser_eat(TOKEN_LBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
            if(parser->current.type != TOKEN_RBRACKET){
                ast_if->else_body = parser_parse_statments(parser,scope);
                ast_if->else_body->type_compound = R_IF_TYPE;
            }
            parser_eat(TOKEN_RBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        }
    }


    return ast_if;


}


ast_t *parser_parse_while(parser_t *parser , scope_t *scope){

    ast_t * ast_while = newAst(AST_WHILE);
    parser_eat(TOKEN_WHILE,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
    ast_while->countScope = GetScope();
    ast_while->while_expr = parser_parse_expr(parser,scope);

    parser_eat(TOKEN_LBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
    if(parser->current.type != TOKEN_RBRACKET){
        ast_while->while_body = parser_parse_statments(parser,scope);
        ast_while->while_body->type_compound = R_WHILE_TYPE;
    }
    parser_eat(TOKEN_RBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);


    return ast_while;
}

ast_t *parser_parse_break(parser_t *parser , scope_t *scope){
    ast_t * ast_break = newAst(AST_BREAK);
    parser_eat(TOKEN_BREAK,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
    ast_break->countScope = GetScope();
    return ast_break;
}

ast_t *parser_parse_list(parser_t *parser , scope_t *scope){
    // assert(false && printf("%s pas implementer \n",__func__));    
    
    ast_t * list = newAst(AST_LIST);
    newValue(&list->value,LIST);

    list->value.as.listValue = __init__list__(sizeof(ast_t*));

    parser_eat(TOKEN_LBRACE,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);

    if(parser->current.type != TOKEN_RBRACE){

        List_Append(list->value.as.listValue,
            parser_parse_expr(parser,scope)
        );

        while (parser->current.type == TOKEN_COMA){
            parser_eat(TOKEN_COMA,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
        
            List_Append(list->value.as.listValue,
                parser_parse_expr(parser,scope)
            );
        }
        

    }
    
    parser_eat(TOKEN_RBRACE,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);

    

    return list;
}


static bool isInClassDefinition(ast_t * item){
    switch (item->type){
        case AST_FUNCTION_DEFINITION:
        case AST_VARIABLE_DEFINITION:
        return true;
        default:
            return false;
    }
}

ast_t *parser_parse_struct_definition(parser_t *parser , scope_t *scope){

    ast_t * ast_struct = newAst(AST_STRUCT_DEFINITION);
    parser_eat(TOKEN_STRUCT,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
    allowChar(&ast_struct->struct_definition_name,parser->current.value);
    parser_eat(TOKEN_ID,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);

    ast_struct->struct_definition_item = __init__list__(sizeof(ast_t*));
    parser_eat(TOKEN_LBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);

    
    if(parser->current.type != TOKEN_RBRACKET){

        // int prevScope = GetScope();
        // ScopePlus();
        ast_t *item = parser_parse_statment(parser,scope);
        if(item->type != AST_NOOP){
            if(!isInClassDefinition(item) ){
                Print_error(
                    parser->lexer,
                    "StructDefError: %s n'est pas permis dans la definition d'une structure\n",
                    AST_TO_STRING[item->type]
                );
                exit(1);
            }
            List_Append(
                ast_struct->struct_definition_item,
                item
            );
        }
        while (parser->current.type == TOKEN_SEMI || parser->current.type != TOKEN_RBRACKET){
            if(parser->current.type == TOKEN_SEMI){
                parser_eat(TOKEN_SEMI,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);
            }
            item = parser_parse_statment(parser,scope);

            if(item->type != AST_NOOP){
                if(!isInClassDefinition(item)){
                    Print_error(
                        parser->lexer,
                        "StructDefError: %s n'est pas permis dans la definition d'une structure\n",
                        AST_TO_STRING[item->type]
                    );
                    exit(1);
                }
                List_Append(
                    ast_struct->struct_definition_item,
                    item
                );
            }
        }
        // ScopeMinus();
        // ScopeSet(prevScope);
        
        

    }
    parser_eat(TOKEN_RBRACKET,SYNTAX_ERROR,"%s inconnue \n",parser->current.value);


    return ast_struct;

}