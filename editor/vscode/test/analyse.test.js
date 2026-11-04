// Tests de l'autocomplétion :  node --test editor/vscode/test/analyse.test.js
//
// Dans chaque source, `|` marque la position du curseur.

const test = require("node:test");
const assert = require("node:assert");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

const { complete, diagnose, BUILTINS, METHODS, KEYWORDS, STANDARD_MODULES } = require("../analyse");

function run(source, options = {}) {
    const offset = source.indexOf("|");
    const text = source.slice(0, offset) + source.slice(offset + 1);
    return complete(text, offset, options);
}

const labels = (source, options) => run(source, options).map((item) => item.label).sort();

function includes(source, expected, options) {
    const found = labels(source, options);
    for (const name of expected) {
        assert.ok(found.includes(name), `${name} manque dans [${found.join(", ")}]`);
    }
    return found;
}

function excludes(source, unwanted, options) {
    const found = labels(source, options);
    for (const name of unwanted) {
        assert.ok(!found.includes(name), `${name} ne devrait pas être dans [${found.join(", ")}]`);
    }
}

const PERSONNE = `
struct Metier {
    var Name :: str = "Informatique"
    func decrire() { return Name }
}
struct Personne {
    var Name :: str
    var Age :: int
    var Job :: Metier
    var Amis = []
    var Plus

    func __init__(self, name :: str, age :: int) {
        self.Name = name
        self.Age = age
        self.Job = new Metier()
        self.Plus = "texte"
    }
    func parler(mot, fois) { return mot }
    func moi() : Personne { return self }
    func cadre() { return new Metier() }
}
`;

test("attributs et méthodes d'un objet", () => {
    const found = includes(PERSONNE + 'var p = new Personne("a", 1)\np.|', ["Name", "Age", "Job", "Amis", "parler", "moi"]);
    assert.ok(!found.includes("__init__") && !found.includes("self") && !found.includes("println"));
});

test("le mot en cours n'empêche pas de compléter", () => {
    includes(PERSONNE + "var p = new Personne()\np.par|", ["parler", "Age"]);
});

test("une chaîne d'attributs et d'appels", () => {
    const debut = PERSONNE + "var p = new Personne()\n";
    includes(debut + "p.Job.|", ["Name", "decrire"]);
    includes(debut + "p.moi().Job.|", ["Name", "decrire"]);
    includes(debut + "p.cadre().|", ["decrire"]);
    includes(debut + "p.Name.|", ["lenght", "split", "format"]);
    includes(debut + "p.Amis.|", ["append", "filter"]);
    includes(debut + "p.Plus.|", ["split"]);
});

test("`[` et `(` sur une nouvelle ligne commencent une instruction", () => {
    includes('var s = "a"\n[1].append(2)\ns.|', ["split"]);
    includes('var s = "a"\n(1)\ns.|', ["split"]);
});

test("le code après le curseur ne gêne pas", () => {
    includes(PERSONNE + "var p = new Personne()\np.|\nprintln(p.Age)\nvar q = 1\n", ["Age", "parler"]);
});

test("self et les attributs dans une méthode", () => {
    const source = `struct P {
    var Age::int
    var Job::Q
    func f(x) { self.| }
}
struct Q { var Nom }`;
    includes(source, ["Age", "Job", "f"]);
    includes(source.replace("self.|", "Job.|"), ["Nom"]);
    includes(source.replace("self.|", "|"), ["Age", "Job", "f", "x", "self"]);
});

test("les types de variables et de paramètres", () => {
    includes(PERSONNE + "func f(a::Personne) { a.| }", ["Age"]);
    includes(PERSONNE + "var a::Personne\na.|", ["Age"]);
    includes(PERSONNE + "var a\na = new Personne()\na.|", ["Age"]);
    includes(PERSONNE + "var a = @Personne(1)\na.|", ["Age"]);
    includes(PERSONNE + "func f() : Personne { }\nf().|", ["Age"]);
    includes(PERSONNE + "func f() { return new Personne() }\nf().|", ["Age"]);
    includes(PERSONNE + "func g(x) { var r = new Metier(); return r }\ng(1).|", ["decrire"]);
});

test("les valeurs simples", () => {
    includes('"abc".|', ["split"]);
    includes('var s = "a"\ns.|', ["split"]);
    includes('var s = "a" + 1\ns.|', ["split"]);
    includes("var s = 1 + \"a\"\ns.|", ["split"]);
    includes("[1, 2].|", ["append"]);
    includes("var s = input()\ns.|", ["split"]);
    includes('"a,b".split(",").|', ["append"]);
    includes('"a,b".split(",")[0].|', ["split"]);
    assert.deepStrictEqual(labels("var n = 1\nn.|"), []);
    assert.deepStrictEqual(labels("var b = 1 < 2\nb.|"), []);
});

test("les maps : clés et méthodes", () => {
    const debut = `var t = { nom: "Yann", age: 22, "avec espace": 1, profil: { ville: "A" }, double: lfunc(x) { return x } }\n`;
    const found = includes(debut + "t.|", ["nom", "age", "profil", "double", "keys", "has", "get"]);
    assert.ok(!found.includes("avec espace"));
    includes(debut + "t.nom.|", ["split"]);
    includes(debut + "t.profil.|", ["ville", "keys"]);
    includes(debut + "t.profil.ville.|", ["split"]);
    includes(debut + 't["profil"].|', ["ville"]);
    includes(debut + 't["|', ["nom", "age", "avec espace", "profil"]);
    includes("var t = {}\nt.ajout = new Q()\nt.|\nstruct Q { var a }", ["ajout", "keys"]);
    includes('var t = {}\nt["ajout"] = "x"\nt.ajout.|', ["split"]);
    includes("var t = {}\nt.keys().|", ["append"]);
    includes("var t::map\nt.|", ["keys"]);
    includes("var nom = 1\nvar t = { nom }\nt.|", ["nom"]);
});

