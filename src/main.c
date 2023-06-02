
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

/*
	L'interpréteur est terminé !
	Tout ce qui suivra comme mise à jour n'est qu'optimisation
	Du code et précision des erreurs soulevées par l'interpréteur.
	Il n'est pas le premier interpréteur que j'ai écrit, juste le premier qui est complet ,
	qui m'a été demander par une amie (à qui j'ai dédié le nom de l'interpréteur), 
	et c'est aussi le premier qui a un traçage de l'AST.

	L'interpréteur suit un parcours simple et basique :
		- lexème du fichier source pour avoir une liste de jetons
		- scanner les jetons pour donner un sens pour la génération de l'AST
		- Visiter l'arbre et exécuter chaque instruction des nœuds

			[Lexer] ==> [Parser] ==> [AST] ==> [Visitor]
*/


/**
 * TODO: secure lexer entry,and try free current ast if we meet a problem
 * - Faire les attributs et les fonction d'une liste   - OK
 * - Ajouter un dictionnaire(si c'est possible)
 * - Bien d'autre
 * - Fix Bug function call and List  		- OK
 * - Fix Bug list access , new obj   		- OK
 * - Fix lfunc type right            		- OK
 * - Fix Bug char from parser and lexer		- OK
 * - Rewrite clean all						- OK
 * - Fix Bug typé							- OK
 */ 


lexer_t lexer;



int main(int argc, char const *argv[]){

	if(argc >= 2){

		
			newLexer(&lexer,argv[1]);
			ast_t *node = parser_parse(&lexer);
			visitor_t visitor;
			newVisitor(&visitor);
			newGb();
			visitor.global_scope->countScope = 0;
			clear_ceflo_push(
				&visitor
			);
			visitor_visit(&visitor,node);
			// freeVsisitorGB();
			// clearAst();
			// freeScope(visitor.global_scope);
			
			// lexerFree(&lexer);
			Clear_Ceflo(&lexer);
		

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
