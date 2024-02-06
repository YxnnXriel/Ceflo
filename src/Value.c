#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/str.h"
#include "include/Value.h"
#include "include/debug.h"
#include "include/error.h"
#include "include/typer.h"


#define RETURN_NONE newAst(AST_NONE)

#define FOR_GLOBAL(value) ((value)->variable_forGlobal = true)
#define RESET_GLOBAL(value) ((value)->variable_forGlobal = false)

static ast_t * value_list_for(int index_function , visitor_t * visitor , list_t *list , ast_t * value ){
    ast_t *get = NULL ;

    if(index_function == 0){
        if(value->function_call_args.size > 1){
            get = newAst(AST_LIST);
            newValue(&get->value,LIST);
            get->value.as.listValue = __init__list__(sizeof(ast_t*));
        }
    }



    for (int i = 0; i < (int)value->function_call_args.size; i++){
        ast_t *val =  attr_value_list[index_function].func_list_name(visitor,list,List_Ast_Get(&value->function_call_args,i));
        if(index_function == 0){
            if(value->function_call_args.size == 1)
                return val;
            else
                List_Append(get->value.as.listValue,val);
        }
    }
    return get != NULL ? get: value;
}

static ast_t * value_list_switch_function_call(int index_function , visitor_t * visitor , list_t *list , ast_t * value){

    switch (index_function){
        case 0:
        case 1:
        case 2:
            return value_list_for(index_function,visitor,list,value);
        default:
            return attr_value_list[index_function].func_list_name(visitor,list,value);

    }

    return value;
    
}


static bool is_null_args(ast_t *args){
    return args->function_call_args.size == 0 ? true: false; 
}

ast_t * value_List_lenght(visitor_t * visitor , list_t *list , ast_t * value){

    if(!is_null_args(value)) return RETURN_NONE;

    ast_t * lenght = newAst(AST_NUMBER);
    newValue(&lenght->value,NUMBER);
    lenght->value.as.numberValue = list->Size;

    return lenght;

}

ast_t * value_List_append(visitor_t * visitor , list_t *list , ast_t * value){
    FOR_GLOBAL(value);
    ast_t * item = visitor_visit(visitor,value);
    RESET_GLOBAL(value);
    List_Append(list,ast_copy(item,visitor->global_scope));
    return item;
}

ast_t * value_List_get(visitor_t * visitor , list_t *list , ast_t * value){
    
    FOR_GLOBAL(value);
    ast_t * l = visitor_visit(visitor,value);
    RESET_GLOBAL(value);

    if(l->type == AST_NUMBER){
        if(l->value.as.numberValue < list->Size)
            return List_Get(list,l->value.as.numberValue);
        
        return RETURN_NONE;
    }

    return RETURN_NONE;
}

ast_t * value_List_deleted(visitor_t * visitor , list_t *list , ast_t * value){


    FOR_GLOBAL(value);
    ast_t * l = visitor_visit(visitor,value);
    RESET_GLOBAL(value);

    if(l->type == AST_NUMBER){

        List_Del_without_supp(
            list,
            l->value.as.numberValue
        );

        return l;            
    }

    return newAst(AST_NONE);
}
ast_t * value_List_print(visitor_t * visitor , list_t *list , ast_t * value){
    if(!is_null_args(value)) return RETURN_NONE;

        printlistByeList(list);
    return value;
}

ast_t * value_list_switch_function(visitor_t * visitor,list_t *list , ast_t *value){


    if(value->type == AST_FUNCTION_CALL){
    
        for (int i = 0; i < VALUE_LIST_ATTR; i++){
            if(strcmp(attr_value_list[i].name,value->function_call_name) == 0){
                return value_list_switch_function_call(i,visitor,list,value);
            }
        }

    }

    return NULL;
}


ast_t * value_List_filter(visitor_t * visitor , list_t *list , ast_t * value){

    

    ast_t * func = visitor_visit(visitor,List_Ast_Get(&value->function_call_args,0));

    list_ast_t new = List_Ast_Cpy_Without_Index(&value->function_call_args,0);

    
    if(func->type == AST_LFUNC){
        return visitor_visit_function(
            visitor,
            func->value.as.lfuncValue,
            &new
        );
    }


    return RETURN_NONE;

}







// ======================================== $STR ======================================================================


ast_t * value_str_switch_function(visitor_t * visitor,string_t * str, ast_t *value){

    if(value->type == AST_FUNCTION_CALL){
        for (int i = 0; i < VALUE_STRING_ATTR; i++){
            if(strcmp(attr_value_str[i].name,value->function_call_name)==0){
                return attr_value_str[i].func(visitor,str,value);
            }
        }
        
    }

    return NULL;

}

