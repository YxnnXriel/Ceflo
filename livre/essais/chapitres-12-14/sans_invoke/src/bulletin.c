#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/bulletin.h"
#include "include/error.h"
#include "include/parser.h"

static value_t *bulletin_print(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)node;
    for(int i = 0; i < argc; i++)
        Value_print(visitor,stdout,args[i]);
    return makeNone();
}

static value_t *bulletin_println(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    bulletin_print(visitor,node,args,argc);
    printf("\n");
    return makeNone();
}

// Lit une ligne sans sa fin de ligne. Retourne false à la fin de l'entrée.
static bool bulletin_read_line(buffer_t *line){
    int carac = fgetc(stdin);
    if(carac == EOF)
        return false;

    while(carac != EOF && carac != '\n'){
        Buffer_Append_Char(line,(char)carac);
        carac = fgetc(stdin);
    }

    if(line->size > 0 && line->data[line->size - 1] == '\r'){
        line->size -= 1;
        line->data[line->size] = '\0';
    }
    return true;
}

// input("prompt") : la ligne lue, ou none à la fin de l'entrée
static value_t *bulletin_input(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    bulletin_print(visitor,node,args,argc);
    fflush(stdout);

    buffer_t line;
    newBuffer(&line);

    value_t *result = bulletin_read_line(&line) ?
        makeStringLenght(line.data,(int)line.size) : makeNone();

    Buffer_Free(&line);
    return result;
}

static value_t *bulletin_nextInt(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    bulletin_print(visitor,node,args,argc);
    fflush(stdout);

    buffer_t line;
    newBuffer(&line);

    char *end = NULL;
    double number = 0;
    bool valid = bulletin_read_line(&line);
    if(valid){
        number = strtod(line.data,&end);
        valid = end != line.data && *end == '\0';
    }
    Buffer_Free(&line);

    if(!valid)
        visitor_error(node,"`nextInt` : un nombre était attendu");

    return makeNumber(number);
}

static value_t *bulletin_exit(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    if(argc == 0)
        exit(0);

    Value_check_args(node,"exit",argc,1);
    Value_check_type(node,"exit",args[0],NUMBER);
    exit((int)Value_number_to_char(AS_NUMBER(args[0])) & 0xFF);
}


//                          ===================================== $FORMAT =================================================


// Un même ${...} n'est analysé qu'une fois, même dans une boucle.
static table_t format_cache;

static ast_t *bulletin_format_parse(const char *expr){
    hash_t *cached = Hash_Get(&format_cache,expr);
    if(cached != NULL)
        return cached->value;

    lexer_t lexer;
    newLexerWithString(&lexer,"format",expr);
    ast_t *node = parser_parse_one(&lexer);

    // La clé doit vivre aussi longtemps que le cache : un noeud de l'AST la garde.
    ast_t *holder = newAst(AST_NOOP);
    allowChar(&holder->variable_name,expr);
    Hash_Set(&format_cache,holder->variable_name,node);

    return node;
}

/**
 * @brief "Bonjour ${nom}, ${1 + 1}" : chaque ${...} est une expression CeFlo,
 *        évaluée dans la portée de l'appelant. \${ écrit un vrai ${.
 */
static value_t *bulletin_format(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    Value_check_args(node,"format",argc,1);
    Value_check_type(node,"format",args[0],STRING);

    string_t *format = AS_STRING(args[0]);

    buffer_t out;
    newBuffer(&out);

    for(int i = 0; i < format->lenght; i++){
        char carac = format->str[i];

        if(carac == '\\' && i + 1 < format->lenght && format->str[i + 1] == '$'){
            Buffer_Append_Char(&out,'$');
            i += 1;
        }else if(carac == '$' && i + 1 < format->lenght && format->str[i + 1] == '{'){
            int start = i + 2;
            int depth = 1;
            int end = start;
            for(; end < format->lenght; end++){
                if(format->str[end] == '{')
                    depth += 1;
                if(format->str[end] == '}' && --depth == 0)
                    break;
            }
            if(depth != 0){
                Buffer_Free(&out);
                visitor_error(node,"`format` : ${ n'est jamais fermé");
            }

            buffer_t expr;
            newBuffer(&expr);
            Buffer_Append(&expr,format->str + start,(size_t)(end - start));
            value_t *value = visitor_visit(visitor,bulletin_format_parse(expr.data));
            Buffer_Free(&expr);

            Value_to_buffer(visitor,&out,value);
            i = end;
        }else{
            Buffer_Append_Char(&out,carac);
        }
    }

    value_t *result = makeStringLenght(out.data,(int)out.size);
    Buffer_Free(&out);
    return result;
}

static value_t *bulletin_printf(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    Value_print(visitor,stdout,bulletin_format(visitor,node,args,argc));
    return makeNone();
}


//                          ===================================== $TABLE ==================================================


typedef struct BULLETIN_STRUCT{
    const char *name;
    ceflo_func_t func;
}bulletin;

static const bulletin Bulletin_List[] = {
    {"println",&bulletin_println},
    {"print",&bulletin_print},
    {"input",&bulletin_input},
    {"format",&bulletin_format},
    {"printf",&bulletin_printf},
    {"nextInt",&bulletin_nextInt},
    {"exit",&bulletin_exit},
    {NULL,NULL}
};

void bulletin_define_all(scope_t *scope, int argc, char **argv){
    for(const bulletin *item = Bulletin_List; item->name != NULL; item++)
        scope_define(scope,item->name,makeBuiltin(item->name,item->func));

    value_t *arguments = makeList();
    scope_define(scope,"argv",arguments);
    for(int i = 0; i < argc; i++)
        Value_list_append(arguments,makeString(argv[i]));
}

void bulletin_free(void){
    Hash_Free(&format_cache);
}