test("for ... in", () => {
    includes("for var x in [new Q()] { x.| }\nstruct Q { var a }", ["a"]);
    includes("for var i, x in [new Q()] { x.| }\nstruct Q { var a }", ["a"]);
    includes('for var x in "abc".split("") { x.| }', ["split"]);
    includes("for var k, v in { a: 1 } { k.| }", ["split"]);
    includes("for var k::Q in z { k.| }\nstruct Q { var a }", ["a"]);
    includes("for var cle, valeur in { a: 1 } { | }", ["cle", "valeur"]);
    excludes("for var cle in { a: 1 } { }\n|", ["cle"]);
    includes("for var i = 0; i < 3; i += 1 { | }", ["i"]);
});

test("ce qui est visible au curseur", () => {
    const found = includes("var a = 1\nfunc f(b) { var c = 2; | }\nvar d = 3\nstruct S { }", ["a", "b", "c", "d", "f", "S", "println", "argv", "if"]);
    assert.ok(found.includes("for"));
    excludes("var a = 1\n|\nvar plus_tard = 2", ["plus_tard"]);
    excludes("func f(interne) { }\n|", ["interne"]);
    excludes("if true { var dedans = 1 }\n|", ["dedans"]);
    includes("func f() { | }\nfunc g() { }\nvar apres = 1", ["g", "apres"]);
});

test("le type inconnu propose ce qui pourrait convenir", () => {
    includes(PERSONNE + "func f(p) { p.| }", ["Age", "parler", "append", "keys"]);
    excludes(PERSONNE + "func f(p) { p.| }", ["println", "self", "__init__"]);
});

test("new, @, :: et le type de retour", () => {
    includes(PERSONNE + "var p = new |", ["Personne", "Metier"]);
    excludes(PERSONNE + "var p = new |", ["int", "println"]);
    includes(PERSONNE + "var p = @|", ["int", "list", "map", "Personne"]);
    includes(PERSONNE + "var p :: |", ["int", "str", "map", "Personne"]);
    includes(PERSONNE + "func f(a:: |", ["int", "Personne"]);
    includes(PERSONNE + "func f() : |", ["int", "Personne"]);
    assert.deepStrictEqual(labels("var |"), []);
    assert.deepStrictEqual(labels("func |"), []);
    assert.deepStrictEqual(labels("struct |"), []);
});

test("rien dans les commentaires et les chaînes", () => {
    assert.deepStrictEqual(labels("var a = 1\n// a|"), []);
    assert.deepStrictEqual(labels("var a = 1\n/* a| */"), []);
    assert.deepStrictEqual(labels('var a = 1\nvar b = "a|"'), []);
    assert.deepStrictEqual(labels('var a = "un.|'), []);
});

test("un caractère de déclenchement n'ouvre pas la liste partout", () => {
    assert.deepStrictEqual(labels("var a = 1\nvar b = a |", { trigger: " " }), []);
    assert.deepStrictEqual(labels("var a = [1]\na[|", { trigger: "[" }), []);
    includes("var a = 1\nprintln(|", ["a"]);
    includes(PERSONNE + "var p = new |", ["Personne"], { trigger: " " });
    includes("var a = [1]\na.|", ["append"], { trigger: "." });
});

test("les fonctions s'insèrent avec leurs parenthèses", () => {
    const items = run(PERSONNE + "var p = new Personne()\np.|");
    assert.strictEqual(items.find((i) => i.label === "parler").snippet, "parler($0)");
    assert.strictEqual(items.find((i) => i.label === "parler").detail, "parler(mot, fois)");
    assert.strictEqual(items.find((i) => i.label === "moi").snippet, "moi()");
    assert.strictEqual(items.find((i) => i.label === "Age").detail, "Age : int");
    assert.strictEqual(run("[].|").find((i) => i.label === "lenght").snippet, "lenght()");
    assert.match(run(PERSONNE + "new |").find((i) => i.label === "Personne").detail, /name, age/);
});

test("du code faux ou incomplet ne casse rien", () => {
    const morceaux = ["struct", "struct {", "func", "func (", "func f(", "var", "var a =", "for", "for var", "for var a in",
        "from", "from @", "from .", "import", "export", "export ::", "new", "@", "}", "}}}", "{{{", "(((", '"', "'", "/*",
        "lfunc", "a.", ".", "[", "]", "::", "->", "a[", "a(", "if", "return", "var a::", "t = {", "{ a:", "x = {\n"];
    for (const debut of morceaux) {
        for (const fin of morceaux) {
            const text = `${debut} ${fin} `;
            for (const offset of [0, debut.length, text.length]) {
                assert.ok(Array.isArray(complete(text, offset)), JSON.stringify(text));
            }
        }
    }
    // Se définir à partir de soi-même ne doit pas boucler.
    assert.deepStrictEqual(labels("var a = a\na.b.c.|").includes("println"), false);
    includes("var a = b\nvar b = a\nfunc f() { return f() }\nf().|", ["append"]);
});

