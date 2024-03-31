/**
 * @file parser.c
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
#include <assert.h>
#include <stdarg.h>
#include "include/parser.h"
#include "include/error.h"
#include "include/common.h"
#include "include/clear.h"
#include "include/debug.h"



#define NEW_OBJECT(type,parser) newAstWithLine(type,parser->lexer->line,parser->lexer->colone)
#define CURRENT(parser) parser->current
#define PREV(parser) parser->prev
#define LINE(parser) parser->lexer->line
#define COLONE(parser) parser->lexer->colone
#define TYPE(parser) parser->current.type
#define VALUE(parser) parser->current.value

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

static bool isInClassDefinition(ast_t * item){
    switch (item->type){
        case AST_FUNCTION_DEFINITION:
        case AST_VARIABLE_DEFINITION:
            item->is_for_obj = true;
        return true;
        default:
            return false;
    }
}

// pour voir si les paramètre on seulement un seul pointeur self ou non
static bool isSelfOneMethodDefinition(ast_t * item){
    if(item->type != AST_FUNCTION_DEFINITION)
        return true;

    int p = 0;

    for (int i = 0; i < (int)item->function_defintion_args.size; i++){
        ast_t * args = List_Ast_Get(&item->function_defintion_args,i);
        if(strcmp("self",args->variable_name) == 0){
            if(p == 0)
                p = 1;
            else
                return false;
        }
    }

    return true;
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





/**
 * @brief init parser
 * 
 * @param parser 
 * @param lexer 
 */
void newParser(parser_t *parser,lexer_t *lexer){
    parser->lexer = lexer;
    parser->current = lexer_get_next_token(parser->lexer);
    parser->prev = (token_t){.type = TOKEN_NONE,.value = NULL};
}



/**
 * @brief panic for parser
 * 
 * @param parser 
 */
void parser_error(parser_t * parser ){    
    freeToken(&parser->prev);
    freeToken(&parser->current);
    lexerFree(parser->lexer);
    clearAst();
    exit(1);
}


/**
 * @brief clear parser if all its ok
 * 
 * @param parser 
 */
void freeParser(parser_t * parser){
    freeToken(&parser->prev);
    freeToken(&parser->current);
    lexerFree(parser->lexer);
}


/**
 * @brief this function is like an incrementation to get next elements 
 * 
 * @param type 
 * @param parser 
 * @param fmt 
 * @param ... 
 * @return int 
 */
int parser_eat(TokenEnum type, parser_t * parser, char *fmt , ...){
    va_list args;
    va_start(args,fmt);

    if(TYPE(parser) != type){
        printf("%s:%d:%d: ",parser->lexer->fileName->str,parser->lexer->line,parser->lexer->colone);
        vprintf(fmt,args);
        va_end(args);
        parser_error(parser);
        return 1;
    }else{
        freeToken(&parser->prev);
        parser->prev = parser->current;
        parser->current = lexer_get_next_token(parser->lexer);
        va_end(args);
        return 0;
    }

}



/**
 * @brief here we start the parser
 * 
 * @param lexer 
 * @return ast_t* 
 */
ast_t *parser_parse(lexer_t *lexer){
    parser_t parser;
    __init_first_ast(); // la tête de l'ast
    newParser(&parser,lexer);
    ast_t *node = parser_parse_statments(&parser);
    freeParser(&parser);
    return node;
}

/**
 * @brief here we parse one statement, it's for lexerString
 * 
 * @param lexer 
 * @return ast_t* 
 */
ast_t *parser_parse_one(lexer_t *lexer){
    parser_t parser;
    newParser(&parser,lexer);
    ast_t *node = parser_parse_statment(&parser);
    freeParser(&parser);
    return node;
}


/**
 * @brief now , parser parse something like block or body 
 * 
 * @param parser 
 * @return ast_t* 
 */
ast_t *parser_parse_statments(parser_t *parser){

    if(TYPE(parser) == TOKEN_LBRACKET ){
        parser_eat(TYPE(parser),parser,"");
        ast_t * block = parser_parse_compound(parser);

        if(TYPE(parser) != TOKEN_RBRACKET){
            parser_eat(TOKEN_RBRACKET,parser,"%s: unexpected token %s\n -> [ %s ]\n",
                "unexpected",
                TOKEN_TO_STRING[CURRENT(parser).type],
                lexer_get_line_for_file(parser->lexer,LINE(parser))
            );
        }
        // Parse statements until a closing bracket is found

        return block;
    }else{
        return parser_parse_compound(parser);
    }

}

/**
 * @brief parse body 
 * 
 * @param parser 
 * @return ast_t* 
 */
