#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/value_expr.h"


void performe_visit_tree_char(Value_t * value, Value_t * left , token_t * op, Value_t * right){
        switch (right->type){
            case NUMBER:
                value->type = NUMBER;
                switch (op->type){
                    case TOKEN_PLUS : value->as.charValue = left->as.charValue + right->as.numberValue;break;
                    case TOKEN_MINUS : value->as.charValue = left->as.charValue - right->as.numberValue;break;
                    case TOKEN_MULT : value->as.charValue = left->as.charValue * right->as.numberValue;break;
                    case TOKEN_DIV :  value->as.charValue = left->as.charValue / right->as.numberValue;break;
                    case TOKEN_MODULO :  value->as.charValue = left->as.charValue % (int)right->as.numberValue;break;
                    case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = left->as.charValue < right->as.numberValue;break;
                    case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = left->as.charValue > right->as.numberValue;break;
                    case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.charValue <= right->as.numberValue;break;
                    case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.charValue >= right->as.numberValue;break;
                    case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = left->as.charValue == right->as.numberValue;break;
                    case TOKEN_OR : value->type = BOOL ; value->as.booleanValue = left->as.charValue || right->as.numberValue;break;
        
                    case TOKEN_AND : value->type = BOOL ; value->as.booleanValue = left->as.charValue && right->as.numberValue;break;
                    case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = left->as.charValue != right->as.numberValue;break;
                    default : value->as.charValue = left->as.charValue + right->as.numberValue;break;
                }
                break;

            case NONE:
                value->type = BOOL;
                switch (op->type){
                    case TOKEN_AND : value->as.booleanValue = left->type && right->type;break;
                    case TOKEN_OR : value->as.booleanValue = left->type || right->type;break;
                    case TOKEN_EQUAL_EQUAL : value->as.booleanValue = left->type == right->type;break;
                    case TOKEN_DIF : value->as.booleanValue = left->type != right->type;break;
                }
                break;


            case CHAR:
                value->type = CHAR;
                switch (op->type){
                    case TOKEN_PLUS : value->as.charValue = left->as.charValue+ right->as.charValue;break;
                    case TOKEN_MINUS : value->as.charValue= left->as.charValue- right->as.charValue;break;
                    case TOKEN_MULT : value->as.charValue = left->as.charValue* right->as.charValue;break;
                    case TOKEN_DIV : value->as.charValue  = left->as.charValue/ right->as.numberValue;break;
                    case TOKEN_MODULO :value->type = CHAR;value->as.charValue=left->as.charValue% right->as.charValue;break;
                    case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = left->as.charValue< right->as.charValue;break;
                    case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = left->as.charValue> right->as.charValue;break;
                    case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.charValue <= right->as.charValue;break;
                    case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.charValue >= right->as.charValue;break;
                    case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = left->as.charValue== right->as.charValue;break;
                    case TOKEN_OR : value->type = BOOL ; value->as.booleanValue = left->as.charValue || right->as.charValue;break;
                    case TOKEN_AND : value->type = BOOL ; value->as.booleanValue = left->as.charValue&& right->as.charValue;break;
                    case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = left->as.charValue!= right->as.charValue;break;
                    default : value->as.charValue = left->as.charValue + right->as.charValue;break;
                }
                break;

            case STRING:
                value->type = STRING;
                switch (op->type){
                    case TOKEN_PLUS : value->as.stringValue = String_cat("%c%s",left->as.charValue,right->as.stringValue->str);break;
                }
                break;
        }
}