test("les imports", (t) => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ceflo-"));
    t.after(() => fs.rmSync(dir, { recursive: true }));

    fs.mkdirSync(path.join(dir, "outils"));
    fs.mkdirSync(path.join(dir, "lib"));
    fs.writeFileSync(path.join(dir, "main.cf"), "");
    fs.writeFileSync(path.join(dir, "cercle.cf"), "from carre import *\nstruct Cercle { var r }\nfunc aire(x) { return x }\nvar RAYON = 1\nfunc main() { }\n");
    fs.writeFileSync(path.join(dir, "carre.cf"), "from cercle import *\nstruct Carre { var cote::int; func taille() { return cote } }\nfunc cote_de(c::Carre) { return c }\n");
    fs.writeFileSync(path.join(dir, "prive.cf"), "export :: [publique]\nfunc publique() { }\nfunc cachee() { }\n");
    fs.writeFileSync(path.join(dir, "outils", "texte.cf"), "func majuscule(s) { return s }\nstruct Mot { var lettres }\n");
    const options = { filePath: path.join(dir, "main.cf"), root: dir };

    // Le chemin
    const fichiers = includes("from |", ["cercle", "carre", "outils", "prive"], options);
    assert.ok(!fichiers.includes("main"), "un fichier ne s'importe pas lui-même");
    includes("from outils.|", ["texte"], options);
    includes('from "|', ["cercle.cf", "outils"], options);
    includes('from "outils/|', ["texte.cf"], options);
    includes("from cercle |", ["import"], options);

    // Les noms
    includes("from cercle import |", ["Cercle", "aire", "RAYON", "Carre"], options);
    excludes("from cercle import |", ["main"], options);
    includes("from cercle import [|", ["Cercle", "aire"], options);
    includes("from cercle import [aire, |", ["Cercle"], options);
    excludes("from cercle import [aire, |", ["aire"], options);
    includes("from outils.texte import [ma|", ["majuscule", "Mot"], options);
    includes('from "outils/texte.cf" import |', ["majuscule"], options);
    includes("from cercle import |", ["aire"], { ...options, trigger: " " });
    assert.strictEqual(run("from cercle import |", options).find((i) => i.label === "aire").snippet, null);

    // export limite ce qui se voit
    includes("from prive import |", ["publique"], options);
    excludes("from prive import |", ["cachee"], options);

    // Ce qui est importé s'utilise ensuite, avec ses membres
    includes("from cercle import *\n|", ["Cercle", "aire", "Carre"], options);
    excludes("from cercle import [aire]\n|", ["Cercle", "RAYON"], options);
    excludes("from prive import *\n|", ["cachee"], options);
    includes("from cercle import *\nvar c = new |", ["Cercle"], options);
    includes("from cercle import *\nvar c = new Cercle()\nc.|", ["r"], options);
    includes("from outils.texte import Mot\nvar m = new Mot()\nm.|", ["lettres"], options);
    includes("from carre import [cote_de, Carre]\ncote_de(1).|", ["cote", "taille"], options);

    // Un import introuvable ne casse rien
    assert.deepStrictEqual(labels("from rien import |", options), []);
    includes("from rien import *\nvar a = 1\n|", ["a"], options);
});

test("les bibliothèques natives", { skip: !fs.existsSync(path.join(__dirname, "../../../lib/math/libmath.so")) }, () => {
    const root = path.join(__dirname, "../../..");
    const options = { filePath: path.join(root, "x.cf"), root };
    includes('from @"lib/|', ["math", "file"], options);
    includes('from @"lib/math/|', ["libmath.so"], options);
    const names = includes('from @"lib/math/libmath.so" import [|', ["mathPi", "mathSqrt"], options);
    assert.ok(!names.includes("printf"));
    includes('from @"lib/math/libmath.so" import [mathPi]\n|', ["mathPi"], options);
});