ast_t *parser_parse_compound(parser_t *parser){
    // is Block of something
    // TODO: ajouter un champ NAME pour chaque block si le block est une fonction
    ast_t * block = NEW_OBJECT(AST_COMPOUND,parser);
    block->countScope = GetScope();

    block->list_compound = __init__list__(sizeof(struct AST_STRUCT*));

    ast_t * child = parser_parse_statment(parser);
    List_Append(block->list_compound,child);

    while (TYPE(parser) == TOKEN_SEMI || child->type != AST_NOOP){
        if(TYPE(parser) == TOKEN_SEMI){
            parser_eat(TOKEN_SEMI,parser,NULL);
        }
        child = parser_parse_statment(parser);
        List_Append(block->list_compound,child);
    }

    return block;
}



/**
 * @brief parse declaration , expression , ...
 * 
 * @param parser 
 * @return ast_t* 
 */
ast_t *parser_parse_statment(parser_t *parser){



    switch(TYPE(parser)){
        case TOKEN_VAR : return parser_parse_variable_definition(parser);
        case TOKEN_CONST : return parser_parse_variable_definition(parser);
        case TOKEN_FUNC : return parser_parse_function_definition(parser);
        case TOKEN_STRUCT : return parser_parse_struct_definition(parser);
        case TOKEN_RETURN : return parser_parse_return(parser);
        case TOKEN_IF : return parser_parse_if(parser);
        case TOKEN_WHILE : return parser_parse_while(parser);
        case TOKEN_FOR : return parser_parse_for(parser);
        case TOKEN_BREAK : return parser_parse_break(parser);
        case TOKEN_ID :
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
        case TOKEN_CHAR:
        case TOKEN_STRING:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parser_parse_expr(parser);
        default:
            return newAst(AST_NOOP);break;

    }

    return newAst(AST_NOOP);
}



/**
 * @brief we start to parse variable definition
 * 
 * @param parser 
 * @return ast_t* 
 */
ast_t *parser_parse_variable_definition(parser_t *parser){

    ast_t * var = NEW_OBJECT(AST_VARIABLE_DEFINITION,parser);

    var->countScope = GetScope();
    
    if(TYPE(parser) == TOKEN_VAR){
        parser_eat(TOKEN_VAR,parser,NULL);
    }


    parser_eat(TOKEN_ID,parser,"%s : declaration variable with name `%s` is not valid\n",
        ERROR_TO_STRING[VARIABLE_DEFINITION_ERROR],
        VALUE(parser)
    );

    allowChar(&var->variable_definition_name,parser->prev.value);


    if(TYPE(parser) == TOKEN_DOUBLE_COLON){
        parser_eat(TYPE(parser),parser,"");    
        int get_type = type_ceflo_convert_to_type(VALUE(parser));
        
        if(get_type == -2){
            parser_eat(TOKEN_EOF,parser,"%s : we cannot use 'none' to define type variable\n",
                ERROR_TO_STRING[VARIABLE_DEFINITION_ERROR],
                VALUE(parser)
            );
        }

        var->variable_definition_static_type = get_type;
        allowChar(&var->variable_definition_static_type_name,parser->current.value);

        parser_eat(TOKEN_ID,parser,"");
    }

    if(TYPE(parser) == TOKEN_EQUAL){
        parser_eat(TOKEN_EQUAL,parser,NULL);
        ast_t * expr = parser_parse_expr(parser);
        var->variable_definition_value = expr;

    }else{
        var->variable_definition_value = NEW_OBJECT(AST_NONE,parser);
    }

    if(TYPE(parser) == TOKEN_COMA){
        parser_eat(TOKEN_COMA,parser,NULL);
        var->left = parser_parse_variable_definition(parser);
    }

    return var;
}



ast_t *parser_parse_dot_access(parser_t * parser){
    ast_t * variable = NEW_OBJECT(AST_VARIABLE,parser);
    variable->countScope = GetScope();

    if(type_ceflo_convert_to_type(VALUE(parser)) != -1){
        parser_eat(TOKEN_EOF,parser,"%s : `%s` name id or variable cannot be a type",
            ERROR_TO_STRING[SYNTAX_ERROR],
            VALUE(parser)
       );        
    }

    parser_eat(TOKEN_ID,parser,"%s : `%s` is not valid for referencies something\n",
        ERROR_TO_STRING[SYNTAX_ERROR],
        VALUE(parser)
    );
    allowChar(&variable->variable_name,parser->prev.value);

    
    if(parser->current.type == TOKEN_LBRACE){
        while (parser->current.type == TOKEN_LBRACE){
            ast_t * list_acces = NEW_OBJECT(AST_LIST_ACCESS,parser);
            list_acces->left = variable;
            list_acces->countScope = GetScope();
            parser_eat(TOKEN_LBRACE,parser,NULL);
            list_acces->list_ptr = parser_parse_expr(parser);
            parser_eat(TOKEN_RBRACE,parser,"%s : we have forgot to close `%s` here\n",
                ERROR_TO_STRING[SYNTAX_ERROR],
                TOKEN_TO_STRING[TOKEN_LBRACE]
            );
            variable = list_acces;
        }
    }

    if(TYPE(parser) == TOKEN_LPARENT){
        return parser_parse_function_call(parser);
    }

    if(TYPE(parser) == TOKEN_DOT){
        return parser_parse_obj_access(parser,variable); 
    }


    return variable;
}


