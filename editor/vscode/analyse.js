// Comprend un fichier CeFlo pour proposer l'autocomplétion.
//
// L'éditeur analyse du code en train d'être écrit, donc souvent faux. Rien ici ne
// s'arrête sur une erreur : on lit les jetons, on repère les définitions, et on ne
// calcule le type d'une valeur qu'au moment où il est demandé.
//
//      texte ==> jetons ==> portées et symboles ==> propositions pour le curseur

const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const KEYWORDS = [
    "for", "in", "while", "if", "else", "match", "case", "var", "const", "func", "lfunc", "return", "import", "from",
    "export", "struct", "enum", "true", "false", "none", "or", "and", "new", "continue", "break",
];
const KEYWORD_SET = new Set(KEYWORDS);

const TYPES = ["int", "float", "number", "bool", "char", "str", "string", "list", "map"];

const BUILTINS = {
    println: { params: ["a", "b..."], doc: "Affiche les valeurs, puis passe à la ligne." },
    print: { params: ["a", "b..."], doc: "Affiche les valeurs, sans passer à la ligne." },
    input: { params: ["texte..."], returns: "str", doc: "Affiche le texte et lit une ligne. Vaut `none` à la fin de l'entrée." },
    nextInt: { params: ["texte..."], returns: "number", doc: "Lit une ligne qui contient un nombre." },
    format: { params: ["texte"], returns: "str", doc: "Calcule chaque `${...}` du texte." },
    printf: { params: ["texte"], doc: "Affiche `format(texte)`." },
    exit: { params: ["code"], doc: "Arrête le programme." },
};

const METHODS = {
    list: {
        append: { params: ["a", "b..."], doc: "Ajoute à la fin." },
        remove: { params: ["index"], returns: "element", doc: "Enlève et retourne l'élément." },
        get: { params: ["index"], returns: "element", doc: "L'élément, ou `none` s'il n'existe pas." },
        lenght: { params: [], returns: "number", doc: "Le nombre d'éléments." },
        length: { params: [], returns: "number", doc: "Le nombre d'éléments." },
        print: { params: [], doc: "Affiche la liste." },
        filter: { params: ["fonction"], returns: "self", doc: "Une nouvelle liste, avec les éléments où la fonction est vraie." },
    },
    map: {
        get: { params: ["cle", "defaut"], doc: "La valeur, ou `none` ou `defaut` si la clé n'existe pas." },
        set: { params: ["cle", "valeur"], returns: "self", doc: "Ajoute ou remplace, et retourne le map." },
        has: { params: ["cle"], returns: "bool", doc: "`true` si la clé existe." },
        remove: { params: ["cle"], doc: "Enlève la clé et retourne sa valeur." },
        keys: { params: [], returns: "list", doc: "La liste des clés, dans l'ordre d'insertion." },
        values: { params: [], returns: "list", doc: "La liste des valeurs, dans l'ordre d'insertion." },
        lenght: { params: [], returns: "number", doc: "Le nombre de clés." },
        length: { params: [], returns: "number", doc: "Le nombre de clés." },
    },
    str: {
        lenght: { params: [], returns: "number", doc: "Le nombre d'octets." },
        length: { params: [], returns: "number", doc: "Le nombre d'octets." },
        format: { params: ["a", "b..."], returns: "str", doc: "Remplace chaque `{}`." },
        split: { params: ["separateur"], returns: "list_str", doc: "Découpe en liste." },
    },
    enum: {
        values: { params: [], returns: "list_items", doc: "La liste des éléments, dans leur ordre." },
    },
};


// Les modules compilés dans l'interpréteur, décrits en CeFlo pour être lus comme les autres.
// Une fonction qui peut échouer retourne `none` : sans type de retour déclaré, son résultat
// sert à proposer, mais pas à souligner une erreur.
const STANDARD_MODULES = {
    os: {
        source: `
            func lire(chemin::str) { return "" }
            func lire_lignes(chemin::str) { return [""] }
            func ecrire(chemin::str, texte::str) : bool { }
            func ajouter(chemin::str, texte::str) : bool { }
            func existe(chemin::str) : bool { }
            func est_dossier(chemin::str) : bool { }
            func taille(chemin::str) { return 0 }
            func dossier(chemin::str) { return [""] }
            func creer_dossier(chemin::str) : bool { }
            func supprimer(chemin::str) : bool { }
            func renommer(ancien::str, nouveau::str) : bool { }
            func copier(source::str, cible::str) : bool { }
            func chemin(a, b) : str { }
            func dossier_courant() { return "" }
            func changer_dossier(chemin::str) : bool { }
            func env(nom::str) { return "" }
            func commande(texte::str) { return { code: 0, sortie: "" } }
            func lancer(texte::str) : number { }
        `,
        variadic: { chemin: ["a", "b..."] },
        docs: {
            lire: "Le contenu du fichier, ou `none` s'il ne peut pas être lu.",
            lire_lignes: "La liste des lignes, sans leur fin de ligne, ou `none`.",
            ecrire: "Remplace le contenu du fichier. Retourne `false` en cas d'échec.",
            ajouter: "Ajoute à la fin du fichier, et le crée s'il manque. Retourne `false` en cas d'échec.",
            existe: "`true` si le fichier ou le dossier existe.",
            est_dossier: "`true` si c'est un dossier.",
            taille: "Le nombre d'octets, ou `none`.",
            dossier: "Les noms triés du dossier, ou `none`.",
            creer_dossier: "Crée le dossier et ses parents. Un dossier qui existe déjà convient.",
            supprimer: "Enlève un fichier ou un dossier vide.",
            renommer: "Renomme ou déplace. Retourne `false` en cas d'échec.",
            copier: "Copie un fichier. Retourne `false` en cas d'échec.",
            chemin: "Assemble un chemin : `chemin(\"a\", \"b.txt\")` donne `a/b.txt`.",
            dossier_courant: "Le dossier de travail, ou `none`.",
            changer_dossier: "Change le dossier de travail. Retourne `false` en cas d'échec.",
            env: "La valeur de la variable d'environnement, ou `none`.",
            commande: "Lance la commande dans le shell, et retourne `{ code, sortie }`, ou `none`.",
            lancer: "Lance la commande dans le shell : elle affiche son texte. Retourne son code de sortie.",
        },
    },
};

const standardCache = new Map();

function standardModule(name) {
    if (!Object.prototype.hasOwnProperty.call(STANDARD_MODULES, name)) return null;
    if (standardCache.has(name)) return standardCache.get(name);

    const description = STANDARD_MODULES[name];
    const module = new Module(description.source, null, null);
    for (const [functionName, symbol] of module.scope.symbols) {
        symbol.func.standard = true;
        symbol.func.doc = description.docs[functionName];
        if (description.variadic[functionName]) {
            symbol.func.params = description.variadic[functionName];
            symbol.func.variadic = true;
        }
    }
    standardCache.set(name, module);
    return module;
}


// ===================================== $JETONS =====================================