ast_t *Value_String_lenght(visitor_t *visitor , string_t * str , ast_t * value){
    ast_t * val = newAstWithLine(AST_NUMBER,value->line,value->colone);
    newValue(&val->value,NUMBER);
    val->value.as.numberValue = str->lenght;

    return val;
}

ast_t *Value_String_format(visitor_t *visitor , string_t * str , ast_t * value){


    char buffer[BUFSIZ];

    int n = 0;

    for (int i = 0; i < str->lenght; i++){
        if(String_index(str,i) == '{'){
            if(i+1 != str->lenght && String_index(str,i+1) == '}'){
                ast_t * item = visitor_visit(visitor,List_Ast_Get(&value->function_call_args,n));
                typer_caste_value(AST_STRING,&item);
                strcat(buffer,AS_STRING(item->value));
                i++;
                n++;
            }else{
                strcat(buffer,"{");
            }
        }else{
            sprintf(buffer + strlen(buffer), "%c", String_index(str,i));
        }
    }

    ast_t * string = newAst(AST_STRING);
    string->value.as.stringValue = new_String(buffer);
    return string;

}

// ======================================= expr





// BOOL

static ast_t *Value_New_Bool(bool x){
    ast_t *new = newAst(AST_BOOL);
    new->value.type = BOOL;
    new->value.as.booleanValue = x;
    return new;
}

// CHAR

static ast_t *Value_New_Char(char x){
    ast_t *new = newAst(AST_CHAR);
    new->value.type = CHAR;
    new->value.as.charValue = x;
    return new;
}


// NUMBER

static ast_t *Value_New_Number(double x){
    ast_t *new = newAst(AST_NUMBER);
    new->value.type = NUMBER;
    new->value.as.numberValue = x;
    return new;
}


// STR

static ast_t *Value_New_str(string_t * x){
    ast_t *new = newAst(AST_STRING);
    new->value.type = STRING;
    new->value.as.stringValue = new_String(x->str);
    return new;
}


// #define MAKE_VALUE(func,op)   \
//     func(left->value) op func(right->value)


// #define RESULT(res,type,op) \
//     do{ \
//         switch (type){    \
//             case AST_BOOL: res = Value_New_Bool(MAKE_VALUE(AS_BOOL,op));    \
//             case AST_CHAR: res = Value_New_Char(MAKE_VALUE(AS_CHAR,op));    \
//             case AST_NUMBER: res = Value_New_Number(MAKE_VALUE(AS_NUMBER,op));    \
//         }   \
//     }while(false);





// ast_t *Value_Add(ast_t *left , ast_t *right){
//     ast_t *res = NULL;

//     RESULT(res,left->type,+)

//     if(res == NULL){

//         if(left->type == AST_STRING){
//             res = newAst(AST_STRING);
//             res->value.type = STRING;
//             res->value.as.stringValue = String_cat("%s%s",AS_STRING(left->value),AS_STRING(right->value));
//         }
//     }

//     return res;

// }

// ast_t *Value_Minus(ast_t *left , ast_t *right){
//     ast_t *res = NULL;
    
//     RESULT(res,left->type,-)

//     if(res == NULL){

//         if(left->type == AST_STRING){
//             return NULL;
//         }
//     }

//     return res;

// }


// // LIST

// ast_t *Value_New_List(string_t * x){
//     ast_t *new = newAst(AST_LIST);
//     new->value.type = LIST;
//     new->value.as.listValue = x;
//     return new;
// }


// // OBJECT

// ast_t *Value_New_List(Obj_t * x){
//     ast_t *new = newAst(AST_OBJ);
//     new->value.type = OBJECT;
//     new->value.as.ObjectValue = ;
//     return new;
// }


// LFUNC