ast_t * parser_parse_obj_access(parser_t * parser , ast_t * prev_variable){

        parser_eat(TYPE(parser),parser,NULL);
        ast_t * obj_acces = NEW_OBJECT(AST_OBJ_ACCESS,parser);
        obj_acces->left = prev_variable;
        obj_acces->right = parser_parse_dot_access(parser);
        obj_acces->countScope = GetScope();

        return obj_acces;
}


/**
 * @brief here you can have variable access like variable, variable[], variable.property etc
 * 
 * @param parser 
 * @return ast_t* 
 */
ast_t *parser_parse_variable(parser_t *parser){

    ast_t * variable = NEW_OBJECT(AST_VARIABLE,parser);
    variable->countScope = GetScope();

    if(type_ceflo_convert_to_type(VALUE(parser)) != -1){
        parser_eat(TOKEN_EOF,parser,"%s : `%s` name id or variable cannot be a type",
            ERROR_TO_STRING[SYNTAX_ERROR],
            VALUE(parser)
       );        
    }

    parser_eat(TOKEN_ID,parser,"%s : `%s` is not valid for referencies something\n",
        ERROR_TO_STRING[SYNTAX_ERROR],
        VALUE(parser)
    );
    allowChar(&variable->variable_name,parser->prev.value);

    // current token :
    int is_lbrace_and_function_call = 0;

    if(parser->current.type == TOKEN_DOUBLE_COLON){

        // syntaxe definition type static
        parser_eat(TYPE(parser),parser,"");    
        if(TYPE(parser) == TOKEN_ID){
            int get_type = type_ceflo_convert_to_type(parser->current.value);
            variable->variable_definition_static_type = get_type == -2 ? NONE : get_type;
            allowChar(&variable->variable_definition_static_type_name,VALUE(parser));
            parser_eat(TYPE(parser),parser,NULL);

        }else{
            parser_eat(TOKEN_EOF,parser,"%s : variable type name must be expected\n",ERROR_TO_STRING[SYNTAX_ERROR]);
        }

        return variable;
    }

    if(parser->current.type == TOKEN_LBRACE){
        //...., maybe the declaration is a list : access or assignement
        is_lbrace_and_function_call = 1;
        while (parser->current.type == TOKEN_LBRACE){
        
            ast_t * list_acces = NEW_OBJECT(AST_LIST_ACCESS,parser);
            list_acces->left = variable;
            list_acces->countScope = GetScope();
            parser_eat(TOKEN_LBRACE,parser,NULL);
            list_acces->list_ptr = parser_parse_expr(parser);
            parser_eat(TOKEN_RBRACE,parser,"%s : we have forgot to close `%s` here\n",
                ERROR_TO_STRING[SYNTAX_ERROR],
                TOKEN_TO_STRING[TOKEN_LBRACE]
            );
            variable = list_acces;
        }
        
        if(parser->current.type == TOKEN_EQUAL ||
            parser->current.type == TOKEN_PLUS_EQUAL ||
            parser->current.type == TOKEN_MINUS_EQUAL ||
            parser->current.type == TOKEN_DIV_EQUAL ||
            parser->current.type == TOKEN_MULT_EQUAL
        ){
            ast_t * list_assign = NEW_OBJECT(AST_LIST_ASSIGNEMENT,parser);
            list_assign->countScope = GetScope();
            if(TYPE(parser) == TOKEN_EQUAL ){
                list_assign->op = AS_TOKEN(TYPE(parser),VALUE(parser));
            }else{

                list_assign->op = AS_TOKEN(parser->current.type -1 ,GetOp(parser->current.type - 1));
            }
            parser_eat(TYPE(parser),parser,NULL);
            list_assign->assignement_left = variable;
            list_assign->assignement_value = parser_parse_expr(parser);

            return list_assign;
        }

    }

    if(parser->current.type == TOKEN_LPARENT){
        //.... maybe the declaration is a function call 
        if(is_lbrace_and_function_call){
            return parser_parse_function_call_With_Name(variable,parser);
        }
        else{

            return parser_parse_function_call(parser);
        }
    }

    if(parser->current.type == TOKEN_DOT){
        //...., maybe the declaration is a struct , access or assignement
        
        // parser_eat(TYPE(parser),parser,NULL);
        // ast_t * obj_acces = NEW_OBJECT(AST_OBJ_ACCESS,parser);
        // obj_acces->left = variable;
        // obj_acces->right = parser_parse_expr(parser);
        // obj_acces->countScope = GetScope();

        // variable = obj_acces;

        variable = parser_parse_obj_access(parser,variable);
    
    }


    if(parser->current.type == TOKEN_EQUAL){
        //...., maybe the declaration is a assignement 
        parser_eat(TYPE(parser),parser,NULL);
        ast_t * assign = NEW_OBJECT(AST_ASSIGNEMENT,parser);
        assign->assignement_left = variable;
        assign->assignement_value = parser_parse_expr(parser);
        assign->countScope = GetScope();
        return assign;     
    }

    if(
        parser->current.type == TOKEN_PLUS_EQUAL ||
        parser->current.type == TOKEN_MINUS_EQUAL ||
        parser->current.type == TOKEN_DIV_EQUAL ||
        parser->current.type == TOKEN_MULT_EQUAL
    ){

        //...., maybe the declaration is a modifier variable

        ast_t * modifier = NEW_OBJECT(AST_ASSIGNEMENT,parser);
        modifier->countScope = GetScope();
        modifier->assignement_left = variable;
        modifier->op = parser->current;
        modifier->op = AS_TOKEN(parser->current.type -1 ,GetOp(parser->current.type - 1));
        parser_eat(TYPE(parser),parser,NULL);
        modifier->assignement_value = parser_parse_expr(parser);
        return modifier;
    
    }

    //...., maybe the declaration is a acces variable


    
    return variable;
}