test("les listes de l'éditeur suivent l'interpréteur", { skip: !fs.existsSync(path.join(__dirname, "../../../src/Value.c")) }, () => {
    const src = (name) => fs.readFileSync(path.join(__dirname, "../../../src", name), "utf8");

    const value = src("Value.c");
    for (const [kind, table] of [["list", "attr_value_list"], ["map", "attr_value_map"], ["str", "attr_value_str"], ["enum", "attr_value_enum"]]) {
        const body = value.slice(value.indexOf(`${table}[]`), value.indexOf("{NULL,NULL}", value.indexOf(`${table}[]`)));
        const names = [...body.matchAll(/\{"(\w+)"/g)].map((match) => match[1]).sort();
        assert.deepStrictEqual(Object.keys(METHODS[kind]).sort(), names, kind);
    }

    const bulletin = src("bulletin.c");
    const list = bulletin.slice(bulletin.indexOf("Bulletin_List"), bulletin.indexOf("{NULL,NULL}"));
    const builtins = [...list.matchAll(/\{"(\w+)"/g)].map((match) => match[1]).sort();
    assert.deepStrictEqual(Object.keys(BUILTINS).sort(), builtins);

    const lexer = src("lexer.c");
    const words = lexer.slice(lexer.indexOf("keywords[]"), lexer.indexOf("};", lexer.indexOf("keywords[]")));
    const keywords = [...words.matchAll(/\{"(\w+)"/g)].map((match) => match[1]).sort();
    assert.deepStrictEqual([...KEYWORDS].sort(), keywords);
});


// ===================================== $ERREURS =====================================


// Le texte souligné et le message de chaque erreur
function erreurs(source, options) {
    return diagnose(source, options).map((e) => [source.slice(e.start, e.end), e.message]);
}

const correct = (source, options) => assert.deepStrictEqual(erreurs(source, options), []);

test("un attribut typé reçoit une valeur d'un autre type", () => {
    const point = "struct Point {\n    var x :: int = 0\n    var y :: int = 1\n}\n\nvar p = new Point()\n\n";
    assert.deepStrictEqual(erreurs(point + 'p.x = "Ynn Ariel"\n\nprintln(p.y)'), [['"Ynn Ariel"', "`x` est de type int, pas str"]]);
    correct(point + "p.x = 3\nprintln(p.y)");
    assert.deepStrictEqual(erreurs(point + "p.x = 3.5"), [["3.5", "`x` est de type int, pas float"]]);
    assert.deepStrictEqual(erreurs(point + "p.z = 3"), [["z", "`Point` n'a pas d'attribut `z`"]]);
    assert.deepStrictEqual(erreurs(point + "println(p.z)"), [["z", "`Point` n'a pas d'attribut `z`"]]);
    assert.deepStrictEqual(erreurs(point + "p.z()"), [["z", "`Point` n'a pas de méthode `z`"]]);
    assert.deepStrictEqual(erreurs('struct P { var a::int; func f() { self.a = "x"; a = "y"; self.b = 1 } }').map((e) => e[1]),
        ["`a` est de type int, pas str", "`a` est de type int, pas str", "`P` n'a pas d'attribut `b`"]);
});

const REFUSES = [
        ['var a::int = "21"', '"21"', "`a` est de type int, pas str"],
        ["var a::int = []", "[]", "`a` est de type int, pas list"],
        ["var a::int = {}", "{}", "`a` est de type int, pas map"],
        ["var a::int = 3.9", "3.9", "`a` est de type int, pas float"],
        ["var a::int = true", "true", "`a` est de type int, pas bool"],
        ["var a::int = none", "none", "`a` est de type int, pas none"],
        ["var a::str = 5", "5", "`a` est de type str, pas int"],
        ['var a::float = "1"', '"1"', "`a` est de type float, pas str"],
        ["var a::bool = 1", "1", "`a` est de type bool, pas int"],
        ["var a::char = 65", "65", "`a` est de type char, pas int"],
        ["var a::list = 1", "1", "`a` est de type list, pas int"],
        ["var a::map = []", "[]", "`a` est de type map, pas list"],
        ['var a::int = 1\na = "b"', '"b"', "`a` est de type int, pas str"],
        ['var a::int = 1\na += "b"', '"b"', "`a` est de type int, pas str"],
        ['var a::int = 1\nvar a = "x"', '"x"', "`a` est de type int, pas str"],
        ['var a::int = 1\nvar a::str = "x"', "a", "`a` est déjà de type int"],
        ["const K = 1\nK = 2", "K", "`K` est une constante"],
        ['var a::int = "x" + 1', '"x" + 1', "`a` est de type int, pas str"],
        ['var a::int = 1 + "x"', '1 + "x"', "`a` est de type int, pas str"],
        ["var a::str = 1 + 2", "1 + 2", "`a` est de type str, pas number"],
        ["var a::int = 1 < 2", "1 < 2", "`a` est de type int, pas bool"],
        ["var a::int = 1 + 2 < 3", "1 + 2 < 3", "`a` est de type int, pas bool"],
        ['var a::int = input("x")', 'input("x")', "`a` est de type int, pas str"],
        ['var a::int = @str(1)', "@str(1)", "`a` est de type int, pas str"],
        ["func f() : int { }", "f", "`f` doit retourner une valeur de type int, pas none"],
        ["func f() : int { return }", "return", "`f` doit retourner une valeur de type int, pas none"],
        ['func f() : str { return 1 }', "1", "`f` doit retourner une valeur de type str, pas int"],
        ['func f() : int { return "a" + 1 }', '"a" + 1', "`f` doit retourner une valeur de type int, pas str"],
        ['func f(x::int) { }\nf("12")', '"12"', "`x` est de type int, pas str"],
        ['func f(x) { }\nf(1, 2)', "f", "`f` prend 1 argument(s), mais en reçoit 2"],
        ['func f(x) { }\nf()', "f", "`f` prend 1 argument(s), mais en reçoit 0"],
        ['struct S { }\nvar a::S = 1', "1", "`a` est de type S, pas int"],
        ['struct S { }\nstruct T { }\nvar a::S = new T()', "new T()", "`a` est de type S, pas T"],
        ['struct M { func __cast__(x) { return new M() } }\nvar m::M = 3', "3", "`m` est de type M, pas int"],
        ['struct P { func __init__(n::str) { } }\nnew P(1)', "1", "`n` est de type str, pas int"],
        ['struct P { func __init__(a) { } }\nnew P(1, 2)', "P", "`P` n'a pas de __init__ qui prend 2 argument(s)"],
        ['struct P { func f(x::int) { } }\nvar p = new P()\np.f("a")', '"a"', "`x` est de type int, pas str"],
        ['struct P { func f(x) { } }\nvar p = new P()\np.f()', "f", "`f` prend 1 argument(s), mais en reçoit 0"],
        ['from os import lire\nlire(3)', "3", "`lire` attend un argument de type str, pas number"],
        ['from os import *\necrire("a", [])', "[]", "`ecrire` attend un argument de type str, pas list"],
        ['from os import lire\nlire("a", "b")', "lire", "`lire` prend 1 argument(s), mais en reçoit 2"],
        ['from os import existe\nvar a::int = existe("a")', 'existe("a")', "`a` est de type int, pas bool"],
        ['from os import chemin\nvar a::int = chemin("a", "b")', 'chemin("a", "b")', "`a` est de type int, pas str"],
        ['var s = "a"\ns.rien()', "rien", "str n'a pas de méthode `rien`"],
        ["var n = 1\nn.rien", "rien", "number n'a pas d'attribut `rien`"],
        ['[1].rien()', "rien", "list n'a pas de méthode `rien`"],
        ['func f() : Q { return new Q() }\nstruct Q { var a::int }\nf().a = "x"', '"x"', "`a` est de type int, pas str"],
        ["enum C { A }\nprintln(C.ROSE)", "ROSE", "`C` n'a pas de valeur `ROSE`"],
        ["enum C { A }\nprintln(C.A.x)", "x", "`C` n'a pas d'attribut `x`"],
        ["enum C { A }\nC.A()", "A", "enum n'a pas de méthode `A`"],
        ["enum C { A }\nC.rien()", "rien", "enum n'a pas de méthode `rien`"],
        ["enum C { A }\nC.A.name()", "name", "C n'a pas de méthode `name`"],
        ["enum C { A }\nC.A = 1", "A", "une énumération ne se modifie pas"],
        ["enum C { A }\nC.B = 1", "B", "une énumération ne se modifie pas"],
        ["enum C { A }\nC.A.value = 1", "value", "une énumération ne se modifie pas"],
        ["enum C { A }\nC = 1", "C", "`C` est une constante"],
        ["enum C { A }\nnew C()", "C", "`C` est une énumération : ses valeurs existent déjà"],
        ["enum C { A }\nvar c::C = 1", "1", "`c` est de type C, pas int"],
        ['enum C { A }\nvar c::C = "A"', '"A"', "`c` est de type C, pas str"],
        ["enum C { A }\nvar c::C = C", "C", "`c` est de type C, pas enum"],
        ["enum C { A }\nenum T { P }\nvar c::C = T.P", "T.P", "`c` est de type C, pas T"],
        ["enum C { A }\nvar c::C = C.A\nc = 1", "1", "`c` est de type C, pas int"],
        ["enum C { A }\nfunc f(c::C) { }\nf(0)", "0", "`c` est de type C, pas int"],
        ["enum C { A }\nvar n::int = C.A", "C.A", "`n` est de type int, pas C"],
        ["enum C { A }\nstruct S { var c::C }\nnew S().c = 1", "1", "`c` est de type C, pas int"],
        ["enum C { A = 5 }\nvar s::str = C.A.value", "C.A.value", "`s` est de type str, pas int"],
        ['enum C { A = "x" }\nvar n::int = C.A.value', "C.A.value", "`n` est de type int, pas str"],
        ["enum C { A }\nvar n::int = C.A.name", "C.A.name", "`n` est de type int, pas str"],
];

test("les mêmes règles que l'interpréteur pour les types", () => {
    for (const [source, souligne, message] of REFUSES) {
        assert.deepStrictEqual(erreurs(source), [[souligne, message]], source);
    }
});

test("aucune erreur quand le code peut être juste", () => {
    const accepte = [
        "var a::int = 1\na = 2",
        "var a::int = 2 + 2",
        "var a::float = 3",
        "var a::number = 3.5",
        "var a::int = @int(3.9)",
        'var a::str = "x" + 1',
        "var a::bool = 1 < 2",
        "var a::bool = true and false",
        "var a::bool = 1 + 2 < 3",
        "var a::list = [1]",
        "var a::map = {}",
        "var a::map = { b: 1 }",
        "var a::string = \"x\"",
        "struct S { }\nvar a::S",
        "struct S { }\nvar a::S = none",
        "struct S { }\nvar a::S = new S()",
        "struct S { }\nvar a::S = new S()\na = none",
        // Une valeur qu'on ne peut pas connaître
        "func f(x) { var a::int = x }",
        "func f(x) { return x }\nvar a::int = f(1)",
        "var a::int = argv.lenght()",
        "var c = true\nvar a::int = c ? 1 : 2",
        "var a::str = true ? \"a\" : 1",
        'var l = ["a"]\nvar a::int = l[0]',
        'var t = { a: "x" }\nvar a::int = t.a',
        // Une variable sans type peut changer
        'var v = "x"\nv = 1\nvar a::int = v',
        'var v = "x"\nv += 1\nvar a::str = v',
        'var v = 1\nv += 1\nvar a::int = v',
        'var v = "x"\nvar v = 1\nvar a::int = v',
        'var v = "x"\nfunc f() { v = 1 }\nf()\nvar a::int = v',
        // Un attribut sans type peut changer d'un objet à l'autre
        'struct S { var b = "x" }\nvar a::int = new S().b',
        // Une valeur qui n'est pas une erreur de type
        "var a::int = 1 / 2",
        "var a::int = 5 - 1\na -= 2\na *= 2",
        "var a::str = \"x\"\na += 1",
        'var a::str = "x"\na += "y"',
        "var a::int = 1\nvar a = 2",
        "var a::int = 1\nvar a::int = 2",
        'var a = 1\nvar a::str = "x"',
        "var a = 1\na = \"x\"",
        // Une portée cache une autre
        'var a::int = 1\nfunc f(a) { a = "x" }',
        'var a::int = 1\nif true { var a = "x"; a = "y" }',
        'func g() { var a = "x"; a = 1 }\nvar a::int = 1',
        "for var a::str in [] { }",
        // Les cas de la grammaire qui se ressemblent
        "var t = { x: 1 }\nt.y = 2\nt.keys()\nt.autre()",
        "var a = 1\n[1].print()\n(a)",
        "var f = lfunc(x::int) { return x }\nf(\"a\", 1)",
        "from outils import *\nvar a::Inconnu = 1\nfoo(1)",
        'from "a.cf" import x',
        "struct S { var a::int; func __init__() { } func __init__(x) { a = x } }\nnew S()\nnew S(1)",
        "func f() : none { return }\nf()",
        // os : une fonction qui peut échouer ne donne pas de type sûr, et le nombre d'arguments de chemin est libre
        'from os import lire\nvar a::int = lire("a")',
        'from os import *\nvar a::str = chemin("a")\nvar b::str = chemin("a", "b", "c")\nvar c::number = lancer("true")\nvar d::bool = existe("a")',
        "func f() : int",
        "func f() : int {",
        "struct S { }\nfunc f() : S { }",
        "struct S { }\nfunc f() : S { return }",
        "func f() : int { if true { return 1 } }",
        "struct P { func f() { } }\nfunc p() { return new P() }\np().rien",
        "enum C { A, values, name }\nprintln(C.A, C.values, C.name.value, C.values(), C.A.name, C.A.value)",
        "enum C { A }\nvar c::C\nc = none\nc = C.A",
        "enum C { A }\nfunc f(c::C) : C { return c }\nf(C.A).name\nf(none)",
        "enum C { A = 1 }\nvar c::C = @C(1)\nc = @C(9)\nvar n::int = @int(C.A)\nvar s::str = @str(C.A)",
        "enum C { A = 2 }\nvar n::int = C.A.value",
        "enum C { A }\nfunc f(C) { C = 1; return C.rien }",
        "enum C { A }\nvar t = { [C.A]: 1 }\nt[C.A] = 2",
        "enum C {\n    A,\n    B = -3,\n    D = \"x\",\n}\nvar x::int = 1\nx = 2",
        "enum C { A B }\nvar x::int = 1",
        "from os import lire\nenum C { A }\nlire(\"a\")",
    ];
    for (const source of accepte) correct(source);
});

test("du code faux ou incomplet ne casse pas les erreurs", () => {
    const morceaux = ["struct", "struct {", "func", "func f(", "var", "var a =", "var a::", "for var a in", "from", "new", "@",
        "}", "{{{", "(((", '"', "/*", "lfunc", "a.", ".", "[", "::", "a(", "a = ", "return", "const a", "p.x ="];
    for (const debut of morceaux) {
        for (const fin of morceaux) {
            assert.ok(Array.isArray(diagnose(`${debut} ${fin} `)), `${debut} ${fin}`);
        }
    }
    assert.deepStrictEqual(diagnose("var a::int = a\nvar b = c\nvar c = b\nvar d::int = b\nfunc f() { return f() }\nvar e::int = f()"), []);
});

test("les erreurs à travers un import", (t) => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ceflo-"));
    t.after(() => fs.rmSync(dir, { recursive: true }));
    fs.writeFileSync(path.join(dir, "forme.cf"), "struct Cercle { var r::int }\nfunc aire(c::Cercle) : int { return 1 }\n");
    const options = { filePath: path.join(dir, "main.cf"), root: dir };

    assert.deepStrictEqual(erreurs('from forme import *\nvar c = new Cercle()\nc.r = "x"', options), [['"x"', "`r` est de type int, pas str"]]);
    assert.deepStrictEqual(erreurs('from forme import [aire]\naire(1)', options).length, 0, "Cercle n'est pas importé ici");
    assert.deepStrictEqual(erreurs('from forme import *\naire(1)', options), [["1", "`c` est de type Cercle, pas int"]]);
    assert.deepStrictEqual(erreurs('from forme import *\nvar s::str = aire(new Cercle())', options), [["aire(new Cercle())", "`s` est de type str, pas int"]]);
});

const RACINE = path.join(__dirname, "../../..");

test("aucune erreur dans les scripts qui marchent", { skip: !fs.existsSync(path.join(RACINE, "tests")) }, () => {
    for (const dossier of ["exemple", "tests", "tests/modules"]) {
        for (const nom of fs.readdirSync(path.join(RACINE, dossier))) {
            if (!nom.endsWith(".cf")) continue;
            const fichier = path.join(RACINE, dossier, nom);
            const options = { filePath: fichier, root: path.dirname(fichier) };
            assert.deepStrictEqual(erreurs(fs.readFileSync(fichier, "utf8"), options), [], `${dossier}/${nom}`);
        }
    }
});

test("les mêmes erreurs que l'interpréteur sur ses tests", { skip: !fs.existsSync(path.join(RACINE, "tests")) }, () => {
    const attendues = ["args_trop", "attribut_absent", "cast_impossible", "cast_jamais_seul", "cast_objet", "constante", "methode_absente",
        "retour_type", "type_affectation", "type_attribut", "type_autre_declaration", "type_chaine", "type_float", "type_parametre", "type_redefinition",
        "type_retour", "type_virgule"];
    for (const nom of attendues) {
        const fichier = path.join(RACINE, "tests/erreurs", `${nom}.cf`);
        if (!fs.existsSync(fichier)) continue;
        const texte = fs.readFileSync(fichier, "utf8");
        const message = texte.split("\n")[0].replace("// attendu: erreur: ", "");
        assert.ok(erreurs(texte).some((e) => e[1].startsWith(message)), `${nom} : ${JSON.stringify(erreurs(texte))}`);
    }
});

test("ce qui est souligné échoue vraiment, avec le même message", { skip: !fs.existsSync(path.join(RACINE, "Ceflo")) }, (t) => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ceflo-"));
    t.after(() => fs.rmSync(dir, { recursive: true }));
    const script = path.join(dir, "cas.cf");
    const ecarts = [];

    for (const [source, , message] of REFUSES) {
        // Une fonction ne se trompe que lorsqu'elle est appelée.
        const appel = /^func f\(\) : \w+ \{.*\}$/.test(source) ? "\nf()" : "";
        fs.writeFileSync(script, source + appel + "\n");
        const resultat = spawnSync(path.join(RACINE, "Ceflo"), [script], { input: "ligne\n", encoding: "utf8" });
        const premiere = resultat.stderr.split("\n")[0];
        // Sans calculer, l'éditeur ne sait pas si 1 + 2 tombe juste : il dit `number` là où l'exécution dit `int`.
        const attendu = `erreur: ${message}`;
        const memeSens = premiere === attendu || (attendu.endsWith("pas number") && /pas (int|float)$/.test(premiere) &&
            premiere.split(", pas")[0] === attendu.split(", pas")[0]);
        if (resultat.status !== 1 || !memeSens) ecarts.push(`${JSON.stringify(source)}\n  éditeur   : ${attendu}\n  exécution : ${resultat.status} ${premiere}`);
    }
    assert.deepStrictEqual(ecarts, []);
});

test("le module os", (t) => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ceflo-"));
    t.after(() => fs.rmSync(dir, { recursive: true }));
    const options = { filePath: path.join(dir, "main.cf"), root: dir };

    // Il est proposé après from, et ses fonctions après import
    assert.strictEqual(run("from |", options).find((i) => i.label === "os").detail, "module fourni par CeFlo");
    includes("from os import |", ["lire", "ecrire", "dossier", "commande", "chemin"], options);
    includes("from os import [lire, |", ["ecrire"], options);
    excludes("from os import [lire, |", ["lire"], options);

    // Ses fonctions ont une description, et leurs parenthèses quand on les appelle
    const lire = run('from os import *\n|').find((i) => i.label === "lire");
    assert.strictEqual(lire.detail, "lire(chemin)");
    assert.strictEqual(lire.snippet, "lire($0)");
    assert.match(lire.doc, /none/);
    assert.strictEqual(run('from os import *\n|').find((i) => i.label === "chemin").detail, "chemin(a, b...)");

    // Leur résultat se complète
    includes('from os import *\nlire("a").|', ["split", "lenght"]);
    includes('from os import *\nvar l = lire_lignes("a")\nl.|', ["append"]);
    includes('from os import *\nlire_lignes("a")[0].|', ["split"]);
    includes('from os import *\nfor var nom in dossier(".") { nom.| }', ["split"]);
    includes('from os import *\ncommande("ls").|', ["code", "sortie", "keys"]);
    includes('from os import *\ncommande("ls").sortie.|', ["split"]);
    includes('from os import *\nchemin("a").|', ["split"]);

    // Seules les fonctions importées existent
    excludes('from os import [lire]\n|', ["ecrire"]);
    excludes("|", ["lire"]);

    // Un fichier os.cf du projet passe avant le module, comme à l'exécution
    fs.writeFileSync(path.join(dir, "os.cf"), "func a_moi() { }\n");
    includes("from os import |", ["a_moi"], options);
    excludes("from os import |", ["lire"], options);
    assert.strictEqual(run("from |", options).find((i) => i.label === "os").detail, "module");
});