Value_t convert_to_type(Value_t *convert , uint8_t type){


    if(type == BOOL){
        switch (convert->type){
            case BOOL : return (Value_t){.type = BOOL , .as.booleanValue = convert->as.booleanValue};
            case CHAR : return (Value_t){.type = BOOL , .as.booleanValue = convert->as.charValue ? true:false};
            case NUMBER : return (Value_t){.type = BOOL , .as.booleanValue = convert->as.numberValue ? true:false};
            case STRING : return (Value_t){.type = BOOL , .as.booleanValue = (convert->as.stringValue != NULL && convert->as.stringValue->str != NULL)? true:false};
        }
    }


    if(type == CHAR){
        switch (convert->type){
            case BOOL : return (Value_t){.type = CHAR , .as.charValue = convert->as.booleanValue};
            case CHAR : return (Value_t){.type = CHAR , .as.charValue = convert->as.charValue};
            case NUMBER : return (Value_t){.type = CHAR , .as.charValue = (char)convert->as.numberValue};
            case STRING : return (Value_t){.type = CHAR , .as.charValue = convert->as.stringValue->lenght }; // a revoir , soit entre calculer la longuer de la chaine 
        }
    }
    
    if(type == NUMBER){
        switch (convert->type){
            case BOOL : return (Value_t){.type = NUMBER , .as.numberValue = convert->as.booleanValue};
            case CHAR : return (Value_t){.type = NUMBER , .as.numberValue = convert->as.charValue};
            case NUMBER : return (Value_t){.type = NUMBER , .as.numberValue = convert->as.numberValue};
            case STRING : return (Value_t){.type = NUMBER , .as.numberValue = convert->as.stringValue->lenght }; // a revoir , soit entre calculer la longuer de la chaine 
        }
    }


    if(type == STRING){
        switch (convert->type){
            case BOOL : return (Value_t){.type = STRING , .as.stringValue = new_String(convert->as.booleanValue ? "true":"false")};
            case CHAR : return (Value_t){.type = STRING , .as.stringValue = String_cat("%c",convert->as.charValue)};
            case NUMBER : return (Value_t){.type = STRING , .as.stringValue = String_cat("%g",convert->as.numberValue)};
            case STRING : return (Value_t){.type = STRING , .as.stringValue = new_String(convert->as.stringValue != NULL ? convert->as.stringValue->str:"")}; // a revoir , soit entre calculer la longuer de la chaine 
        }
    }

}


Value_t value_expr(Value_t * a , Value_t *b , Value_t *res , token_t *token){
    if(Value_verif_is_none(a) == false || Value_verif_is_none(b) == false ){
        Value_t none;
        newValue(&none,NONE);
        return none;
    }

    Value_t *priority = a->type >= b->type? a:b;
    Value_t *unpriority = a->type >= b->type? b:a;

    
    Value_t value = convert_to_type(unpriority,priority->type);

    if(!Value_verif_is_none(&value)){
        *res = value;
        return value;
    }

    switch (token->type){
        case TOKEN_PLUS : return add_value(a,b,res,priority,unpriority);
        case TOKEN_MINUS : return minus_value(a,b,res,priority,unpriority);

    }
}

Value_t add_value(Value_t * a ,Value_t * b, Value_t * res , Value_t *priority , Value_t * unpriority){

    Value_t value = convert_to_type(unpriority,priority->type);

    if(!Value_verif_is_none(&value)){
        *res = value;
        return value;
    }

    switch (priority->type)
    {
        case BOOL:
            *res = (Value_t){.type = BOOL , 
                .as.booleanValue = priority->as.booleanValue + value.as.booleanValue 
            };
            break;
        
        case CHAR : 
            *res = (Value_t){.type = CHAR ,
                .as.charValue = priority->as.charValue + value.as.charValue
            };
            break;

        case NUMBER :
            *res = (Value_t){.type = NUMBER ,
                .as.numberValue = priority->as.numberValue + value.as.numberValue
            };
            break;

        case STRING:
            *res = (Value_t){.type = STRING ,
                .as.stringValue = String_cat("%s%s",priority->as.stringValue->str,value.as.stringValue->str)
            };

            String_free(value.as.stringValue);
            break;
    
    }
    
    return (Value_t){.type = NONE};
}





Value_t minus_value(Value_t * a ,Value_t * b, Value_t * res, Value_t *priority, Value_t * unpriority){

    
    Value_t value = convert_to_type(unpriority,priority->type);

    if(!Value_verif_is_none(&value)){
        *res = value;
        return value;
    }

    switch (priority->type)
    {
        case BOOL:
            *res = (Value_t){.type = BOOL , 
                .as.booleanValue = priority->as.booleanValue - value.as.booleanValue 
            };
            break;
        
        case CHAR : 
            *res = (Value_t){.type = CHAR ,
                .as.charValue = priority->as.charValue - value.as.charValue
            };
            break;

        case NUMBER :
            *res = (Value_t){.type = NUMBER ,
                .as.numberValue = priority->as.numberValue - value.as.numberValue
            };
            break;

        case STRING:
            String_free(value.as.stringValue);
            return (Value_t){.type = NONE};
            break;
    
    }
    
    return (Value_t){.type = NONE};
}