static void get_function_definition_args(parser_t *parser, ast_t *function){
    bool addr = false;
    if(TYPE(parser) == TOKEN_ADDR){
        addr = true;
        parser_eat(TYPE(parser),parser,NULL);
    }

    ast_t *arg = parser_parse_variable(parser);

    if(strcmp(arg->variable_name,"self") == 0){
        function->is_define_self = true;
    }else{
        List_Ast_Append(&function->function_defintion_args,arg);
    }
    arg->is_arg_ptr = addr;
    addr = false;


    while (parser->current.type == TOKEN_COMA){
        parser_eat(TYPE(parser),parser,NULL);
        if(TYPE(parser) == TOKEN_ADDR){
            addr = true;
            parser_eat(TYPE(parser),parser,NULL);
         }
        arg = parser_parse_variable(parser);

        if(strcmp(arg->variable_name,"self") == 0){
            if(function->is_define_self == false){
                function->is_define_self = true;
                continue;
            }else{
                parser_eat(TOKEN_EOF,parser,"%s : multiple self definition for this Function \n",ERROR_TO_STRING[FUNCTION_DEFINITION_ERROR]);
            }
        }
        arg->is_arg_ptr = addr;
        addr = false;
        List_Ast_Append(&function->function_defintion_args,arg);
    }
}

/**
 * @brief Parses a function definition from the parser context
 * 
 * @param parser 
 * @return ast_t* 
 */
ast_t *parser_parse_function_definition(parser_t *parser){

    // introduce `call` for method of obj value

    ast_t *function = newAstWithLine(AST_FUNCTION_DEFINITION,parser->lexer->line,parser->lexer->colone);
    function->countScope = GetScope();
    newList_Ast(&function->function_defintion_args); 
    parser_eat(TYPE(parser),parser,NULL);

    if(TYPE(parser) == TOKEN_ID){
        if(PREV(parser).type == TOKEN_LFUNC){
            parser_eat(TOKEN_EOF,parser,"%s : dont put name for definition lfunc\n",ERROR_TO_STRING[SYNTAX_ERROR]);
        }
        allowChar(&function->function_definition_name,VALUE(parser));
        parser_eat(TYPE(parser),parser,NULL);
    }else{
        if(PREV(parser).type != TOKEN_LFUNC){
            parser_eat(TOKEN_EOF,parser,"%s : function name expected\n",ERROR_TO_STRING[FUNCTION_DEFINITION_ERROR]);
        }
    }

    bool isarrow = false;

    if(TYPE(parser) == TOKEN_ARROW){
        isarrow = true;
        parser_eat(TYPE(parser),parser,NULL);
    }


    if(TYPE(parser) != TOKEN_LBRACKET && TYPE(parser) != TOKEN_COLON){
        if(isarrow){
            get_function_definition_args(parser,function);
        }else{
            parser_eat(TOKEN_LPARENT,parser,"%s : `%s` :  you are missing `%s`\n",  
                ERROR_TO_STRING[FUNCTION_DEFINITION_ERROR],
                function->function_definition_name,
                TOKEN_TO_STRING[TOKEN_LPARENT]
            );

            if(TYPE(parser) != TOKEN_RPARENT )
                get_function_definition_args(parser,function);
            
            parser_eat(TOKEN_RPARENT,parser,"%s : `%s` :  you are missing `%s`\n",  
                ERROR_TO_STRING[FUNCTION_DEFINITION_ERROR],
                function->function_definition_name,
                TOKEN_TO_STRING[TOKEN_RPARENT]
            );
        }
    }


    if(TYPE(parser) == TOKEN_COLON){
        parser_eat(TYPE(parser),parser,NULL);
        int get_type = type_ceflo_convert_to_type(parser->current.value);
  
        function->function_definition_return_type = get_type == -1 ? OBJECT : get_type;
        if(get_type == -2){
            function->function_definition_return_type = NONE;
        }
        parser_eat(TOKEN_ID,parser,"ID %s inconnue \n",parser->current.value);
        allowChar(&function->function_definition_return_type_name,parser->prev.value);
    }


    ScopePlus();


    // TODO : make inline function
    
    parser_eat(TOKEN_LBRACKET,parser,"%s : unexpected `%s` for begin bodyfunction\n",
        ERROR_TO_STRING[FUNCTION_DEFINITION_ERROR],
        TOKEN_TO_STRING[TOKEN_LBRACKET]
    );


    if(parser->current.type != TOKEN_RBRACKET){
        function->function_definition_body = parser_parse_statments(parser);
    }

    ScopeMinus();
    parser_eat(TOKEN_RBRACKET,parser,"%s : unexpected `%s` for end bodyfunction\n",
        ERROR_TO_STRING[FUNCTION_DEFINITION_ERROR],
        TOKEN_TO_STRING[TOKEN_RBRACKET]
    );

    return function;

}