void performe_visit_tree_number(Value_t * value, Value_t * left , token_t * op, Value_t * right){
            value->type = NUMBER;

    switch (right->type){
        case CHAR:
            value->type = NUMBER;
            switch (op->type){
                case TOKEN_PLUS : value->as.numberValue = left->as.numberValue+ right->as.charValue;break;
                case TOKEN_MINUS : value->as.numberValue = left->as.numberValue- right->as.charValue;break;
                case TOKEN_MULT : value->as.numberValue = left->as.numberValue* right->as.charValue;break;
                case TOKEN_DIV : value->as.numberValue  = left->as.numberValue/ right->as.numberValue;break;
                case TOKEN_MODULO :value->type = NUMBER;value->as.numberValue = (int)left->as.numberValue % right->as.charValue;break;
                
                case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = left->as.numberValue < right->as.charValue;break;
                case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = left->as.numberValue> right->as.charValue;break;

                case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.numberValue <= right->as.charValue;break;
                case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.numberValue >= right->as.charValue;break;

                case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = left->as.numberValue== right->as.charValue;break;
                case TOKEN_AND : value->type = BOOL ; value->as.booleanValue = left->as.numberValue && right->as.charValue;break;
                case TOKEN_OR : value->type = BOOL ; value->as.booleanValue = left->as.numberValue || right->as.charValue;break;
                case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = left->as.numberValue!= right->as.charValue;break;
                default : value->as.numberValue = left->as.numberValue + right->as.charValue;break;
            }
            break;
        case NUMBER:

            switch (op->type){
                case TOKEN_PLUS :value->as.numberValue = left->as.numberValue + right->as.numberValue;break;
                case TOKEN_MINUS : value->as.numberValue = left->as.numberValue - right->as.numberValue;break;
                case TOKEN_MULT : value->as.numberValue = left->as.numberValue * right->as.numberValue;break;
                case TOKEN_DIV :  value->as.numberValue = left->as.numberValue / right->as.numberValue;break;
                case TOKEN_MODULO :  value->as.numberValue = (int)left->as.numberValue % (int)right->as.numberValue;break;

                case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = left->as.numberValue < right->as.numberValue;break;
                case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = left->as.numberValue > right->as.numberValue;break;
                
                case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.numberValue <= right->as.numberValue;break;
                case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.numberValue >= right->as.numberValue;break;

                case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = left->as.numberValue == right->as.numberValue;break;
                case TOKEN_AND : value->type = BOOL ; value->as.booleanValue = left->as.numberValue && right->as.numberValue;break;
                case TOKEN_OR : value->type = BOOL ; value->as.booleanValue = left->as.numberValue || right->as.numberValue;break;
                case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = left->as.numberValue != right->as.numberValue;break;
                default : value->as.numberValue = left->as.numberValue + right->as.numberValue;break;
            }
            break;

        case NONE:
            value->type = BOOL;
            switch (op->type){
                case TOKEN_AND : value->as.booleanValue = left->type && right->type;break;
                case TOKEN_OR : value->as.booleanValue = left->type || right->type;break;
                case TOKEN_EQUAL_EQUAL : value->as.booleanValue = left->type == right->type;break;
                case TOKEN_DIF : value->as.booleanValue = left->type != right->type;break;
            }
            break;


        case STRING:
            value->type = STRING;
            switch (op->type){
                case TOKEN_PLUS :
                value->as.stringValue = String_cat("%g%s",left->as.numberValue,right->as.stringValue->str);break;
            }
            break;
    }

}


void performe_visit_tree_bool(Value_t * value, Value_t * left , token_t * op, Value_t * right){

        switch (right->type){
            case CHAR:
                value->type = CHAR;
                switch (op->type){
                    case TOKEN_PLUS : value->as.charValue = left->as.booleanValue + right->as.charValue;break;
                    case TOKEN_MINUS : value->as.charValue= left->as.booleanValue - right->as.charValue;break;
                    case TOKEN_MULT : value->as.charValue = left->as.booleanValue * right->as.charValue;break;
                    case TOKEN_DIV : value->as.charValue  = left->as.booleanValue / right->as.numberValue;break;
                    case TOKEN_MODULO :value->type = NUMBER;value->as.booleanValue  =left->as.booleanValue % right->as.charValue;break;
                    
                    case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue  < right->as.charValue;break;
                    case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue > right->as.charValue;break;
                    case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue  <= right->as.charValue;break;
                    case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue >= right->as.charValue;break;

                    case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue == right->as.charValue;break;
                    case TOKEN_OR : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue  || right->as.charValue;break;
                    case TOKEN_AND : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue  && right->as.charValue;break;
                    case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue != right->as.charValue;break;
                    default : value->as.charValue  = left->as.booleanValue  + right->as.charValue;break;
                }
                break;


            case NUMBER:
            value->type = NUMBER;
                switch (op->type){
                    case TOKEN_PLUS : value->as.numberValue = left->as.booleanValue + right->as.numberValue;break;
                    case TOKEN_MINUS : value->as.numberValue = left->as.booleanValue - right->as.numberValue;break;
                    case TOKEN_MULT : value->as.numberValue = left->as.booleanValue * right->as.numberValue;break;
                    case TOKEN_DIV :  value->as.numberValue = left->as.booleanValue / right->as.numberValue;break;
                    case TOKEN_MODULO :  value->as.numberValue = left->as.booleanValue % (int)right->as.numberValue;break;



                    case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue < right->as.numberValue;break;
                    case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue > right->as.numberValue;break;

                    case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue <= right->as.numberValue;break;
                    case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue >= right->as.numberValue;break;

                    case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue == right->as.numberValue;break;
                    case TOKEN_OR : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue || right->as.numberValue;break;
                    case TOKEN_AND : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue && right->as.numberValue;break;
                    case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue != right->as.numberValue;break;
                    default : value->as.numberValue = left->as.booleanValue + right->as.numberValue;break;
                }
                break;
            case BOOL:
            value->type = BOOL;
                switch (op->type){
                    case TOKEN_MODULO :  value->as.booleanValue = left->as.booleanValue % right->as.booleanValue;break;

                    case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue < right->as.booleanValue;break;
                    case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue > right->as.booleanValue;break;
                    
                    case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue <= right->as.booleanValue;break;
                    case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = left->as.booleanValue >= right->as.booleanValue;break;
                    case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue == right->as.booleanValue;break;
                    case TOKEN_AND : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue && right->as.booleanValue;break;
                    case TOKEN_OR : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue || right->as.booleanValue;break;
                    case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = left->as.booleanValue != right->as.booleanValue;break;
                    default : value->as.booleanValue = left->as.booleanValue + right->as.booleanValue;break;
                }
                break;
            case NONE:
                value->type = BOOL;
                switch (op->type){
                    case TOKEN_AND : value->as.booleanValue = left->type && right->type;break;
                    case TOKEN_OR : value->as.booleanValue = left->type || right->type;break;
                    case TOKEN_EQUAL_EQUAL : value->as.booleanValue = left->type == right->type;break;
                    case TOKEN_DIF : value->as.booleanValue = left->type != right->type;break;
                }
                break;

            case STRING:
                value->type = STRING;
                switch (op->type){
                    case TOKEN_PLUS : value->as.stringValue = String_cat("%s%s",left->as.booleanValue ? "true":"false",right->as.stringValue->str);break;
                }
                break;
        }

}


