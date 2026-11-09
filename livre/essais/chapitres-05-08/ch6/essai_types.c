#include <stdio.h>
#include "visitor.h"
#include "typer.h"

static void essaie(visitor_t *visitor, value_t *value, const char *type){
    printf("%-6s est-il %-5s ? %s\n", AS_STRING(Value_to_string(visitor, value))->str, type,
        typer_has_type(visitor, NULL, value, type) ? "oui" : "non");
}

int main(void){
    ceflo_stack_init();
    visitor_t visitor = {0};
    visitor.scope = newScope(NULL);

    essaie(&visitor, makeNumber(2), "int");
    essaie(&visitor, makeNumber(2.5), "int");
    essaie(&visitor, makeNumber(2.5), "float");
    essaie(&visitor, makeString("2"), "int");
    essaie(&visitor, makeChar('a'), "str");

    value_t *zero = typer_default_value(&visitor, NULL, "int");
    value_t *vide = typer_default_value(&visitor, NULL, "list");
    printf("sans valeur : int -> %s, list -> %s\n",
        AS_STRING(Value_to_string(&visitor, zero))->str, AS_STRING(Value_to_string(&visitor, vide))->str);
    printf("nom dans un message : %s et %s\n", typer_type_name(makeNumber(3)), typer_type_name(makeNumber(3.5)));

    essaie(&visitor, makeNumber(1), "Point");
    return 0;
}