/**
 * @brief je parse une fonction qui est en faite une fonction dans un tableau , exemple : table[0]()
 * 
 * @param prev 
 * @param parser 
 * @return ast_t* 
 */
ast_t *parser_parse_function_call_With_Name(ast_t * prev,parser_t *parser){

    ast_t *function = NEW_OBJECT(AST_FUNCTION_CALL, parser);

    function->countScope = GetScope();
    function->left = prev;
    parser_eat(TOKEN_LPARENT, parser,"unexpected `(` here");
    newList_Ast(&function->function_call_args);
    if(parser->current.type != TOKEN_RPARENT){
        ast_t * arg = parser_parse_expr(parser);
        List_Ast_Append(&function->function_call_args,arg);

        while (parser->current.type == TOKEN_COMA){
            parser_eat(TOKEN_LPARENT, parser,"unexpected `,` here");
            
            arg = parser_parse_expr(parser);
            List_Ast_Append(&function->function_call_args,arg);
        }
    }

    parser_eat(TOKEN_LPARENT, parser,"unexpected `)` here");

    return function;
}


ast_t *parser_parse_function_call(parser_t *parser){
    ast_t *function = NEW_OBJECT(AST_FUNCTION_CALL,parser);
    function->countScope = GetScope();

    // TODO: transform () in statement , for ViL interpreter

    allowChar(&function->function_call_name,parser->prev.value);

    parser_eat(TYPE(parser),parser,NULL);
    newList_Ast(&function->function_call_args);
    if(parser->current.type != TOKEN_RPARENT){
        ast_t * arg = parser_parse_expr(parser);
        List_Ast_Append(&function->function_call_args,arg);

        while (parser->current.type == TOKEN_COMA){
            parser_eat(TOKEN_COMA, parser,"unexpected `,` here");
            arg = parser_parse_expr(parser);
            List_Ast_Append(&function->function_call_args,arg);
        }
    }
    

    parser_eat(TOKEN_RPARENT,parser,"%s : unexpected `%s` for end funcCall\n",
        ERROR_TO_STRING[FUNCTION_CALL_ERROR],
        TOKEN_TO_STRING[TOKEN_RPARENT]
    );

    return function;
}

ast_t *parser_parse_lfunc(parser_t *parser){
    return NULL;
}

ast_t *parser_parse_number(parser_t *parser){
    ast_t *number = NEW_OBJECT(AST_NUMBER,parser);

    newValue(&number->value,NUMBER);

    number->value.as.numberValue = strtod(parser->current.value,NULL);

    number->countScope = GetScope();
    parser_eat(TYPE(parser),parser,NULL);

    return number;
}

ast_t *parser_parse_char(parser_t *parser){
    ast_t * ast_char = NEW_OBJECT(AST_CHAR,parser);
    
    if(strlen(parser->current.value) > 1){
        parser_eat(TOKEN_EOF,parser,"%s: %s.lenght > 1 \n",
            ERROR_TO_STRING[CHAR_ERROR],
            VALUE(parser)
        );
    }

    newValue(&ast_char->value,CHAR);
    ast_char->value.as.charValue = parser->current.value[0];
    ast_char->countScope = GetScope();
    parser_eat(TYPE(parser),parser,NULL);


    return ast_char;
}

