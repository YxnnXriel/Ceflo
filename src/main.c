
/**
 * @file main.c
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

#include "include/lexer.h"
#include "include/token.h"
#include "include/parser.h"
#include "include/ast.h"
#include "include/visitor.h"


/**
 * TODO: secure lexer entry,and try free current ast if we meet a problem
 * - Faire les attributs et les fonction d'une liste 
 * - Ajouter un dictionnaire(si c'est possible)
 * - Bien d'autre
 * - Fix Bug function call and List
 */

int main(int argc, char const *argv[]){

	if(argc >= 2){
		lexer_t lexer;

		newLexer(&lexer,argv[1]);

		ast_t *node = parser_parse(&lexer);

		visitor_t visitor;
		newVisitor(&visitor);
		newGb();
		
		visitor.global_scope->countScope = 0;

		visitor_visit(&visitor,node);

		clearAst();

		freeScope(visitor.global_scope);
		
		lexerFree(&lexer);

	}else{
		printf("[CEFLO]\n");
		printf("./Ceflo [args]\n");
		printf("file error\n");
	}


	




	return 0;
}