test("les énumérations", (t) => {
    const COULEUR = 'enum Couleur { ROUGE, VERT = 5, NOM = "x" }\n';

    assert.deepStrictEqual(labels(`${COULEUR}Couleur.|`), ["NOM", "ROUGE", "VERT", "values"]);
    assert.deepStrictEqual(labels(`${COULEUR}Couleur.ROUGE.|`), ["name", "value"]);
    assert.deepStrictEqual(labels(`${COULEUR}var c::Couleur\nc.|`), ["name", "value"]);
    assert.deepStrictEqual(labels(`${COULEUR}func f(c::Couleur) { c.| }`), ["name", "value"]);
    assert.deepStrictEqual(labels(`${COULEUR}for var c in Couleur.values() { c.| }`), ["name", "value"]);
    assert.deepStrictEqual(labels(`${COULEUR}for var i, c in Couleur.values() { c.| }`), ["name", "value"]);
    assert.deepStrictEqual(labels(`${COULEUR}Couleur.values()[0].|`), ["name", "value"]);
    includes(`${COULEUR}Couleur.values().|`, ["append", "lenght"]);
    includes(`${COULEUR}Couleur.NOM.name.|`, ["split"]);
    includes(`${COULEUR}Couleur.NOM.value.|`, ["split"]);

    includes(`${COULEUR}var c::|`, ["Couleur", "int"]);
    includes(`${COULEUR}func f() : |`, ["Couleur"]);
    includes(`${COULEUR}@|`, ["Couleur"]);
    excludes(`${COULEUR}new |`, ["Couleur"]);
    includes(`${COULEUR}Cou|`, ["Couleur", "enum"]);
    assert.strictEqual(run(`${COULEUR}Cou|`).find((i) => i.label === "Couleur").kind, "enum");
    assert.strictEqual(run(`${COULEUR}Couleur.|`).find((i) => i.label === "VERT").kind, "enumMember");

    // Le nom d'une énumération et de ses éléments : c'est à l'auteur de les choisir.
    assert.deepStrictEqual(labels("enum |"), []);
    assert.deepStrictEqual(labels("enum Cou|"), []);
    assert.deepStrictEqual(labels("var a = 1\nenum C { |"), []);
    assert.deepStrictEqual(labels("var a = 1\nenum C { A = 1, B|"), []);

    // Le reste du fichier s'analyse encore si l'énumération n'est pas finie.
    includes("enum C { A,\nvar x = 1\nx|", ["x"]);
    includes("enum C { A\nfunc f() { }\nf|", ["f"]);

    // Coupé n'importe où, le code ne casse ni la complétion ni les erreurs.
    const source = 'enum C { A, B = -5, D = "x", }\nvar c::C = C.A\nprintln(C.D.value, @C(1), { [C.A]: 1 })\n';
    for (let i = 0; i <= source.length; i++) {
        complete(source.slice(0, i), i);
        diagnose(source.slice(0, i));
    }

    // À travers un import, avec `export` qui cache le reste.
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ceflo-"));
    t.after(() => fs.rmSync(dir, { recursive: true }));
    fs.writeFileSync(path.join(dir, "reseau.cf"), "enum Http { OK = 200 }\nenum Cache { PRIVE }\nexport :: [Http]\n");
    const options = { filePath: path.join(dir, "ici.cf"), root: dir };
    assert.deepStrictEqual(labels("from reseau import |", options), ["Http"]);
    assert.deepStrictEqual(labels("from reseau import [Http]\nHttp.|", options), ["OK", "values"]);
    assert.deepStrictEqual(erreurs("from reseau import [Http]\nvar h::Http = 1\nHttp.NON", options), [
        ["1", "`h` est de type Http, pas int"],
        ["NON", "`Http` n'a pas de valeur `NON`"],
    ]);
});