ast_t *parser_parse_bool(parser_t *parser){
    
    ast_t *boolean = NEW_OBJECT(AST_BOOL,parser);

    newValue(&boolean->value,BOOL);
    boolean->value.as.booleanValue = parser->current.type == TOKEN_TRUE ? 1:0;
    parser_eat(TYPE(parser),parser,NULL);

    boolean->countScope = GetScope();

    return boolean;
}

ast_t *parser_parse_string(parser_t *parser){
    ast_t *string = NEW_OBJECT(AST_STRING,parser);

    newValue(&string->value,STRING);
    string->value.as.stringValue = new_String(parser->current.value);
    parser_eat(TYPE(parser),parser,NULL);

    string->countScope = GetScope();


    return string;
}


/**
 * @brief for none 
 * 
 * @param parser 
 * @return ast_t* 
 */
static ast_t *parser_parse_none(parser_t *parser){

    ast_t *none = NEW_OBJECT(AST_STRING,parser);
    parser_eat(TYPE(parser),parser,NULL);

    none->countScope = GetScope();


    return none;
}


ast_t *parser_parse_expr(parser_t *parser){
    
    ast_t * value = parser_parse_expression(parser);
    value->countScope = GetScope();

    
    while (
            parser->current.type == TOKEN_OR ||
            parser->current.type == TOKEN_AND 
    ){
        ast_t *binop = NEW_OBJECT(AST_BINOP,parser);
        binop->countScope = GetScope();
        binop->left = value;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(TYPE(parser),parser,NULL);
        binop->right = parser_parse_expression(parser);
        value = binop; 
    }
    
    return value;

}

ast_t *parser_parse_expression(parser_t *parser){
    
    ast_t * value = parser_parse_term(parser);
    value->countScope = GetScope();
    while (
            parser->current.type == TOKEN_PLUS ||
            parser->current.type == TOKEN_MINUS ||
            parser->current.type == TOKEN_EQUAL_EQUAL ||
            parser->current.type == TOKEN_EQUAL_EQUAL ||
            parser->current.type == TOKEN_DIF 
    ){
        ast_t *binop = NEW_OBJECT(AST_BINOP,parser);
        binop->countScope = GetScope();
        binop->left = value;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(TYPE(parser),parser,NULL);       
        binop->right = parser_parse_term(parser);
        value = binop; 
    }

    while (
            parser->current.type == TOKEN_SUP ||
            parser->current.type == TOKEN_SUP_EQUAL ||
            parser->current.type == TOKEN_INF ||
            parser->current.type == TOKEN_INF_EQUAL
    ){
        ast_t *binop = NEW_OBJECT(AST_BINOP,parser);
        binop->countScope = GetScope();
        binop->left = value;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(TYPE(parser),parser,NULL);
        
        binop->right = parser_parse_expression(parser);
        value = binop; 
    }
    
    
    
    return value;
}

ast_t *parser_parse_term(parser_t *parser){
    ast_t * value = parser_parse_factor(parser);
    value->countScope = GetScope();

    while (
            parser->current.type == TOKEN_MULT ||
            parser->current.type == TOKEN_DIV ||
            parser->current.type == TOKEN_MODULO 
    ){
        ast_t *binop = NEW_OBJECT(AST_BINOP,parser);
        binop->countScope = GetScope();
        binop->left = value;
        binop->op = AS_TOKEN(parser->current.type,parser->current.value);
        parser_eat(TYPE(parser),parser,NULL);        
        binop->right = parser_parse_factor(parser);
        value = binop; 
    }


    return value;
}

ast_t *parser_parse_factor(parser_t *parser){

    if(parser->current.type == TOKEN_MINUS){
        ast_t * unop = NEW_OBJECT(AST_UNOP,parser);
        unop->op = parser->current;
        parser_eat(TYPE(parser),parser,NULL);
        unop->right = parser_parse_term(parser);
        return unop;
    }


    if(parser->current.type == TOKEN_LPARENT){

        parser_eat(TYPE(parser),parser,NULL);

        ast_t * group = parser_parse_expr(parser);
        group->countScope = GetScope();
        parser_eat(TOKEN_RPARENT, parser,"unexpected `)` here");
        return group;
    }

    if(parser->current.type == TOKEN_NEW){
        parser_eat(TYPE(parser),parser,NULL);
        parser_eat(TOKEN_ID,parser,"%s inconnue \n",ERROR_TO_STRING[SYNTAX_ERROR],parser->current.value);
        ast_t * func = parser_parse_function_call(parser);
        func->type = AST_STRUCT;
        
        return func;
    }

    if(TYPE(parser) == TOKEN_LFUNC){
        // assert(false && printf("Pas encore crée\n"));
        ast_t * lfunc = NEW_OBJECT(AST_LFUNC,parser);
        lfunc->value.as.lfuncValue = parser_parse_function_definition(parser);
        lfunc->value.type = LFUNC;
        return lfunc;
    }



    if(parser->current.type == TOKEN_AT_SIGN){
        parser_eat(TYPE(parser),parser,NULL);
        parser_eat(TYPE(parser),parser,NULL);

        ast_t *func_typer =  parser_parse_function_call(parser);
        func_typer->is_func_typer = true;
        return func_typer; 
    }

    if(parser->current.type == TOKEN_NONE){
        return parser_parse_none(parser);
    }

    if(parser->current.type == TOKEN_LFUNC){
        return parser_parse_lfunc(parser);
    }

    if(parser->current.type == TOKEN_ID){
        return parser_parse_variable(parser);
    }


    ast_t * value = (void*)0;

    switch (parser->current.type){
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
            value = parser_parse_number(parser);
            break;
        case TOKEN_CHAR:
            value = parser_parse_char(parser);break;
        case TOKEN_STRING:
            value = parser_parse_string(parser);break;
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            value = parser_parse_bool(parser);break;
        case TOKEN_LBRACE:
            value = parser_parse_list(parser);
            break;
    }

    if(value != NULL && parser->current.type == TOKEN_DOT){
        while (parser->current.type == TOKEN_DOT){
            parser_eat(TOKEN_DOT,parser,NULL);
            ast_t * obj_acces = NEW_OBJECT(AST_OBJ_ACCESS,parser);
            obj_acces->left = value;
            obj_acces->right = parser_parse_expr(parser);
            value = obj_acces;
        }
    }

    return value != NULL ? value : newAst(AST_NONE);
}