void performe_visit_tree_str(Value_t * value, Value_t * left , token_t * op, Value_t * right){

                value->type = STRING;


        switch (right->type){
            case NUMBER:
                switch (op->type){
                    case TOKEN_PLUS :
                        value->as.stringValue = String_cat("%s%g",left->as.stringValue->str,right->as.numberValue);break;
                    case TOKEN_MULT :{
                        if(right->as.numberValue <= 0){
                            // Print_visitor_error(
                            //     right->line,
                            //     right->colone,
                            //     "./Ceflo",
                            //     " ...* %g pas pris en charge \n",
                            //     right->as.numberValue
                            // );
                            // clear_ceflo_clean();
                            exit(1);
                        }
                        char *chaine = calloc(strlen(left->as.stringValue->str)*right->as.numberValue,sizeof(char*));
                        for (int i = 0; i < right->as.numberValue; i++)
                            strcat(chaine,left->as.stringValue->str);

                        value->as.stringValue = new_String(chaine);
                        free(chaine);
                    }
                        break;
                }
                break;

            case NONE:
                value->type = BOOL;
                switch (op->type){
                    case TOKEN_AND : value->as.booleanValue = left->type && right->type;break;
                    case TOKEN_OR : value->as.booleanValue = left->type || right->type;break;
                    case TOKEN_EQUAL_EQUAL : value->as.booleanValue = left->type == right->type;break;
                    case TOKEN_DIF : value->as.booleanValue = left->type != right->type;break;
                }
                break;

            case CHAR:
                switch (op->type){
                    case TOKEN_PLUS : value->as.stringValue = String_cat("%s%c",left->as.stringValue->str,right->as.charValue);break;
                    // case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.charValue) < 0 ? 1 : 0; break;
                    // case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.charValue ) > 0 ? 1 : 0; break;
                    // case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.charValue) == 0 ? 1 : 0; break;
                    // case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.charValue) != 0 ? 1 : 0; break;
                }
                break;

            case STRING:
                switch (op->type){
                    case TOKEN_PLUS : value->as.stringValue = String_cat("%s%s",left->as.stringValue->str,right->as.stringValue->str);break;
                    
                    case TOKEN_INF : value->type = BOOL ;value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.stringValue->str) < 0 ? 1 : 0; break;
                    case TOKEN_SUP : value->type = BOOL ;value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.stringValue->str) > 0 ? 1 : 0; break;
                    
                    case TOKEN_INF_EQUAL : value->type = BOOL ;value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.stringValue->str) <= 0 ? 1 : 0; break;
                    case TOKEN_SUP_EQUAL : value->type = BOOL ;value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.stringValue->str) >= 0 ? 1 : 0; break;

                    case TOKEN_EQUAL_EQUAL : value->type = BOOL ; value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.stringValue->str) == 0 ? 1 : 0; break;
                    case TOKEN_DIF : value->type = BOOL ; value->as.booleanValue = strcmp(left->as.stringValue->str,right->as.stringValue->str) != 0 ? 1 : 0; break;
                }
                break;
        }
}

void performe_visit_tree_none(Value_t * value, Value_t * left , token_t * op, Value_t * right){
        switch (op->type){
            case TOKEN_AND : value->as.booleanValue = left->type && right->type;break;
            case TOKEN_EQUAL_EQUAL : value->as.booleanValue = left->type == right->type;break;
            case TOKEN_DIF : value->as.booleanValue = left->type != right->type;break;
            case TOKEN_OR : value->as.booleanValue = left->type || right->type;break;
        }
}


int value_to_type_ast(Value_t *value){
    switch (value->type){
        case NONE :return AST_NONE;
        case BOOL : return AST_BOOL;
        case CHAR : return AST_CHAR;
        case NUMBER : return AST_NUMBER;
        case STRING : return AST_STRING;
        case LIST : return AST_LIST;
        case OBJECT : return AST_OBJ;
    }
    return -1;
}

int type_ast_to_value(ast_t * node_value){
     switch (node_value->type){
        case AST_NONE :return NONE;
        case AST_BOOL : return BOOL;
        case AST_CHAR : return CHAR;
        case AST_NUMBER : return NUMBER;
        case AST_STRING : return STRING;
        case AST_LIST : return LIST;
        case AST_OBJ : return OBJECT;
    }
    return -1;   
}