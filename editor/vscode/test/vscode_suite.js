// Tourne à l'intérieur de Visual Studio Code : voir lancer.sh.
// Vérifie ce que l'utilisateur obtient vraiment, et non seulement analyse.js.

const fs = require("fs");
const path = require("path");
const vscode = require("vscode");

const RACINE = path.join(__dirname, "../../..");

async function propositions(contenu, declencheur) {
    const curseur = contenu.indexOf("|");
    const document = await vscode.workspace.openTextDocument({
        language: "ceflo",
        content: contenu.slice(0, curseur) + contenu.slice(curseur + 1),
    });
    const liste = await vscode.commands.executeCommand(
        "vscode.executeCompletionItemProvider", document.uri, document.positionAt(curseur), declencheur
    );
    return liste.items;
}

async function propositionsDansFichier(fichier, contenu) {
    const curseur = contenu.indexOf("|");
    const uri = vscode.Uri.file(fichier);
    fs.writeFileSync(fichier, contenu.slice(0, curseur) + contenu.slice(curseur + 1));
    const document = await vscode.workspace.openTextDocument(uri);
    const liste = await vscode.commands.executeCommand(
        "vscode.executeCompletionItemProvider", uri, document.positionAt(curseur)
    );
    return liste.items;
}

const nom = (item) => (typeof item.label === "string" ? item.label : item.label.label);

const STRUCT = `struct Metier { var Name::str }
struct Point {
    var x::int
    var Job::Metier
    func avance(n) { return self }
}
`;