ast_t *parser_parse_return(parser_t *parser){

    if(GetScope() == 0){
        parser_eat(TOKEN_SEMI,parser,"%s : return dans le main impossible",ERROR_TO_STRING[RETURN_ERROR]);
    }

    ast_t *ast_return = NEW_OBJECT(AST_RETURN,parser);
    ast_return->countScope = GetScope();

    parser_eat(TYPE(parser),parser,NULL);

    if(parser->current.type != TOKEN_SEMI){
        ast_return->return_expr = parser_parse_expr(parser);
    }else{
        ast_return->return_expr = newAst(AST_NONE);
    }

    return ast_return;
}

ast_t *parser_parse_if(parser_t *parser){
    
    ast_t *ast_if = NEW_OBJECT(AST_IF,parser);
    ast_if->countScope = GetScope();

    parser_eat(TYPE(parser),parser,NULL);

    ast_if->if_expr = parser_parse_expr(parser);
    
    parser_eat(TOKEN_LBRACKET,parser,"%s : you have forgotten block for if `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);
    if(parser->current.type != TOKEN_RBRACKET){
        ast_if->if_body = parser_parse_statments(parser);
    }
    parser_eat(TOKEN_RBRACKET,parser,"%s : you have forgotten endblock for if `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);

    if(parser->current.type == TOKEN_ELSE){
        parser_eat(TYPE(parser),parser,NULL);
        

        if(parser->current.type == TOKEN_IF){
            ast_if->if_other = parser_parse_if(parser);
        }else{
            parser_eat(TOKEN_LBRACKET,parser,"%s : you have forgotten block for else `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);
            if(parser->current.type != TOKEN_RBRACKET){
                ast_if->else_body = parser_parse_statments(parser);
            }
            parser_eat(TOKEN_RBRACKET,parser,"%s : you have forgotten endblock for else `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);
        }
    }


    return ast_if;
}

ast_t *parser_parse_while(parser_t *parser){
    ast_t * ast_while = NEW_OBJECT(AST_WHILE,parser);
    parser_eat(TYPE(parser),parser,NULL);
    ast_while->countScope = GetScope();
    // printf("while is : %d\n",ast_while->countScope);
    ast_while->while_expr = parser_parse_expr(parser);

    parser_eat(TOKEN_LBRACKET,parser,"%s : you have forgotten block for while `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);
    if(parser->current.type != TOKEN_RBRACKET){
        ast_while->while_body = parser_parse_statments(parser);
        ast_while->while_body->type_compound = R_WHILE_TYPE;
    }
    parser_eat(TOKEN_RBRACKET,parser,"%s : you have forgotten endblock for while `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);

    return ast_while;
}

ast_t *parser_parse_for(parser_t * parser){

    ast_t * ast_for = NEW_OBJECT(AST_FOR,parser);
    ast_for->countScope = GetScope();
    parser_eat(TYPE(parser),parser,NULL);
    ast_for->for_init = parser_parse_statment(parser);
    parser_eat(TOKEN_SEMI,parser,"%s : you have forgotten semicolon `;` after initializing",ERROR_TO_STRING[SYNTAX_ERROR]);
    ast_for->for_condition = parser_parse_expr(parser);
    parser_eat(TOKEN_SEMI,parser,"%s : you have forgotten semicolon `;` after condition",ERROR_TO_STRING[SYNTAX_ERROR]);

    ast_t * compound = NEW_OBJECT(AST_COMPOUND,parser);
    compound->list_compound = __init__list__(sizeof(struct AST_STRUCT*));

    ast_t * child = parser_parse_statment(parser);
    List_Append(compound->list_compound,child);

    while (TYPE(parser) == TOKEN_COMA || child->type != AST_NOOP){
        if(TYPE(parser) == TOKEN_COMA){
            parser_eat(TOKEN_COMA,parser,NULL);
        }
        child = parser_parse_statment(parser);
        List_Append(compound->list_compound,child);
    }

    parser_eat(TOKEN_LBRACKET,parser,"%s : you have forgotten block for for statment `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);
    if(parser->current.type != TOKEN_RBRACKET){
        ast_for->for_body = parser_parse_statments(parser);
    }
    parser_eat(TOKEN_RBRACKET,parser,"%s : you have forgotten endblock for for statment `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);


    ast_for->for_solve = compound;

    return ast_for;
}

ast_t *parser_parse_break(parser_t *parser){
    ast_t * ast_break = NEW_OBJECT(AST_BREAK,parser);
    parser_eat(TYPE(parser),parser,NULL);
    ast_break->countScope = GetScope();
    return ast_break;
}


ast_t *parser_parse_list(parser_t *parser){
    ast_t * list = NEW_OBJECT(AST_LIST,parser);
    list->countScope = GetScope();
    newValue(&list->value,LIST);

    list->value.as.listValue = __init__list__(sizeof(ast_t*));

    parser_eat(TOKEN_LBRACE,parser,"%s : you have forgotten list `()` ",ERROR_TO_STRING[SYNTAX_ERROR]);


    if(parser->current.type != TOKEN_RBRACE){

        List_Append(list->value.as.listValue,
            parser_parse_expr(parser)
        );

        while (parser->current.type == TOKEN_COMA){
            parser_eat(TOKEN_COMA, parser,"unexpected `,` here");
            List_Append(list->value.as.listValue,
                parser_parse_expr(parser)
            );
        }
        

    }
    
    parser_eat(TOKEN_RBRACE,parser,"%s : you have forgotten list `()` ",ERROR_TO_STRING[SYNTAX_ERROR]);

    

    return list;
}

ast_t *parser_parse_struct_definition(parser_t *parser){
    
    ast_t * ast_struct = NEW_OBJECT(AST_STRUCT_DEFINITION,parser);
    parser_eat(TYPE(parser),parser,NULL);
    allowChar(&ast_struct->struct_definition_name,parser->current.value);
    parser_eat(TYPE(parser),parser,NULL); // à revoir

    ast_struct->struct_definition_item = __init__list__(sizeof(ast_t*));

    // if(parser->current.type == TOKEN_ARROW || parser->current.type == TOKEN_DOUBLE_COLON){
    //     parser_eat(TYPE(parser),parser,NULL);
    //     return parser_parse_struct_definition_inline(ast_struct,parser);
    // }

    parser_eat(TOKEN_LBRACKET,parser,"%s : you have forgotten block `{}` for define struct",ERROR_TO_STRING[SYNTAX_ERROR]);

    if(parser->current.type != TOKEN_RBRACKET){

        ast_t *item = parser_parse_statment(parser);
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
                parser_eat(TOKEN_SEMI, parser,"unexpected `;` here");
            }
            item = parser_parse_statment(parser);

            if(item->type != AST_NOOP){
                if(!isInClassDefinition(item)){
                    Print_error(
                        parser->lexer,
                        "StructDefError: %s n'est pas permis dans la definition d'une structure\n",
                        AST_TO_STRING[item->type]
                    );
                    exit(1);
                }
                // if(!isSelfOneMethodDefinition(item)){
                //     Print_error(
                //         parser->lexer,
                //         "StructDefError: le pointeur self est definir plusieur fois\n"
                //     );
                //     exit(1);
                // }
                List_Append(
                    ast_struct->struct_definition_item,
                    item
                );
            }
        }
    }
 
    parser_eat(TOKEN_RBRACKET,parser,"%s : you have forgotten block `{}` ",ERROR_TO_STRING[SYNTAX_ERROR]);


    return ast_struct;
}


ast_t * makeAst(const char * chaine){
    lexer_t lex;

	newLexerWithString(&lex,chaine);
	ast_t *node = parser_parse_one(&lex);

    return node;
}

bool expectedAst(ast_t * node ,ast_t *parent,  uint8_t *expect, int lenght){
    for (int i = 0; i < lenght; i++){
        if(node->type == expect[i]){
            fprintf(stderr,"unexpected tokenizer: you try to inject something like %s here [%d:%d]\n",
                AST_TO_STRING[expect[i]],
                parent != NULL ? parent->line:0, 
                parent != NULL ? parent->colone: 0
            );
            return false;
        }
    }

    return true;
}
