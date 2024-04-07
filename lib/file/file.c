#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include "../../src/include/ast.h"
#include "../../src/include/visitor.h"
#include "../../src/include/clear.h"


struct AST_STRUCT *file_stdin(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){
    return makeGeneric(stdin);
}


struct AST_STRUCT *file_stdout(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){
    return makeGeneric(stdout);
}


struct AST_STRUCT *file_fopen(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

    uint8_t check[] = {STRING,STRING};


    if(api_check_params_function(vs,node,args,check,2) == false){
        return makeError();
    }


    struct AST_STRUCT *generic = NULL;
    struct AST_STRUCT *arg1 = visitor_visit(vs,List_Ast_Get(args,0));
    struct AST_STRUCT *arg2 = visitor_visit(vs,List_Ast_Get(args,1));

    
    const char *file = arg1->value.as.stringValue->str;
    const char *md =  arg2->value.as.stringValue->str;

    FILE *fic = fopen(file,md);

    if(fic == NULL){
        fprintf(stderr,
            "%s : %s not working or mod %s is not supported\n",
            node->function_call_name,
            AS_STRING(arg1->value),
            AS_STRING(arg2->value)
        );
        
        return makeError();

    }

    generic = makeGeneric(fic);

    return generic;

}



struct AST_STRUCT *file_fclose(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

    uint8_t check[] = {GENERIC};

    if(api_check_params_function(vs,node,args,check,1) == false){
        return makeError();

    }
    
    ast_t * arg1 =  visitor_visit(vs, List_Ast_Get(args,0));


    FILE * fic = arg1->value.as.GenericValue;
    fclose(fic);

    return makeNumber(1);
}

struct AST_STRUCT *file_fprintf(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

    uint8_t check = GENERIC;

    ast_t * arg1 =  visitor_visit(vs, List_Ast_Get(args,0));
    
    api_check_param_function(
        vs,
        node,
        arg1,
        check,
        1
    );

    FILE * fic = arg1->value.as.GenericValue;

    int i = 0;
    for ( i = 1; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(vs,child);
        fprintValueAst(fic,child);
    }

    return makeNumber(1);
}

struct AST_STRUCT *file_fgetline(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

        uint8_t check = GENERIC;

    ast_t * arg1 =  visitor_visit(vs, List_Ast_Get(args,0));
    
    api_check_param_function(
        vs,
        node,
        arg1,
        check,
        1
    );

    FILE * fic = arg1->value.as.GenericValue;

    ast_t * line = NULL;

    char buffer[BUFSIZ];

    fgets(buffer,BUFSIZ-1,fic);

    line = makeString(buffer);

    return line;
}