// Les erreurs arrivent peu après l'ouverture du fichier : on les attend.
async function erreursDe(contenu) {
    const document = await vscode.workspace.openTextDocument({ language: "ceflo", content: contenu });
    for (let essai = 0; essai < 50; essai++) {
        const erreurs = vscode.languages.getDiagnostics(document.uri);
        if (erreurs.length > 0) return { document, erreurs };
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
    return { document, erreurs: [] };
}

const CAS = {
    "une mauvaise valeur est soulignée en rouge": async () => {
        const { document, erreurs } = await erreursDe('struct Point { var x::int = 0 }\nvar p = new Point()\np.x = "Ynn Ariel"\n');
        if (erreurs.length !== 1) return `${erreurs.length} erreurs`;
        const erreur = erreurs[0];
        return (document.getText(erreur.range) === '"Ynn Ariel"' && erreur.message === "`x` est de type int, pas str" &&
            erreur.severity === vscode.DiagnosticSeverity.Error && erreur.source === "ceflo") || JSON.stringify(erreur);
    },
    "l'erreur disparaît quand le code est corrigé": async () => {
        const { document, erreurs } = await erreursDe('var a::int = "x"\n');
        if (erreurs.length !== 1) return `${erreurs.length} erreurs au départ`;
        const edition = new vscode.WorkspaceEdit();
        edition.replace(document.uri, erreurs[0].range, "1");
        await vscode.workspace.applyEdit(edition);
        for (let essai = 0; essai < 50; essai++) {
            if (vscode.languages.getDiagnostics(document.uri).length === 0) return true;
            await new Promise((resolve) => setTimeout(resolve, 100));
        }
        return "l'erreur reste";
    },
    "aucune erreur sur un exemple": async () => {
        const fichier = path.join(RACINE, "exemple/5_structures.cf");
        const document = await vscode.workspace.openTextDocument(fichier);
        await new Promise((resolve) => setTimeout(resolve, 500));
        const erreurs = vscode.languages.getDiagnostics(document.uri);
        return erreurs.length === 0 || erreurs[0].message;
    },
    "le fichier est reconnu comme du CeFlo": async () => {
        const document = await vscode.workspace.openTextDocument(path.join(RACINE, "exemple/1_hello.cf"));
        return document.languageId === "ceflo" || `langage : ${document.languageId}`;
    },
    "attributs et méthodes après un point": async () => {
        const items = await propositions(`${STRUCT}var p = new Point()\np.|`, ".");
        const noms = items.map(nom);
        return (noms.includes("x") && noms.includes("Job") && noms.includes("avance")) || noms.join(",");
    },
    "une fonction s'insère avec ses parenthèses": async () => {
        const items = await propositions(`${STRUCT}var p = new Point()\np.|`, ".");
        const item = items.find((i) => nom(i) === "avance");
        return (item.insertText instanceof vscode.SnippetString && item.insertText.value === "avance($0)" &&
            item.kind === vscode.CompletionItemKind.Method && item.detail === "avance(n)") || JSON.stringify(item);
    },
    "une chaîne d'attributs": async () => {
        const noms = (await propositions(`${STRUCT}var p = new Point()\np.avance(1).Job.|`, ".")).map(nom);
        return noms.includes("Name") || noms.join(",");
    },
    "les clés et les méthodes d'un map": async () => {
        const noms = (await propositions('var t = { nom: "Yann" }\nt.|', ".")).map(nom);
        return (noms.includes("nom") && noms.includes("keys")) || noms.join(",");
    },
    "les noms visibles": async () => {
        const noms = (await propositions("var compteur = 1\npri|")).map(nom);
        return (noms.includes("compteur") && noms.includes("println")) || noms.join(",");
    },
    "les fichiers après from": async () => {
        const fichier = path.join(RACINE, "tests/__suite_tmp.cf");
        try {
            const noms = (await propositionsDansFichier(fichier, "from |")).map(nom);
            return (noms.includes("modules") && noms.includes("valeurs")) || noms.join(",");
        } finally {
            fs.rmSync(fichier, { force: true });
        }
    },
    "le module os après from et après import": async () => {
        // Deux fichiers : VS Code garde le texte d'un document déjà ouvert.
        const premier = path.join(RACINE, "tests/__suite_os1.cf");
        const second = path.join(RACINE, "tests/__suite_os2.cf");
        try {
            const apres = (await propositionsDansFichier(premier, "from |")).map(nom);
            const noms = (await propositionsDansFichier(second, "from os import |")).map(nom);
            return (apres.includes("os") && noms.includes("lire") && noms.includes("commande")) || `${apres} / ${noms}`;
        } finally {
            fs.rmSync(premier, { force: true });
            fs.rmSync(second, { force: true });
        }
    },
    "les noms après import": async () => {
        const fichier = path.join(RACINE, "tests/__suite_tmp.cf");
        try {
            const noms = (await propositionsDansFichier(fichier, "from modules.tout import |")).map(nom);
            return noms.length > 0 || "aucun nom";
        } finally {
            fs.rmSync(fichier, { force: true });
        }
    },
    "les structures après new": async () => {
        const noms = (await propositions(`${STRUCT}var p = new |`, " ")).map(nom);
        return (noms.includes("Point") && noms.includes("Metier")) || noms.join(",");
    },
    "les éléments d'une énumération": async () => {
        const noms = (await propositions("enum Couleur { ROUGE, VERT }\nCouleur.|", ".")).map(nom);
        return (noms.includes("ROUGE") && noms.includes("VERT") && noms.includes("values")) || noms.join(",");
    },
    "un mauvais élément d'énumération est souligné": async () => {
        const { document, erreurs } = await erreursDe("enum Couleur { ROUGE }\nprintln(Couleur.ROSE)\n");
        if (erreurs.length !== 1) return `${erreurs.length} erreurs`;
        return (document.getText(erreurs[0].range) === "ROSE" && erreurs[0].message === "`Couleur` n'a pas de valeur `ROSE`") || JSON.stringify(erreurs[0]);
    },
    "rien dans un commentaire": async () => {
        const items = await propositions("var a = 1\n// a|");
        return items.length === 0 || `des propositions dans un commentaire : ${items.map(nom).join(",")}`;
    },
    "tous les exemples s'analysent sans erreur": async () => {
        for (const nomFichier of fs.readdirSync(path.join(RACINE, "exemple"))) {
            if (!nomFichier.endsWith(".cf")) continue;
            const fichier = path.join(RACINE, "exemple", nomFichier);
            const document = await vscode.workspace.openTextDocument(fichier);
            const fin = document.positionAt(document.getText().length);
            const liste = await vscode.commands.executeCommand("vscode.executeCompletionItemProvider", document.uri, fin);
            if (!liste.items.some((item) => nom(item) === "println")) return `${nomFichier} : println manque`;
        }
        return true;
    },
};

async function run() {
    const lignes = [];
    let reussis = 0;
    let echoues = 0;

    await vscode.extensions.getExtension("yxnnxriel.ceflo").activate();

    for (const [titre, cas] of Object.entries(CAS)) {
        let resultat;
        try {
            resultat = await cas();
        } catch (erreur) {
            resultat = erreur.stack || String(erreur);
        }
        if (resultat === true) {
            reussis += 1;
        } else {
            echoues += 1;
            lignes.push(`ÉCHEC  ${titre} : ${resultat}`);
        }
    }

    lignes.push(`${reussis} réussis, ${echoues} échoués`);
    fs.writeFileSync(process.env.CEFLO_TEST_RESULTAT, lignes.join("\n") + "\n");
}

module.exports = { run };