test("match", (t) => {
    const C = "enum C { A, B, D }\n";
    const avertissements = (source, options) => diagnose(source, options)
        .filter((e) => e.severity === "warning")
        .map((e) => [source.slice(e.start, e.end), e.message]);

    // Un élément oublié : en jaune, sur le mot `match`.
    assert.deepStrictEqual(avertissements(`${C}match C.A { case C.A { } }`), [["match", "ce match ne traite pas C.B, C.D"]]);
    assert.deepStrictEqual(avertissements(`${C}func f(c::C) { match c { case C.A, C.B { } } }`), [["match", "ce match ne traite pas C.D"]]);
    assert.deepStrictEqual(avertissements(`${C}func f(c) { match c { case C.A { } case C.B { } } }`), [["match", "ce match ne traite pas C.D"]]);
    assert.deepStrictEqual(avertissements(`${C}func f(c::C) { match c { case none { } case C.A { } case C.B { } } }`), [["match", "ce match ne traite pas C.D"]]);
    assert.strictEqual(diagnose(`${C}match C.A { case C.A { } }`)[0].severity, "warning");

    // Rien à dire.
    const justes = [
        `${C}match C.A { case C.A { } case C.B { } case C.D { } }`,
        `${C}match C.A { case C.A, C.B, C.D { } }`,
        `${C}match C.A { case C.A { } else { } }`,
        `${C}func f(c::C, d::C) { match c { case C.A { } case d { } } }`,
        `${C}func f(c) { match c { case 1 { } } }`,
        "match 1 { case 1 { } }",
        'match "if" { case "if" { } case "else" { } }',
        "var a = 1\nmatch true { case a < 1 { } case a > 1 { } }",
        `${C}match C.A { case C.A { match 1 { case 1 { } } } case C.B { } case C.D { } }`,
        `${C}match C.A {`,
        `${C}match C.A { case C.A {`,
        `${C}match`,
        "match { a: 1 }.a { case 1 { } }",
    ];
    for (const source of justes) assert.deepStrictEqual(diagnose(source), [], source);

    // Un `match` imbriqué ne compte pas les cas de celui qui l'entoure.
    assert.deepStrictEqual(avertissements(`${C}match C.A { case C.A { match C.B { case C.B { } } } case C.B { } case C.D { } }`),
        [["match", "ce match ne traite pas C.A, C.D"]]);

    // Les erreurs habituelles continuent de s'appliquer.
    assert.deepStrictEqual(erreurs(`${C}match C.A { case C.ROSE { } else { } }`), [["ROSE", "`C` n'a pas de valeur `ROSE`"]]);
    assert.deepStrictEqual(erreurs("match 1 { case 1 { var a::int = \"x\" } }"), [['"x"', "`a` est de type int, pas str"]]);

    // case : les éléments pas encore traités
    assert.deepStrictEqual(labels(`${C}var c::C\nmatch c { case | }`), ["C.A", "C.B", "C.D"]);
    assert.deepStrictEqual(labels(`${C}var c::C\nmatch c { case C.A, | }`), ["C.B", "C.D"]);
    assert.deepStrictEqual(labels(`${C}var c::C\nmatch c {\n    case C.A { }\n    case |\n}`), ["C.B", "C.D"]);
    assert.deepStrictEqual(labels(`${C}var c::C\nmatch c { case C.A { } case C.B { println(1) } case | }`), ["C.D"]);
    assert.strictEqual(run(`${C}match C.A { case | }`)[0].kind, "enumMember");
    assert.deepStrictEqual(labels(`${C}match C.A { case C.| }`), ["A", "B", "D", "values"]);
    includes("var x = 1\nmatch x { case | }", ["x", "println"]);
    includes(`${C}var c::C\nmatch c { case C.A { c.| } }`, ["name", "value"]);
    includes(`${C}var liste = [1, |]`, ["C"]);
    includes("ma|", ["match", "case"]);

    // Coupé n'importe où, le code ne casse ni la complétion ni les erreurs.
    const source = `${C}match C.A {\n    case C.A, C.B { println(1) }\n    case "x", none { }\n    else { }\n}\n`;
    for (let i = 0; i <= source.length; i++) {
        complete(source.slice(0, i), i);
        diagnose(source.slice(0, i));
    }

    // À travers un import.
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ceflo-"));
    t.after(() => fs.rmSync(dir, { recursive: true }));
    fs.writeFileSync(path.join(dir, "reseau.cf"), "enum Http { OK = 200, NON = 404 }\n");
    const options = { filePath: path.join(dir, "ici.cf"), root: dir };
    assert.deepStrictEqual(avertissements("from reseau import [Http]\nmatch Http.OK { case Http.OK { } }", options),
        [["match", "ce match ne traite pas Http.NON"]]);
});

