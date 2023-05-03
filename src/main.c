
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
#include <string.h>


#include "include/lexer.h"
#include "include/token.h"
#include "include/parser.h"
#include "include/ast.h"
#include "include/visitor.h"
#include "include/clear.h"

#include <unistd.h>


/**
 * TODO: secure lexer entry,and try free current ast if we meet a problem
 * - Faire les attributs et les fonction d'une liste   - OK
 * - Ajouter un dictionnaire(si c'est possible)
 * - Bien d'autre
 * - Fix Bug function call and List  		- OK
 * - Fix Bug list access , new obj   		- OK
 * - Fix lfunc type right            		- OK
 * - Fix Bug char from parser and lexer		- OK
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
			// freeVsisitorGB();
			// clearAst();
			// freeScope(visitor.global_scope);
			
			// lexerFree(&lexer);
			Clear_Ceflo(&visitor,&lexer);
		

	}else{
		printf("[CEFLO]\n");
		printf("./Ceflo [args]\n");
		printf("file error\n");
	}


    // memcpy(value,&lexer->fileCode[initiale],lenght);


	// char *chaine = "yann Ariel";

	// printf("%d \n",memcmp("Areel",&chaine[5],5) == 0);

	return 0;
}