function tokenize(text) {
    const tokens = [];
    const comments = [];
    let i = 0;

    const push = (type, value, start, closed = true) => tokens.push({ type, value, start, end: i, closed });
    const isIdStart = (c) => /[A-Za-z_]/.test(c);
    const isId = (c) => /[A-Za-z0-9_]/.test(c);
    const isDigit = (c) => c >= "0" && c <= "9";

    while (i < text.length) {
        const c = text[i];
        const start = i;

        if (/\s/.test(c)) {
            i += 1;
        } else if (c === "/" && text[i + 1] === "/") {
            while (i < text.length && text[i] !== "\n") i += 1;
            comments.push([start, i]);
        } else if (c === "/" && text[i + 1] === "*") {
            const end = text.indexOf("*/", i + 2);
            i = end < 0 ? text.length : end + 2;
            comments.push([start, i]);
        } else if (c === "#" && text[i + 1] === "!" && i === 0) {
            while (i < text.length && text[i] !== "\n") i += 1;
            comments.push([start, i]);
        } else if (c === '"' || c === "'") {
            i += 1;
            while (i < text.length && text[i] !== c && text[i] !== "\n") {
                i += text[i] === "\\" ? 2 : 1;
            }
            const closed = text[i] === c;
            if (closed) i += 1;
            push(c === '"' ? "str" : "chr", text.slice(start + 1, closed ? i - 1 : i), start, closed);
        } else if (isIdStart(c)) {
            while (i < text.length && isId(text[i])) i += 1;
            push("id", text.slice(start, i), start);
        } else if (isDigit(c)) {
            while (i < text.length && (isDigit(text[i]) || (text[i] === "." && isDigit(text[i + 1] || "")))) i += 1;
            push("num", text.slice(start, i), start);
        } else {
            const two = text.slice(i, i + 2);
            i += ["::", "->", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%="].includes(two) ? 2 : 1;
            push("op", text.slice(start, i), start);
        }
    }
    return { tokens, comments };
}

const isOp = (token, value) => token !== undefined && token.type === "op" && token.value === value;
const isWord = (token, value) => token !== undefined && token.type === "id" && token.value === value;
const isName = (token) => token !== undefined && token.type === "id" && !KEYWORD_SET.has(token.value);


// ===================================== $PORTEES =====================================


class Scope {
    constructor(parent, start, kind) {
        this.parent = parent;
        this.start = start;
        this.end = Infinity;
        this.kind = kind;     // "file", "func", "struct" ou "block"
        this.symbols = new Map();
        this.children = [];
        if (parent) parent.children.push(this);
    }
}

// Lit les définitions d'un fichier. Chaque `{` ouvre une portée : un map écrit dans le
// code en ouvre donc une aussi, sans conséquence, et ses lfunc y trouvent leur place.
class Reader {
    constructor(text, module) {
        const { tokens, comments } = tokenize(text);
        this.tokens = tokens;
        this.comments = comments;
        this.module = module;
        this.i = 0;
        this.func = null;
    }

    get token() {
        return this.tokens[this.i];
    }

    define(scope, name, symbol) {
        const existing = scope.symbols.get(name);
        symbol.name = name;
        symbol.scope = scope;
        symbol.module = this.module;
        symbol.assigns = existing ? existing.assigns : [];
        // Une deuxième déclaration garde le type de la première, comme à l'exécution.
        if (existing && symbol.kind === "var" && existing.kind === "var") {
            symbol.typeName = symbol.typeName || existing.typeName;
            symbol.mutated = true;
        }
        scope.symbols.set(name, symbol);
        return symbol;
    }

    // Passe une expression sans rien définir : donne l'indice du jeton qui la suit.
    expressionEnd(from) {
        let depth = 0;
        for (let i = from; i < this.tokens.length; i++) {
            const token = this.tokens[i];
            if (token.type === "op" && "([{".includes(token.value)) depth += 1;
            if (token.type === "op" && ")]}".includes(token.value)) {
                if (depth === 0) return i;
                depth -= 1;
            }
            if (depth === 0 && (isOp(token, ",") || isOp(token, ";"))) return i;
        }
        return this.tokens.length;
    }

    // var a::int = 1, b = "x"     Le texte des valeurs est ensuite lu par walk : il peut contenir des lfunc.
    readVariables(scope, kind) {
        const isConst = isWord(this.token, "const");
        let i = this.i + 1;
        while (isName(this.tokens[i])) {
            const nameToken = this.tokens[i];
            const symbol = { kind, pos: nameToken.start, isConst };
            const declared = scope.symbols.get(nameToken.value);
            const definition = { token: nameToken, scope, oldType: declared ? declared.typeName : undefined };
            this.module.definitions.push(definition);
            i += 1;
            if (isOp(this.tokens[i], "::") && this.tokens[i + 1] !== undefined) {
                symbol.typeName = this.tokens[i + 1].value;
                definition.typeName = symbol.typeName;
                i += 2;
            }
            if (isOp(this.tokens[i], "=")) {
                symbol.expr = { index: i + 1, scope };
                definition.value = i + 1;
                this.module.definitionEquals.add(i);
                i = this.expressionEnd(i + 1);
            }
            this.define(scope, nameToken.value, symbol);
            if (!isOp(this.tokens[i], ",")) break;
            i += 1;
        }
    }

    readParameters(scope, func) {
        const add = () => {
            const token = this.token;
            this.i += 1;
            const symbol = { kind: "param", pos: token.start };
            if (isOp(this.token, "::")) {
                symbol.typeName = this.tokens[this.i + 1] && this.tokens[this.i + 1].value;
                this.i += 2;
            }
            this.define(scope, token.value, symbol);
            if (token.value !== "self") {
                func.params.push(token.value);
                func.paramTypes.push(symbol.typeName || null);
            }
        };

        if (isOp(this.token, "(")) {
            this.i += 1;
            while (isName(this.token)) {
                add();
                if (!isOp(this.token, ",")) break;
                this.i += 1;
            }
            if (isOp(this.token, ")")) this.i += 1;
        } else if (isOp(this.token, "->")) {
            this.i += 1;
            while (isName(this.token)) {
                add();
                if (!isOp(this.token, ",")) break;
                this.i += 1;
            }
        }
    }

    // func nom(a, b) : int { ... }   et   lfunc(a) { ... }
    readFunction(scope) {
        const isLambda = isWord(this.token, "lfunc");
        const keyword = this.token;
        this.i += 1;

        let name = null;
        if (!isLambda && isName(this.token)) {
            name = this.token.value;
            this.i += 1;
        }

        const inner = new Scope(scope, keyword.start, "func");
        const func = { params: [], paramTypes: [], returnType: null, returns: [], scope: inner, name: name || "lfunc", nameIndex: this.i - 1 };
        if (isLambda) this.module.lambdas.set(this.i - 1, func);
        this.module.functions.push(func);
        this.readParameters(inner, func);

        if (isOp(this.token, ":") && this.tokens[this.i + 1] !== undefined) {
            func.returnType = this.tokens[this.i + 1].value;
            this.i += 2;
        }

        if (name !== null) {
            const symbol = this.define(scope, name, { kind: "func", pos: keyword.start, func });
            // __init__ se surcharge : on garde la première version pour décrire les paramètres.
            if (name === "__init__") {
                if (scope.init === undefined) scope.init = symbol;
                (scope.inits = scope.inits || []).push(func);
            }
        }

        if (isOp(this.token, "{")) {
            const outerFunc = this.func;
            this.func = func;
            this.i += 1;
            this.walk(inner);
            this.func = outerFunc;
        } else {
            inner.end = this.token ? this.token.start : Infinity;
        }
    }

    readStruct(scope) {
        const keyword = this.token;
        this.i += 1;
        if (!isName(this.token)) return;

        const name = this.token.value;
        this.i += 1;

        const inner = new Scope(scope, keyword.start, "struct");
        const struct = { name, scope: inner };
        this.define(scope, name, { kind: "struct", pos: keyword.start, struct });
        this.define(inner, "self", { kind: "self", pos: keyword.start, struct });

        if (isOp(this.token, "{")) {
            this.i += 1;
            this.walk(inner);
        }
    }

    // enum Couleur { ROUGE, VERT = 5, NOM = "x" }
    readEnum(scope) {
        const keyword = this.token;
        this.i += 1;
        if (!isName(this.token)) return;

        const enumeration = { name: this.token.value, members: new Map() };
        this.define(scope, enumeration.name, { kind: "enum", pos: keyword.start, isConst: true, enumeration });
        this.i += 1;
        if (!isOp(this.token, "{")) return;
        this.i += 1;

        let next = 0;
        while (isName(this.token)) {
            const member = { number: next };
            const name = this.token.value;
            this.i += 1;
            if (isOp(this.token, "=")) {
                const negative = isOp(this.tokens[this.i + 1], "-");
                const value = this.tokens[this.i + (negative ? 2 : 1)];
                if (value && value.type === "num") member.number = negative ? -Number(value.value) : Number(value.value);
                else member.number = undefined;
                this.i += negative ? 3 : 2;
            }
            enumeration.members.set(name, member);
            next = member.number + 1;
            if (!isOp(this.token, ",")) break;
            this.i += 1;
        }

        // Du code à moitié écrit : on s'arrête au `}`, ou à la prochaine instruction s'il manque.
        const stops = ["var", "const", "func", "struct", "enum", "if", "match", "for", "while", "from", "export"];
        while (this.token !== undefined && !isOp(this.token, "}") && !stops.some((word) => isWord(this.token, word))) this.i += 1;
        if (isOp(this.token, "}")) this.i += 1;
    }

    // for var k, v in c { }   ou   for var i = 0; i < n; i += 1 { }
    readFor(scope) {
        const keyword = this.token;
        this.i += 1;
        const inner = new Scope(scope, keyword.start, "block");

        if (isWord(this.token, "var")) {
            const names = [];
            let j = this.i + 1;
            while (isName(this.tokens[j])) {
                const entry = { token: this.tokens[j] };
                j += 1;
                if (isOp(this.tokens[j], "::") && this.tokens[j + 1]) {
                    entry.typeName = this.tokens[j + 1].value;
                    j += 2;
                }
                names.push(entry);
                if (!isOp(this.tokens[j], ",")) break;
                j += 1;
            }

            if (isWord(this.tokens[j], "in")) {
                names.forEach((entry, position) => {
                    this.define(inner, entry.token.value, {
                        kind: "var",
                        pos: entry.token.start,
                        typeName: entry.typeName,
                        forIn: { index: j + 1, scope: inner, position, count: names.length },
                    });
                });
                this.i = j + 1;
            } else {
                this.readVariables(inner, "var");
            }
        }

        // L'en-tête : un `{` juste après `in` est un map, le suivant est le corps.
        while (this.token !== undefined && !isOp(this.token, "}")) {
            if (isOp(this.token, "{")) {
                if (isWord(this.tokens[this.i - 1], "in")) {
                    this.i = this.skipBalanced(this.i);
                    continue;
                }
                this.i += 1;
                this.walk(new Scope(inner, this.tokens[this.i - 1].start, "block"));
                break;
            }
            if (isWord(this.token, "lfunc")) this.readFunction(inner);
            else this.i += 1;
        }
        inner.end = this.tokens[this.i - 1] ? this.tokens[this.i - 1].end : Infinity;
    }

    skipBalanced(from) {
        let depth = 0;
        for (let i = from; i < this.tokens.length; i++) {
            if (this.tokens[i].type === "op" && "([{".includes(this.tokens[i].value)) depth += 1;
            if (this.tokens[i].type === "op" && ")]}".includes(this.tokens[i].value)) {
                depth -= 1;
                if (depth <= 0) return i + 1;
            }
        }
        return this.tokens.length;
    }

    // from a.b import *     from @"lib.so" import [f, g]     from "x.cf" import f
    readImport() {
        const entry = { native: false, path: "", names: [], star: false, pos: this.token.start };
        this.i += 1;

        if (isOp(this.token, "@")) {
            entry.native = true;
            this.i += 1;
        }
        if (this.token && this.token.type === "str") {
            entry.path = this.token.value;
            entry.isString = true;
            this.i += 1;
        } else {
            const parts = [];
            while (isName(this.token)) {
                parts.push(this.token.value);
                this.i += 1;
                if (!isOp(this.token, ".")) break;
                this.i += 1;
            }
            entry.path = parts.join("/");
        }

        if (isWord(this.token, "import")) {
            this.i += 1;
            if (isOp(this.token, "*")) {
                entry.star = true;
                this.i += 1;
            } else if (isName(this.token)) {
                entry.names.push(this.token.value);
                this.i += 1;
            } else if (isOp(this.token, "[")) {
                this.i += 1;
                while (isName(this.token)) {
                    entry.names.push(this.token.value);
                    this.i += 1;
                    if (!isOp(this.token, ",")) break;
                    this.i += 1;
                }
                if (isOp(this.token, "]")) this.i += 1;
            }
        }
        this.module.imports.push(entry);
    }

    readExport() {
        this.i += 1;
        if (isOp(this.token, "::")) this.i += 1;
        if (!isOp(this.token, "[")) return;
        this.i += 1;
        this.module.exports = new Set();
        while (isName(this.token)) {
            this.module.exports.add(this.token.value);
            this.i += 1;
            if (!isOp(this.token, ",")) break;
            this.i += 1;
        }
    }

    // a = 1, self.b = 2, t.c = 3, t["d"] = 4 : on s'en souvient pour trouver leur type.
    noteAssignment(scope) {
        const t = this.tokens;
        const i = this.i;
        if (!isName(t[i]) || isOp(t[i - 1], ".") || this.module.definitionEquals.has(i + 1)) return;

        if (t[i + 1] && t[i + 1].type === "op" && ["+=", "-=", "*=", "/=", "%="].includes(t[i + 1].value)) {
            this.module.assigns.push({ name: t[i].value, key: null, compound: true, scope });
        } else if (isOp(t[i + 1], "=")) {
            this.module.assigns.push({ name: t[i].value, key: null, expr: { index: i + 2, scope }, scope });
        } else if (isOp(t[i + 1], ".") && isName(t[i + 2]) && isOp(t[i + 3], "=")) {
            this.module.assigns.push({ name: t[i].value, key: t[i + 2].value, expr: { index: i + 4, scope }, scope });
        } else if (isOp(t[i + 1], "[") && t[i + 2] && t[i + 2].type === "str" && isOp(t[i + 3], "]") && isOp(t[i + 4], "=")) {
            this.module.assigns.push({ name: t[i].value, key: t[i + 2].value, expr: { index: i + 5, scope }, scope });
        }
    }

    // Lit jusqu'au `}` qui ferme cette portée.
    walk(scope) {
        while (this.token !== undefined) {
            const token = this.token;

            if (isOp(token, "}")) {
                scope.end = token.start;
                this.i += 1;
                return;
            }
            if (isOp(token, "{")) {
                this.i += 1;
                this.walk(new Scope(scope, token.start, "block"));
            } else if (isWord(token, "func") || isWord(token, "lfunc")) {
                this.readFunction(scope);
            } else if (isWord(token, "struct")) {
                this.readStruct(scope);
            } else if (isWord(token, "enum")) {
                this.readEnum(scope);
            } else if (isWord(token, "for")) {
                this.readFor(scope);
            } else if (isWord(token, "from")) {
                this.readImport();
            } else if (isWord(token, "export")) {
                this.readExport();
            } else if (isWord(token, "var") || isWord(token, "const")) {
                this.readVariables(scope, "var");
                this.i += 1;
            } else if (isWord(token, "return")) {
                if (this.func) this.func.returns.push({ index: this.i + 1, scope, token });
                this.i += 1;
            } else {
                this.noteAssignment(scope);
                this.i += 1;
            }
        }
    }
}


// ===================================== $MODULES =====================================


// Un cache par chemin : un fichier n'est relu que s'il a changé.
const fileCache = new Map();

class Module {
    constructor(text, filePath, root) {
        this.filePath = filePath;
        this.root = root;
        this.text = text;
        this.imports = [];
        this.exports = null;
        this.assigns = [];
        this.lambdas = new Map();
        this.definitions = [];
        this.definitionEquals = new Set();
        this.functions = [];
        this.scope = new Scope(null, 0, "file");

        const reader = new Reader(text, this);
        this.tokens = reader.tokens;
        this.comments = reader.comments;
        reader.walk(this.scope);
        // Un `}` en trop ne doit pas cacher le reste du fichier.
        while (reader.token !== undefined) {
            reader.i += 1;
            reader.walk(this.scope);
        }

        for (const assign of this.assigns) {
            const symbol = this.lookup(assign.name, assign.scope, Infinity);
            if (symbol === null) continue;
            if (assign.compound) {
                symbol.mutated = true;
            } else if (assign.key === null) {
                symbol.assigns.push(assign.expr);
            } else if (symbol.kind === "self") {
                const attribute = symbol.struct.scope.symbols.get(assign.key);
                if (attribute) attribute.assigns.push(assign.expr);
            } else {
                (symbol.keys = symbol.keys || new Map()).set(assign.key, assign.expr);
            }
        }
    }

    get directory() {
        return this.filePath ? path.dirname(this.filePath) : this.root;
    }

    // Comme l'interpréteur : à côté du fichier, puis depuis le dossier courant.
    resolvePath(entry) {
        const name = entry.isString || entry.native ? entry.path : entry.path + ".cf";
        const bases = [this.directory, this.root].filter(Boolean);
        for (const base of bases) {
            const candidate = path.resolve(base, name);
            if (fs.existsSync(candidate)) return candidate;
        }
        return null;
    }

    importedModule(entry) {
        if (entry.module !== undefined) return entry.module;
        const file = entry.native ? null : this.resolvePath(entry);
        if (file) entry.module = loadFile(file, this.root);
        else entry.module = entry.native ? null : standardModule(entry.path);
        return entry.module;
    }

    publicSymbols() {
        const result = new Map();
        for (const [name, symbol] of this.importedSymbols()) result.set(name, symbol);
        for (const [name, symbol] of this.scope.symbols) result.set(name, symbol);
        if (this.exports === null) return result;
        return new Map([...result].filter(([name]) => this.exports.has(name)));
    }

    importedSymbols() {
        if (this.imported) return this.imported;
        this.imported = new Map();

        for (const entry of this.imports) {
            if (entry.native) {
                for (const name of entry.names) {
                    this.imported.set(name, { kind: "native", name, pos: 0, module: this, assigns: [] });
                }
                continue;
            }
            const module = this.importedModule(entry);
            if (module === null) continue;
            const symbols = module.publicSymbols();
            if (entry.star) {
                for (const [name, symbol] of symbols) {
                    if (name !== "main") this.imported.set(name, symbol);
                }
            }
            for (const name of entry.names) {
                if (symbols.has(name)) this.imported.set(name, symbols.get(name));
            }
        }
        return this.imported;
    }

    scopeAt(offset) {
        let scope = this.scope;
        for (let inner = scope; inner !== null;) {
            scope = inner;
            inner = null;
            for (const child of scope.children) {
                if (child.start <= offset && offset <= child.end) inner = child;
            }
        }
        return scope;
    }

    // Avant le curseur dans la fonction courante. Au-delà d'une fonction, tout le fichier existe déjà.
    lookup(name, scope, offset) {
        let limit = offset;
        for (; scope !== null; scope = scope.parent) {
            const symbol = scope.symbols.get(name);
            if (symbol && (symbol.kind !== "var" || symbol.pos < limit)) return symbol;
            if (scope.kind === "func") limit = Infinity;
        }
        const imported = this.importedSymbols();
        return imported.has(name) ? imported.get(name) : null;
    }
}

function loadFile(file, root) {
    let stat;
    try {
        stat = fs.statSync(file);
    } catch (error) {
        return null;
    }

    const cached = fileCache.get(file);
    if (cached && cached.mtime === stat.mtimeMs) return cached.module;

    // Placé dans le cache avant la lecture : deux fichiers peuvent s'importer l'un l'autre.
    const entry = { mtime: stat.mtimeMs, module: null };
    fileCache.set(file, entry);
    try {
        entry.module = new Module(fs.readFileSync(file, "utf8"), file, root);
    } catch (error) {
        entry.module = null;
    }
    return entry.module;
}


// ===================================== $TYPES =====================================


const UNKNOWN = { kind: "unknown" };
const named = (kind) => ({ kind });
const MAX_DEPTH = 24;

// Pour proposer, un type probable suffit. Pour souligner une erreur, il faut en être sûr :
// en mode strict, tout ce qui peut changer pendant l'exécution reste inconnu.
let strict = false;

function typeOfStrictSymbol(symbol, depth) {
    const module = symbol.module;
    if (symbol.kind === "self") return { kind: "instance", struct: symbol.struct, module };
    if (symbol.kind === "func") return { kind: "func", func: symbol.func, module, scope: symbol.scope };
    if (symbol.kind === "struct") return { kind: "structure", struct: symbol.struct, module };
    if (symbol.kind === "enum") return { kind: "enum", enumeration: symbol.enumeration };
    if (symbol.kind === "native") return UNKNOWN;
    if (symbol.typeName) return typeFromName(symbol.typeName, module, symbol.scope);

    // Sans type déclaré, seule une variable jamais réaffectée garde sa première valeur.
    // Un attribut change d'un objet à l'autre : il reste inconnu.
    const isStable = symbol.kind === "var" && symbol.expr && !symbol.mutated &&
        symbol.assigns.length === 0 && symbol.scope.kind !== "struct";
    return isStable ? certainType(module, symbol.expr.index, symbol.expr.scope, depth + 1) : UNKNOWN;
}

// L'indice du jeton qui suit l'expression. Les `;` sont facultatifs : elle s'arrête quand
// deux valeurs se suivent sans opérateur, ou sur un `[` ou un `(` en début de ligne.
function expressionStop(module, from) {
    const tokens = module.tokens;
    let depth = 0;
    for (let i = from; i < tokens.length; i++) {
        const token = tokens[i];
        const previous = tokens[i - 1];
        const isOpener = token.type === "op" && "([{".includes(token.value);

        if (depth === 0 && i > from) {
            const endsValue = previous.type !== "op" ? previous.type !== "id" || !KEYWORD_SET.has(previous.value) || ["true", "false", "none"].includes(previous.value) :
                ")]}".includes(previous.value);
            const startsValue = token.type !== "op" ? !isWord(token, "and") && !isWord(token, "or") : false;
            const newLine = module.text.slice(previous.end, token.start).includes("\n");
            if (endsValue && (startsValue || isOp(token, "{") || (isOpener && newLine))) return i;
        }
        if (isOpener) depth += 1;
        if (token.type === "op" && ")]}".includes(token.value)) {
            if (depth === 0) return i;
            depth -= 1;
        }
        if (depth === 0 && token.type === "op" && [",", ";", "=", "+=", "-=", "*=", "/=", "%=", ":"].includes(token.value)) return i;
    }
    return tokens.length;
}

function certainType(module, index, scope, depth = 0) {
    if (depth > MAX_DEPTH) return UNKNOWN;
    const tokens = module.tokens;
    const end = expressionStop(module, index);

    // Le signe de plus faible priorité décide : `1 + 2 < 3` est un bool.
    let inner = 0;
    let verdict = null;
    for (let i = index; i < end; i++) {
        const token = tokens[i];
        if (token.type === "op" && "([{".includes(token.value)) inner += 1;
        if (token.type === "op" && ")]}".includes(token.value)) inner -= 1;
        if (inner !== 0) continue;
        if (isOp(token, "?")) return UNKNOWN;
        if (isWord(token, "and") || isWord(token, "or")) verdict = named("bool");
        if (token.type === "op" && ["==", "!=", "<", "<=", ">", ">="].includes(token.value)) verdict = named("bool");
    }
    return verdict || typeOfExpression(module, index, scope, depth + 1, end);
}

function typeFromName(name, module, scope) {
    if (name === undefined || name === null) return UNKNOWN;
    if (name === "int" || name === "float" || name === "number") return { kind: "number", display: name };
    if (name === "str" || name === "string") return named("str");
    if (name === "list" || name === "map" || name === "bool" || name === "char") return named(name);

    const symbol = module.lookup(name, scope, Infinity);
    if (symbol && symbol.kind === "enum") return { kind: "enumItem", enumeration: symbol.enumeration };
    return symbol && symbol.kind === "struct" ? { kind: "instance", struct: symbol.struct, module: symbol.module } : UNKNOWN;
}

function typeOfSymbol(symbol, depth) {
    if (depth > MAX_DEPTH) return UNKNOWN;
    if (strict) return typeOfStrictSymbol(symbol, depth);
    const module = symbol.module;

    if (symbol.kind === "self") return { kind: "instance", struct: symbol.struct, module };
    if (symbol.kind === "func" || symbol.kind === "native") return { kind: "func", func: symbol.func, module, scope: symbol.scope };
    if (symbol.kind === "struct") return { kind: "structure", struct: symbol.struct, module };
    if (symbol.kind === "enum") return { kind: "enum", enumeration: symbol.enumeration };

    let type = UNKNOWN;
    if (symbol.typeName) type = typeFromName(symbol.typeName, module, symbol.scope);
    if (type === UNKNOWN && symbol.forIn) type = typeOfForIn(symbol, module, depth);
    if (type === UNKNOWN && symbol.expr) type = typeOfExpression(module, symbol.expr.index, symbol.expr.scope, depth + 1);

    // `var Job` puis `self.Job = new Metier()` : la première valeur connue donne le type.
    for (let i = 0; (type === UNKNOWN || type.kind === "none") && i < symbol.assigns.length; i++) {
        const assign = symbol.assigns[i];
        type = typeOfExpression(symbol.module, assign.index, assign.scope, depth + 1);
    }

    // Les clés ajoutées avec t.x = ... complètent celles du map.
    if ((type === UNKNOWN || type.kind === "map") && symbol.keys) {
        const keys = new Map(type.keys || []);
        for (const [key, expr] of symbol.keys) {
            if (!keys.has(key)) keys.set(key, { module, ...expr });
        }
        return { kind: "map", keys };
    }
    return type;
}

function typeOfForIn(symbol, module, depth) {
    const { index, scope, position, count } = symbol.forIn;
    const iterated = typeOfExpression(module, index, scope, depth + 1);

    if (iterated.kind === "map") return position === 0 ? named("str") : UNKNOWN;
    if (count === 2 && position === 0) return named("number");
    if (iterated.kind === "list") return elementOf(iterated, depth);
    if (iterated.kind === "str") return named("char");
    return UNKNOWN;
}

function elementOf(list, depth) {
    if (strict) return UNKNOWN;
    if (list.element) return list.element;
    if (!list.elementExpr) return UNKNOWN;
    const { module, index, scope } = list.elementExpr;
    return typeOfExpression(module, index, scope, depth + 1);
}

function typeOfMapLiteral(module, index, scope) {
    const tokens = module.tokens;
    const keys = new Map();
    let i = index + 1;

    const next = (from) => {
        let depth = 0;
        for (let j = from; j < tokens.length; j++) {
            if (tokens[j].type !== "op") continue;
            if ("([{".includes(tokens[j].value)) depth += 1;
            if (")]}".includes(tokens[j].value)) {
                if (depth === 0) return j;
                depth -= 1;
            }
            if (depth === 0 && tokens[j].value === ",") return j;
        }
        return tokens.length;
    };

    while (i < tokens.length && !isOp(tokens[i], "}")) {
        const key = tokens[i];
        if ((key.type === "id" || key.type === "str") && isOp(tokens[i + 1], ":")) {
            keys.set(key.value, { module, index: i + 2, scope });
        } else if (key.type === "id" && (isOp(tokens[i + 1], ",") || isOp(tokens[i + 1], "}"))) {
            keys.set(key.value, { module, index: i, scope });
        }
        const end = next(i);
        if (!isOp(tokens[end], ",")) break;
        i = end + 1;
    }
    return { kind: "map", keys };
}

// Le type de ce qui commence au jeton `index`. `stop` : ne pas lire au-delà (le curseur).
function typeOfExpression(module, index, scope, depth = 0, stop = Infinity) {
    if (depth > MAX_DEPTH) return UNKNOWN;
    const tokens = module.tokens;
    const token = tokens[index];
    if (token === undefined || index >= stop) return UNKNOWN;

    let type = UNKNOWN;
    let i = index + 1;

    if (token.type === "num") type = { kind: "number", literal: Number(token.value) };
    else if (token.type === "str") type = named("str");
    else if (token.type === "chr") type = named("char");
    else if (isWord(token, "true") || isWord(token, "false")) type = named("bool");
    else if (isWord(token, "none")) type = named("none");
    else if (strict && (isOp(token, "!") || isOp(token, "-"))) return UNKNOWN;
    else if (isOp(token, "!")) return named("bool");
    else if (isOp(token, "-")) return named("number");
    else if (isOp(token, "[")) {
        type = isOp(tokens[i], "]") ? named("list") : { kind: "list", elementExpr: { module, index: i, scope } };
        i = closingOf(tokens, index, stop);
    } else if (isOp(token, "{")) {
        type = typeOfMapLiteral(module, index, scope);
        i = closingOf(tokens, index, stop);
    } else if (isOp(token, "(")) {
        type = strict ? certainType(module, i, scope, depth + 1) : typeOfExpression(module, i, scope, depth + 1, stop);
        i = closingOf(tokens, index, stop);
    } else if (isWord(token, "new") || isOp(token, "@")) {
        if (tokens[i] !== undefined) {
            type = typeFromName(tokens[i].value, module, scope);
            i += 1;
        }
        if (isOp(tokens[i], "(")) i = closingOf(tokens, i, stop);
    } else if (isWord(token, "lfunc")) {
        return { kind: "func", func: module.lambdas.get(index), module, scope };
    } else if (isName(token)) {
        const symbol = module.lookup(token.value, scope, Infinity);
        if (symbol !== null) type = typeOfSymbol(symbol, depth + 1);
        if (BUILTINS[token.value] && symbol === null) type = { kind: "builtin", name: token.value };
    } else {
        return UNKNOWN;
    }

    // Les suites : .nom  [index]  (arguments)
    while (i < stop && tokens[i] !== undefined) {
        if (isOp(tokens[i], ".") && tokens[i + 1] !== undefined && i + 1 < stop) {
            const member = tokens[i + 1].value;
            if (isOp(tokens[i + 2], "(") && i + 2 < stop && !memberIsFunctionSymbol(type, member)) {
                type = returnOfMethod(type, member, depth);
                i = closingOf(tokens, i + 2, stop);
            } else {
                type = typeOfMember(type, member, depth);
                i += 2;
            }
        } else if (isOp(tokens[i], "[") && !newlineBetween(module, i)) {
            const key = tokens[i + 1];
            if (type.kind === "list") type = elementOf(type, depth);
            else if (type.kind === "str") type = named("char");
            else if (type.kind === "map" && key && key.type === "str") type = typeOfMember(type, key.value, depth);
            else type = UNKNOWN;
            i = closingOf(tokens, i, stop);
        } else if (isOp(tokens[i], "(") && !newlineBetween(module, i)) {
            type = returnOfCall(type, depth);
            i = closingOf(tokens, i, stop);
        } else {
            break;
        }
    }

    // Une opération change le type : "a" + 1 est une chaîne, 1 < 2 un bool.
    const after = tokens[i];
    if (i < stop && after !== undefined && after.type === "op") {
        if (["==", "!=", "<", "<=", ">", ">="].includes(after.value)) return named("bool");
        if (strict && ["+", "-", "*", "/", "%"].includes(after.value)) return strictArithmetic(type, after.value, module, i + 1, scope, depth, stop);
        if (after.value === "?") return typeOfExpression(module, i + 1, scope, depth + 1, stop);
        if (["-", "*", "/", "%"].includes(after.value)) return type.kind === "str" ? type : named("number");
        if (after.value === "+") {
            const right = typeOfExpression(module, i + 1, scope, depth + 1, stop);
            if (type.kind === "str" || right.kind === "str") return named("str");
            if (type.kind === "list") return type;
            return named("number");
        }
    }
    if (isWord(after, "and") || isWord(after, "or")) return named("bool");
    return type;
}

function strictArithmetic(left, operator, module, index, scope, depth, stop) {
    const right = typeOfExpression(module, index, scope, depth + 1, stop);
    const isNumeric = (type) => type.kind === "number" || type.kind === "bool" || type.kind === "char";

    if (operator === "+" && (left.kind === "str" || right.kind === "str")) return named("str");
    if (left === UNKNOWN || right === UNKNOWN) return UNKNOWN;
    return isNumeric(left) && isNumeric(right) ? named("number") : UNKNOWN;
}

function newlineBetween(module, index) {
    const before = module.tokens[index - 1];
    return before !== undefined && module.text.slice(before.end, module.tokens[index].start).includes("\n");
}

function closingOf(tokens, index, stop) {
    let depth = 0;
    for (let i = index; i < tokens.length && i < stop; i++) {
        if (tokens[i].type !== "op") continue;
        if ("([{".includes(tokens[i].value)) depth += 1;
        if (")]}".includes(tokens[i].value)) {
            depth -= 1;
            if (depth === 0) return i + 1;
        }
    }
    return Math.min(stop, tokens.length);
}

function memberIsFunctionSymbol(type, member) {
    if (type.kind === "instance") return type.struct.scope.symbols.has(member);
    return type.kind === "map" && type.keys !== undefined && type.keys.has(member);
}

function typeOfMember(type, member, depth) {
    if (type.kind === "instance") {
        const symbol = type.struct.scope.symbols.get(member);
        return symbol ? typeOfSymbol(symbol, depth + 1) : UNKNOWN;
    }
    if (type.kind === "enum") {
        const item = type.enumeration.members.get(member);
        return item ? { kind: "enumItem", enumeration: type.enumeration, member: item } : UNKNOWN;
    }
    if (type.kind === "enumItem") {
        if (member === "name") return named("str");
        if (member !== "value" || !type.member) return UNKNOWN;
        return type.member.number !== undefined ? { kind: "number", literal: type.member.number } : named("str");
    }
    if (strict) return UNKNOWN;
    if (type.kind === "map" && type.keys && type.keys.has(member)) {
        const { module, index, scope } = type.keys.get(member);
        return typeOfExpression(module, index, scope, depth + 1);
    }
    return UNKNOWN;
}

function returnOfMethod(type, name, depth) {
    const table = METHODS[type.kind];
    if (table === undefined || table[name] === undefined) return UNKNOWN;

    const returns = table[name].returns;
    if (returns === "self") return type;
    if (returns === "element") return elementOf(type, depth);
    if (returns === "list_str") return { kind: "list", element: named("str") };
    if (returns === "list_items") return { kind: "list", element: { kind: "enumItem", enumeration: type.enumeration } };
    if (name === "keys") return { kind: "list", element: named("str") };
    return returns ? named(returns) : UNKNOWN;
}

function returnOfCall(type, depth) {
    if (type.kind === "builtin") {
        return BUILTINS[type.name].returns ? named(BUILTINS[type.name].returns) : UNKNOWN;
    }
    if (type.kind !== "func" || !type.func) return UNKNOWN;
    if (type.func.returnType) return typeFromName(type.func.returnType, type.module, type.scope);
    if (strict) return UNKNOWN;

    for (const result of type.func.returns) {
        const found = typeOfExpression(type.module, result.index, result.scope, depth + 1);
        if (found !== UNKNOWN) return found;
    }
    return UNKNOWN;
}


// ===================================== $PROPOSITIONS =================================


function functionItem(label, kind, params, doc, inList = false) {
    const signature = `${label}(${params.join(", ")})`;
    return {
        label,
        kind,
        detail: signature,
        doc,
        snippet: inList ? null : params.length > 0 ? `${label}($0)` : `${label}()`,
    };
}

function typeLabel(type) {
    if (type.kind === "instance") return type.struct.name;
    if (type.kind === "enumItem") return type.enumeration.name;
    return type === UNKNOWN ? null : type.kind;
}

function describeType(symbol) {
    return symbol.typeName || typeLabel(typeOfSymbol(symbol, 0));
}

function itemFromSymbol(name, symbol, inList = false) {
    if (symbol.kind === "func") {
        return functionItem(name, "function", symbol.func.params, symbol.func.doc, inList);
    }
    if (symbol.kind === "native") return { label: name, kind: "function", detail: "fonction native" };
    if (symbol.kind === "struct") {
        const init = symbol.struct.scope.init;
        const params = init ? init.func.params.join(", ") : "";
        return { label: name, kind: "struct", detail: `struct ${name}(${params})` };
    }
    if (symbol.kind === "enum") return { label: name, kind: "enum", detail: `enum ${name}` };
    const type = describeType(symbol);
    return { label: name, kind: "variable", detail: type ? `${name} : ${type}` : name };
}

function structMembers(struct) {
    const items = [];
    for (const [name, symbol] of struct.scope.symbols) {
        if (name === "self" || name.startsWith("__")) continue;
        if (symbol.kind === "func") {
            items.push(functionItem(name, "method", symbol.func.params));
        } else {
            const type = describeType(symbol);
            items.push({ label: name, kind: "field", detail: type ? `${name} : ${type}` : name });
        }
    }
    return items;
}

function enumMembers(enumeration) {
    return [...enumeration.members.keys()].map((name) => ({
        label: name,
        kind: "enumMember",
        detail: `${enumeration.name}.${name}`,
        sort: "0",
    }));
}

function builtinMethods(kind) {
    return Object.entries(METHODS[kind]).map(([name, method]) => functionItem(name, "method", method.params, method.doc));
}

function allStructs(module) {
    const found = [];
    const visit = (scope) => {
        for (const symbol of scope.symbols.values()) {
            if (symbol.kind === "struct") found.push(symbol.struct);
        }
        scope.children.forEach(visit);
    };
    visit(module.scope);
    for (const symbol of module.importedSymbols().values()) {
        if (symbol.kind === "struct") found.push(symbol.struct);
    }
    return found;
}

function membersOfType(type, module) {
    if (type.kind === "instance") return structMembers(type.struct);
    if (type.kind === "list" || type.kind === "str") return builtinMethods(type.kind);
    if (type.kind === "enum") return enumMembers(type.enumeration).concat(builtinMethods("enum"));
    if (type.kind === "enumItem") {
        return [
            { label: "name", kind: "field", detail: "name : str", doc: "Le nom de l'élément." },
            { label: "value", kind: "field", detail: "value", doc: "La valeur de l'élément." },
        ];
    }
    if (type.kind === "map") {
        const keys = [...(type.keys ? type.keys.keys() : [])]
            .filter((key) => /^[A-Za-z_][A-Za-z0-9_]*$/.test(key) && !KEYWORD_SET.has(key))
            .map((key) => {
                const { module: owner, index, scope } = type.keys.get(key);
                const found = typeOfExpression(owner, index, scope, 1);
                if (found.kind === "func" && found.func) return functionItem(key, "method", found.func.params);
                const name = typeLabel(found);
                return { label: key, kind: "field", detail: name ? `${key} : ${name}` : key, sort: "0" };
            });
        return keys.concat(builtinMethods("map"));
    }
    if (type !== UNKNOWN) return [];

    // Le type n'est pas connu, comme `p` dans `func f(p) { p. }` : tout ce qui pourrait convenir.
    const items = new Map();
    for (const struct of allStructs(module)) {
        for (const item of structMembers(struct)) {
            item.detail = `${struct.name}.${item.detail}`;
            items.set(item.label, item);
        }
    }
    for (const kind of ["list", "map", "str"]) {
        for (const item of builtinMethods(kind)) {
            if (!items.has(item.label)) items.set(item.label, { ...item, sort: "z" });
        }
    }
    return [...items.values()];
}

// Recule depuis le `.` jusqu'au début de `a.b(c)[d]`.
function chainStart(tokens, end) {
    let i = end - 1;
    while (i >= 0) {
        const token = tokens[i];
        if (isOp(token, ")") || isOp(token, "]") || isOp(token, "}")) {
            let depth = 0;
            for (; i >= 0; i--) {
                if (tokens[i].type !== "op") continue;
                if (")]}".includes(tokens[i].value)) depth += 1;
                if ("([{".includes(tokens[i].value)) {
                    depth -= 1;
                    if (depth === 0) break;
                }
            }
            if (i < 0) return 0;
            // `(a)` et `[1, 2]` commencent ici, sinon ce qui précède est appelé ou indexé.
            const before = tokens[i - 1];
            if (before === undefined || !(before.type === "id" || isOp(before, ")") || isOp(before, "]"))) return i;
            if (before.type === "id" && KEYWORD_SET.has(before.value)) return i;
            i -= 1;
        } else if (token.type === "id" || token.type === "str" || token.type === "num" || token.type === "chr") {
            if (isOp(tokens[i - 1], ".")) {
                i -= 2;
            } else if (isWord(tokens[i - 1], "new") || isOp(tokens[i - 1], "@")) {
                return i - 1;
            } else {
                return i;
            }
        } else {
            return i + 1;
        }
    }
    return 0;
}

// Les morceaux de `match sujet { case a, b { } else { } }`. null si ce n'est pas encore écrit.
function matchParts(module, index) {
    const tokens = module.tokens;
    const open = expressionStop(module, index + 1);
    if (open === index + 1 || !isOp(tokens[open], "{")) return null;

    const parts = { open, values: [], hasElse: false, closed: false };
    let i = open + 1;
    while (i < tokens.length) {
        if (isOp(tokens[i], "}")) {
            parts.closed = true;
            return parts;
        }
        if (isWord(tokens[i], "else")) {
            parts.hasElse = true;
            i = isOp(tokens[i + 1], "{") ? closingOf(tokens, i + 1, Infinity) : i + 1;
        } else if (isWord(tokens[i], "case")) {
            let start = i + 1;
            let stop = expressionStop(module, start);
            parts.values.push({ start, stop });
            while (isOp(tokens[stop], ",")) {
                start = stop + 1;
                stop = expressionStop(module, start);
                parts.values.push({ start, stop });
            }
            i = isOp(tokens[stop], "{") ? closingOf(tokens, stop, Infinity) : stop;
        } else {
            i += 1;
        }
    }
    return parts;
}

// Le `match` dont fait partie le `case` à cet indice : on remonte en sautant les blocs déjà fermés.
function matchOfCase(tokens, caseIndex) {
    let depth = 0;
    for (let i = caseIndex - 1; i >= 0; i--) {
        if (tokens[i].type !== "op") continue;
        if ("}])".includes(tokens[i].value)) depth += 1;
        if ("{[(".includes(tokens[i].value)) {
            if (depth === 0) {
                for (let j = i - 1; j >= 0 && !isOp(tokens[j], ";"); j--) {
                    if (isWord(tokens[j], "match")) return j;
                }
                return -1;
            }
            depth -= 1;
        }
    }
    return -1;
}

// Les éléments déjà écrits dans ce `match`, comme `C.A` : leur nom dans l'énumération.
function coveredMembers(module, parts, enumeration, scope) {
    const covered = new Set();
    for (const value of parts.values) {
        const type = typeOfExpression(module, value.start, scope, 0, value.stop);
        if (type.kind === "enumItem" && type.member && type.enumeration.name === enumeration.name) {
            covered.add(module.tokens[value.stop - 1].value);
        }
    }
    return covered;
}

// case |   et   case a, |     Les éléments que ce match ne traite pas encore.
function caseCompletions(module, last, scope) {
    const tokens = module.tokens;
    let caseIndex = last;
    while (caseIndex >= 0 && !isWord(tokens[caseIndex], "case")) {
        if (!(isName(tokens[caseIndex]) || isOp(tokens[caseIndex], ".") || isOp(tokens[caseIndex], ","))) return null;
        caseIndex -= 1;
    }
    if (caseIndex < 0) return null;

    const matchIndex = matchOfCase(tokens, caseIndex);
    if (matchIndex < 0) return null;

    const parts = matchParts(module, matchIndex);
    if (parts === null) return null;

    const subject = typeOfExpression(module, matchIndex + 1, scope, 0, parts.open);
    if (subject.kind !== "enumItem") return null;

    const covered = coveredMembers(module, parts, subject.enumeration, scope);
    return [...subject.enumeration.members.keys()]
        .filter((name) => !covered.has(name))
        .map((name) => ({
            label: `${subject.enumeration.name}.${name}`,
            kind: "enumMember",
            detail: subject.enumeration.name,
            sort: "0",
        }));
}

function typeCompletions(module, scope) {
    const items = TYPES.map((name) => ({ label: name, kind: "type", detail: "type" }));
    const enums = visibleSymbols(module, scope, Infinity)
        .filter(([, symbol]) => symbol.kind === "enum")
        .map(([name, symbol]) => itemFromSymbol(name, symbol));
    return items.concat(structCompletions(module, scope), enums);
}

function structCompletions(module, scope) {
    return visibleSymbols(module, scope, Infinity)
        .filter(([, symbol]) => symbol.kind === "struct")
        .map(([name, symbol]) => itemFromSymbol(name, symbol));
}

function visibleSymbols(module, scope, offset) {
    const seen = new Map();
    let limit = offset;
    for (let s = scope; s !== null; s = s.parent) {
        for (const [name, symbol] of s.symbols) {
            if (seen.has(name) || (symbol.kind === "var" && symbol.pos >= limit)) continue;
            if (name.startsWith("__") && s.kind === "struct") continue;
            seen.set(name, symbol);
        }
        if (s.kind === "func") limit = Infinity;
    }
    for (const [name, symbol] of module.importedSymbols()) {
        if (!seen.has(name)) seen.set(name, symbol);
    }
    return [...seen];
}

function generalCompletions(module, scope, offset) {
    const items = visibleSymbols(module, scope, offset).map(([name, symbol]) => ({ ...itemFromSymbol(name, symbol), sort: "0" }));
    const names = new Set(items.map((item) => item.label));

    for (const [name, builtin] of Object.entries(BUILTINS)) {
        if (!names.has(name)) items.push({ ...functionItem(name, "function", builtin.params, builtin.doc), sort: "1" });
    }
    if (!names.has("argv")) {
        items.push({ label: "argv", kind: "variable", detail: "argv : list", doc: "Les arguments du script. `argv[0]` est son nom.", sort: "1" });
    }
    for (const keyword of KEYWORDS) {
        items.push({ label: keyword, kind: "keyword", sort: "2" });
    }
    return items;
}

function nativeSymbols(file) {
    try {
        const output = execFileSync("nm", ["-D", "--defined-only", file], { encoding: "utf8", timeout: 2000 });
        return output.split("\n")
            .map((line) => line.trim().split(/\s+/))
            .filter((parts) => parts.length === 3 && parts[1] === "T")
            .map((parts) => parts[2]);
    } catch (error) {
        return [];
    }
}

// Les fichiers d'un dossier que `from` peut importer.
function pathCompletions(module, folder, extension, withExtension) {
    const items = new Map();
    for (const base of [module.directory, module.root].filter(Boolean)) {
        let entries;
        try {
            entries = fs.readdirSync(path.resolve(base, folder), { withFileTypes: true });
        } catch (error) {
            continue;
        }
        for (const entry of entries) {
            if (entry.name.startsWith(".")) continue;
            if (entry.isDirectory()) {
                items.set(entry.name, { label: entry.name, kind: "folder", detail: "dossier", sort: "1" });
            } else if (entry.name.endsWith(extension)) {
                const full = path.resolve(base, folder, entry.name);
                if (module.filePath && full === path.resolve(module.filePath)) continue;
                const label = withExtension ? entry.name : entry.name.slice(0, -extension.length);
                items.set(label, { label, kind: "file", detail: extension === ".so" ? "bibliothèque native" : "module", sort: "0" });
            }
        }
    }
    return [...items.values()];
}

function importedNames(module, entry, already) {
    if (entry.native) {
        const file = module.resolvePath(entry);
        if (file === null) return [];
        return nativeSymbols(file)
            .filter((name) => !already.includes(name))
            .map((name) => ({ label: name, kind: "function", detail: "fonction native" }));
    }

    const imported = module.importedModule(entry);
    if (imported === null) return [];
    return [...imported.publicSymbols()]
        .filter(([name]) => !already.includes(name) && name !== "main")
        .map(([name, symbol]) => itemFromSymbol(name, symbol, true));
}

// Le curseur est-il entre les `{ }` d'un enum ? Les noms y sont à choisir par l'auteur.
function insideEnum(tokens, index) {
    for (let i = index; i >= 0; i--) {
        const token = tokens[i];
        if (isOp(token, "{")) return isName(tokens[i - 1]) && isWord(tokens[i - 2], "enum");
        if (!(isName(token) || token.type === "num" || token.type === "str" || isOp(token, ",") || isOp(token, "=") || isOp(token, "-"))) return false;
    }
    return false;
}

// Le curseur est-il dans un `from ... import ...` ? Recule en lisant ce qu'un import peut contenir.
function importContext(tokens, index) {
    let sawImport = false;
    const names = [];

    for (let i = index; i >= 0; i--) {
        const token = tokens[i];
        if (isWord(token, "from")) {
            return { from: i, sawImport, names };
        }
        if (isWord(token, "import")) {
            sawImport = true;
        } else if (isName(token)) {
            if (sawImport === false) names.push(token.value);
        } else if (!(token.type === "str" || isOp(token, ".") || isOp(token, ",") || isOp(token, "[") || isOp(token, "@"))) {
            return null;
        }
    }
    return null;
}

/**
 * @brief ce que l'éditeur peut proposer à cette position du texte
 * @param trigger le caractère qui a ouvert la liste, s'il y en a un
 */
function complete(text, offset, options = {}) {
    const root = options.root || null;
    const filePath = options.filePath || null;
    const trigger = options.trigger;

    const module = new Module(text, filePath, root);
    const tokens = module.tokens;
    const scope = module.scopeAt(offset);

    if (module.comments.some(([start, end]) => start < offset && offset <= end)) return [];

    // Le mot en train d'être écrit ne compte pas : `p.no|` se complète comme `p.|`.
    let wordStart = offset;
    while (wordStart > 0 && /[A-Za-z0-9_]/.test(text[wordStart - 1])) wordStart -= 1;

    let last = -1;
    const isOpenText = (token) => (token.type === "str" || token.type === "chr") && !token.closed;
    while (last + 1 < tokens.length && tokens[last + 1].end <= wordStart && !isOpenText(tokens[last + 1])) last += 1;
    const next = tokens[last + 1];
    const inText = next !== undefined && (next.type === "str" || next.type === "chr") &&
        next.start < offset && (offset < next.end || !next.closed);

    if (inText && next.type === "chr") return [];

    // Dans une chaîne
    if (inText) {
        const before = tokens[last];
        const beforeBefore = tokens[last - 1];

        // t["clé
        if (isOp(before, "[") && !isWord(beforeBefore, "import")) {
            const start = chainStart(tokens, last);
            const type = typeOfExpression(module, start, scope, 0, last);
            if (type.kind === "map" && type.keys) {
                return [...type.keys.keys()].map((key) => ({ label: key, kind: "field" }));
            }
            return [];
        }

        // from "dossier/fic   ou   from @"dossier/lib.so
        if (isWord(before, "from") || (isOp(before, "@") && isWord(beforeBefore, "from"))) {
            const written = text.slice(next.start + 1, offset);
            const folder = written.includes("/") ? written.slice(0, written.lastIndexOf("/") + 1) : ".";
            return pathCompletions(module, folder, isOp(before, "@") ? ".so" : ".cf", true);
        }
        return [];
    }

    // Dans un import
    const context = importContext(tokens, last);
    if (context !== null) {
        const fromIndex = context.from;
        if (!context.sawImport) {
            // from a.b.|     On propose `import` seulement si le chemin est complet.
            const folder = [...context.names].reverse().join("/");
            if (isOp(tokens[last], ".")) return pathCompletions(module, folder, ".cf", false);
            if (isWord(tokens[last], "from")) {
                const files = pathCompletions(module, ".", ".cf", false);
                const standard = Object.keys(STANDARD_MODULES)
                    .filter((name) => !files.some((item) => item.label === name))
                    .map((name) => ({ label: name, kind: "file", detail: "module fourni par CeFlo", sort: "0" }));
                return standard.concat(files);
            }
            return [{ label: "import", kind: "keyword" }];
        }

        const reader = new Reader(text.slice(tokens[fromIndex].start, wordStart), module);
        const imports = module.imports;
        const saved = imports.length;
        reader.readImport();
        const entry = imports[saved];
        imports.length = saved;
        return importedNames(module, entry, entry.names);
    }

    // export :: [
    const exportIndex = [last, last - 1, last - 2].find((i) => isWord(tokens[i], "export"));
    if (exportIndex !== undefined && (isOp(tokens[last], "[") || isOp(tokens[last], ","))) {
        return [...module.scope.symbols].map(([name, symbol]) => itemFromSymbol(name, symbol, true));
    }

    const previous = tokens[last];

    // p.
    if (isOp(previous, ".")) {
        const start = chainStart(tokens, last);
        const type = typeOfExpression(module, start, scope, 0, last);
        return membersOfType(type, module);
    }

    if (isWord(previous, "new")) return structCompletions(module, scope);
    if (isOp(previous, "@")) return typeCompletions(module, scope);
    if (isOp(previous, "::")) return typeCompletions(module, scope);

    // func f() : int    Mais pas le `:` d'une condition ou d'un map.
    if (isOp(previous, ":") && (isOp(tokens[last - 1], ")") || isName(tokens[last - 1]))) {
        for (let i = last - 1; i >= 0 && !isOp(tokens[i], "{") && !isOp(tokens[i], "}") && !isOp(tokens[i], ";"); i--) {
            if (isWord(tokens[i], "func") || isWord(tokens[i], "lfunc")) return typeCompletions(module, scope);
        }
    }

    // Le nom d'une nouvelle définition : c'est à l'auteur de le choisir.
    if (isWord(previous, "var") || isWord(previous, "const") || isWord(previous, "func") || isWord(previous, "struct") || isWord(previous, "enum")) return [];
    if (insideEnum(tokens, last)) return [];

    if (isWord(previous, "case") || isOp(previous, ",")) {
        const items = caseCompletions(module, last, scope);
        if (items !== null) return items;
    }

    // Ces caractères n'ouvrent une liste que dans les cas vus plus haut.
    if (trigger !== undefined && trigger !== null && trigger !== ".") return [];

    return generalCompletions(module, scope, wordStart);
}


// ===================================== $ERREURS =====================================


// Le nom d'un type dans un message, avec les mêmes mots que l'interpréteur.
function typeNameForMessage(type) {
    if (type.kind === "instance") return type.struct.name;
    if (type.kind === "enumItem") return type.enumeration.name;
    if (type.kind === "structure") return "struct";
    if (type.kind === "builtin") return "func";
    if (type.display) return type.display;
    if (type.kind === "number" && type.literal !== undefined) return Number.isInteger(type.literal) ? "int" : "float";
    return type.kind;
}

// Faux seulement si la valeur est sûrement refusée : `x::int` accepte tout nombre dont on ne sait rien.
function isRefused(typeName, value, module, scope) {
    if (value === UNKNOWN) return false;

    const target = typeFromName(typeName, module, scope);
    if (target === UNKNOWN) return false;

    if (target.kind === "instance") {
        if (value.kind === "none") return false;
        return value.kind !== "instance" || value.struct.name !== target.struct.name;
    }
    if (target.kind === "enumItem") {
        if (value.kind === "none") return false;
        return value.kind !== "enumItem" || value.enumeration.name !== target.enumeration.name;
    }
    if (value.kind !== target.kind) return true;
    return typeName === "int" && value.literal !== undefined && !Number.isInteger(value.literal);
}

function diagnose(text, options = {}) {
    const module = new Module(text, options.filePath || null, options.root || null);
    const tokens = module.tokens;
    const errors = [];

    const report = (message, first, last = first) => {
        errors.push({ message, start: tokens[first].start, end: tokens[Math.max(first, last)].end });
    };
    const lastOf = (index) => Math.max(index, expressionStop(module, index) - 1);
    const scopeOf = (index) => module.scopeAt(tokens[index].start);

    const checkValue = (typeName, index, scope, describe) => {
        if (!typeName || tokens[index] === undefined) return;
        const value = certainType(module, index, scope);
        if (isRefused(typeName, value, module, scope)) report(describe(typeNameForMessage(value)), index, lastOf(index));
    };

    strict = true;
    try {
        // var x::int = "a"
        for (const definition of module.definitions) {
            const name = definition.token.value;
            const index = tokens.indexOf(definition.token);
            if (definition.oldType && definition.typeName && definition.oldType !== definition.typeName) {
                report(`\`${name}\` est déjà de type ${definition.oldType}`, index);
            } else if (definition.value !== undefined) {
                checkValue(definition.oldType || definition.typeName, definition.value, definition.scope,
                    (found) => `\`${name}\` est de type ${definition.oldType || definition.typeName}, pas ${found}`);
            }
        }

        // func f() : str { return 1 }
        for (const func of module.functions) {
            if (!func.returnType || func.returnType === "none") continue;
            const describe = (found) => `\`${func.name}\` doit retourner une valeur de type ${func.returnType}, pas ${found}`;
            const refusesNone = isRefused(func.returnType, named("none"), module, func.scope);

            // Le corps est fini et ne contient aucun `return` : la fonction retourne toujours none.
            if (func.returns.length === 0 && func.scope.end !== Infinity && refusesNone) {
                report(describe("none"), func.nameIndex);
            }
            for (const result of func.returns) {
                const value = tokens[result.index];
                const isBare = value === undefined || isOp(value, "}") || isOp(value, ";") ||
                    text.slice(tokens[result.index - 1].end, value.start).includes("\n");
                if (!isBare) checkValue(func.returnType, result.index, result.scope, describe);
                else if (refusesNone) report(describe("none"), result.index - 1);
            }
        }

        for (let i = 0; i < tokens.length; i++) {
            if (importContext(tokens, i) !== null) continue;
            if (isWord(tokens[i], "match")) checkMatch(module, i, errors);
            else if (isOp(tokens[i], ".")) checkMember(module, i, report);
            else if (isOp(tokens[i], "(")) checkCall(module, i, report, checkValue, scopeOf);
            else if (tokens[i].type === "op" && !module.definitionEquals.has(i) &&
                     ["=", "+="].includes(tokens[i].value)) checkAssignment(module, i, report, checkValue, scopeOf);
        }
    } finally {
        strict = false;
    }

    return errors.sort((a, b) => a.start - b.start);
}

// match c { case C.A { } }   : C.B n'est pas traité. Un avertissement et non une erreur :
// cette valeur n'arrivera peut-être jamais à l'exécution. Une valeur inconnue pourrait
// couvrir le reste : on ne dit alors rien.
function checkMatch(module, index, errors) {
    const tokens = module.tokens;
    const parts = matchParts(module, index);
    if (parts === null || !parts.closed || parts.hasElse) return;

    const scope = module.scopeAt(tokens[index].start);
    const subject = typeOfExpression(module, index + 1, scope, 0, parts.open);
    let enumeration = subject.kind === "enumItem" ? subject.enumeration : null;

    for (const value of parts.values) {
        const type = typeOfExpression(module, value.start, scope, 0, value.stop);
        if (type.kind === "none") continue;
        if (type.kind !== "enumItem" || !type.member) return;
        if (enumeration === null) enumeration = type.enumeration;
    }
    if (enumeration === null) return;

    const covered = coveredMembers(module, parts, enumeration, scope);
    const missing = [...enumeration.members.keys()].filter((name) => !covered.has(name));
    if (missing.length === 0) return;

    errors.push({
        message: `ce match ne traite pas ${missing.map((name) => `${enumeration.name}.${name}`).join(", ")}`,
        start: tokens[index].start,
        end: tokens[index].end,
        severity: "warning",
    });
}

// p.x   : le type de p est sûr, et il n'a pas de x
function checkMember(module, dot, report) {
    const tokens = module.tokens;
    const nameIndex = dot + 1;
    if (!isName(tokens[nameIndex])) return;

    const scope = module.scopeAt(tokens[dot].start);
    const base = typeOfExpression(module, chainStart(tokens, dot), scope, 0, dot);
    const name = tokens[nameIndex].value;
    const isCall = isOp(tokens[nameIndex + 1], "(");
    const after = tokens[nameIndex + 1];
    const isAssigned = after !== undefined && after.type === "op" && ["=", "+=", "-=", "*=", "/=", "%="].includes(after.value);

    if ((base.kind === "enum" || base.kind === "enumItem") && isAssigned) {
        report("une énumération ne se modifie pas", nameIndex);
    } else if (base.kind === "enum") {
        if (isCall && !METHODS.enum[name]) report(`enum n'a pas de méthode \`${name}\``, nameIndex);
        if (!isCall && !base.enumeration.members.has(name)) report(`\`${base.enumeration.name}\` n'a pas de valeur \`${name}\``, nameIndex);
    } else if (base.kind === "enumItem") {
        if (isCall) report(`${base.enumeration.name} n'a pas de méthode \`${name}\``, nameIndex);
        else if (name !== "name" && name !== "value") report(`\`${base.enumeration.name}\` n'a pas d'attribut \`${name}\``, nameIndex);
    } else if (base.kind === "instance") {
        if (base.struct.scope.symbols.has(name)) return;
        report(`\`${base.struct.name}\` n'a pas ${isCall ? "de méthode" : "d'attribut"} \`${name}\``, nameIndex);
    } else if (METHODS[base.kind] && base.kind !== "map") {
        if (METHODS[base.kind][name]) return;
        report(`${base.kind} n'a pas ${isCall ? "de méthode" : "d'attribut"} \`${name}\``, nameIndex);
    } else if (["number", "bool", "char", "none"].includes(base.kind)) {
        report(`${base.kind} n'a pas d'attribut \`${name}\``, nameIndex);
    }
}

// La cible d'une affectation : le symbole de `x`, ou de l'attribut `x` dans `p.x`.
function assignmentTarget(module, index, scope) {
    const tokens = module.tokens;
    const nameToken = tokens[index - 1];
    if (!isName(nameToken)) return null;

    if (!isOp(tokens[index - 2], ".")) {
        return module.lookup(nameToken.value, scope, Infinity);
    }
    const base = typeOfExpression(module, chainStart(tokens, index - 2), scope, 0, index - 2);
    return base.kind === "instance" ? base.struct.scope.symbols.get(nameToken.value) || null : null;
}

function checkAssignment(module, index, report, checkValue, scopeOf) {
    const tokens = module.tokens;
    const scope = scopeOf(index);
    const symbol = assignmentTarget(module, index, scope);
    if (symbol === null || symbol === undefined) return;

    const name = tokens[index - 1].value;
    if (symbol.isConst) {
        report(`\`${name}\` est une constante`, index - 1);
        return;
    }
    if (symbol.kind !== "var" && symbol.kind !== "param") return;
    if (!symbol.typeName) return;

    const describe = (found) => `\`${name}\` est de type ${symbol.typeName}, pas ${found}`;
    if (tokens[index].value === "=") {
        checkValue(symbol.typeName, index + 1, scope, describe);
    } else if (symbol.typeName !== "str" && symbol.typeName !== "string" &&
               certainType(module, index + 1, scope).kind === "str") {
        // x::int += "a" donne une chaîne
        report(describe("str"), index + 1, Math.max(index + 1, expressionStop(module, index + 1) - 1));
    }
}

// Les indices du début de chaque argument de `(` ... `)`
function argumentsOf(tokens, open) {
    const starts = [];
    let depth = 0;
    for (let i = open; i < tokens.length; i++) {
        if (tokens[i].type !== "op") continue;
        if ("([{".includes(tokens[i].value)) {
            depth += 1;
        } else if (")]}".includes(tokens[i].value)) {
            depth -= 1;
            if (depth === 0) return { starts: isOp(tokens[open + 1], ")") ? [] : [open + 1, ...starts], closed: true };
        } else if (depth === 1 && tokens[i].value === ",") {
            starts.push(i + 1);
        }
    }
    return { starts: [], closed: false };
}

function checkArguments(module, func, label, nameIndex, args, report, checkValue, scope) {
    if (func.variadic) return;
    if (args.length !== func.params.length) {
        report(`\`${label}\` prend ${func.params.length} argument(s), mais en reçoit ${args.length}`, nameIndex);
        return;
    }
    args.forEach((index, position) => {
        const typeName = func.paramTypes[position];
        if (!typeName) return;
        // Une fonction écrite en C vérifie elle-même ses arguments, avec son propre message.
        const message = func.standard ?
            () => `\`${label}\` attend un argument de type ${typeName}, pas ${certainType(module, index, scope).kind}` :
            (found) => `\`${func.params[position]}\` est de type ${typeName}, pas ${found}`;
        checkValue(typeName, index, scope, message);
    });
}

function checkCall(module, open, report, checkValue, scopeOf) {
    const tokens = module.tokens;
    const nameIndex = open - 1;
    if (!isName(tokens[nameIndex]) || newlineBetween(module, open)) return;

    const previous = tokens[nameIndex - 1];
    if (isWord(previous, "func") || isWord(previous, "struct") || isOp(previous, "@")) return;

    const { starts, closed } = argumentsOf(tokens, open);
    if (!closed) return;

    const scope = scopeOf(open);
    const name = tokens[nameIndex].value;

    if (isWord(previous, "new")) {
        const symbol = module.lookup(name, scope, Infinity);
        if (symbol && symbol.kind === "enum") report(`\`${name}\` est une énumération : ses valeurs existent déjà`, nameIndex);
        if (!symbol || symbol.kind !== "struct") return;
        const inits = symbol.struct.scope.inits || [];
        const init = inits.find((func) => func.params.length === starts.length);
        if (init) {
            checkArguments(module, init, name, nameIndex, starts, report, checkValue, scope);
        } else if (starts.length > 0) {
            report(`\`${name}\` n'a pas de __init__ qui prend ${starts.length} argument(s)`, nameIndex);
        }
        return;
    }

    let func = null;
    if (isOp(previous, ".")) {
        const base = typeOfExpression(module, chainStart(tokens, nameIndex - 1), scope, 0, nameIndex - 1);
        const symbol = base.kind === "instance" ? base.struct.scope.symbols.get(name) : null;
        func = symbol && symbol.kind === "func" ? symbol.func : null;
    } else {
        const symbol = module.lookup(name, scope, Infinity);
        func = symbol && symbol.kind === "func" ? symbol.func : null;
    }
    if (func) checkArguments(module, func, name, nameIndex, starts, report, checkValue, scope);
}

module.exports = { complete, diagnose, tokenize, BUILTINS, METHODS, KEYWORDS, TYPES, STANDARD_MODULES };
