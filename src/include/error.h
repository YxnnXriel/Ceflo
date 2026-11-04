#ifndef __ERROR_H__
#define __ERROR_H__

    struct AST_STRUCT;

    // Enregistre un texte source pour pouvoir l'afficher dans les erreurs.
    // Le module devient propriétaire de `code`. Retourne un nom de fichier stable.
    const char *error_add_source(const char *name, char *code);
    void error_free_sources(void);

    // Toutes ces fonctions affichent l'erreur et arrêtent le programme.
    _Noreturn void error_syntax(const char *file, int line, int colone, const char *format, ...)
        __attribute__((format(printf,4,5)));
    _Noreturn void visitor_error(const struct AST_STRUCT *node, const char *format, ...)
        __attribute__((format(printf,2,3)));

#endif