test("ce que match signale arrive vraiment à l'exécution", { skip: !fs.existsSync(path.join(RACINE, "Ceflo")) }, (t) => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ceflo-"));
    t.after(() => fs.rmSync(dir, { recursive: true }));
    const script = path.join(dir, "cas.cf");

    fs.writeFileSync(script, "enum C { A, B }\nmatch C.B { case C.A { } }\n");
    const resultat = spawnSync(path.join(RACINE, "Ceflo"), [script], { encoding: "utf8" });
    assert.strictEqual(resultat.status, 1);
    assert.strictEqual(resultat.stderr.split("\n")[0], "erreur: ce match ne traite pas C.B");
    assert.strictEqual(diagnose(fs.readFileSync(script, "utf8"))[0].message, "ce match ne traite pas C.B");
});

test("la description de os suit l'interpréteur", { skip: !fs.existsSync(path.join(RACINE, "src/os.c")) }, () => {
    const source = fs.readFileSync(path.join(RACINE, "src/os.c"), "utf8");
    const list = source.slice(source.indexOf("Os_List[]"), source.indexOf("{NULL,NULL}"));
    const inC = [...list.matchAll(/\{"(\w+)"/g)].map((match) => match[1]).sort();
    const os_ = STANDARD_MODULES.os;
    const decrites = [...os_.source.matchAll(/func (\w+)/g)].map((match) => match[1]).sort();
    assert.deepStrictEqual(decrites, inC);
    assert.deepStrictEqual(Object.keys(os_.docs).sort(), inC);
});
