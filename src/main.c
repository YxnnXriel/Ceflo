#include <stdio.h>
#include <stdlib.h>

#include "include/lexer.h"
#include "include/token.h"
#include "include/parser.h"
#include "include/ast.h"
#include "include/visitor.h"


int main(int argc, char const *argv[]){

		lexer_t lexer;
		newLexer(&lexer,argv[1]);

		// token_t token = AS_TOKEN(EOF,"NULL");

		// while( (token = lexer_get_next_token(&lexer)).type != TOKEN_EOF){
		// 	printf("`%s` -- `%s`\n",TOKEN_TO_STRING[token.type],token.value);
		// 	freeToken(&token);
		// }


		// lexerFree(&lexer);


		ast_t *node = parser_parse(&lexer);


		// printf("%d\n",node->list_compound->Size);

		visitor_t visitor;
		newVisitor(&visitor);
		visitor.global_scope->countScope = 0;

		visitor_visit(&visitor,node);

		clearAst();

		freeScope(visitor.global_scope);
		
		lexerFree(&lexer);






	return 0;
}
