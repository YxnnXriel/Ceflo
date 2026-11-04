// Branche l'analyse de analyse.js sur Visual Studio Code.

const vscode = require("vscode");
const { complete, diagnose } = require("./analyse");

const KINDS = {
    function: vscode.CompletionItemKind.Function,
    method: vscode.CompletionItemKind.Method,
    field: vscode.CompletionItemKind.Field,
    variable: vscode.CompletionItemKind.Variable,
    struct: vscode.CompletionItemKind.Struct,
    enum: vscode.CompletionItemKind.Enum,
    enumMember: vscode.CompletionItemKind.EnumMember,
    type: vscode.CompletionItemKind.TypeParameter,
    keyword: vscode.CompletionItemKind.Keyword,
    file: vscode.CompletionItemKind.File,
    folder: vscode.CompletionItemKind.Folder,
};

function toCompletionItem(item) {
    const result = new vscode.CompletionItem(item.label, KINDS[item.kind]);
    result.detail = item.detail;
    if (item.doc) result.documentation = new vscode.MarkdownString(item.doc);
    if (item.snippet) result.insertText = new vscode.SnippetString(item.snippet);
    if (item.sort) result.sortText = `${item.sort}_${item.label}`;
    return result;
}

const provider = {
    provideCompletionItems(document, position, token, context) {
        return complete(document.getText(), document.offsetAt(position), {
            ...analysisOptions(document),
            trigger: context.triggerCharacter,
        }).map(toCompletionItem);
    },
};

function analysisOptions(document) {
    const folder = vscode.workspace.getWorkspaceFolder(document.uri);
    return {
        filePath: document.uri.scheme === "file" ? document.uri.fsPath : null,
        root: folder ? folder.uri.fsPath : null,
    };
}

function showErrors(document, collection) {
    if (document.languageId !== "ceflo") return;
    collection.set(document.uri, diagnose(document.getText(), analysisOptions(document)).map((error) => {
        const range = new vscode.Range(document.positionAt(error.start), document.positionAt(error.end));
        const diagnostic = new vscode.Diagnostic(range, error.message, vscode.DiagnosticSeverity.Error);
        diagnostic.source = "ceflo";
        return diagnostic;
    }));
}

// On attend une courte pause dans la frappe, pour ne pas souligner un mot pas encore fini.
function watchErrors(context) {
    const collection = vscode.languages.createDiagnosticCollection("ceflo");
    const timers = new Map();

    const later = (document) => {
        const key = document.uri.toString();
        clearTimeout(timers.get(key));
        timers.set(key, setTimeout(() => {
            timers.delete(key);
            showErrors(document, collection);
        }, 300));
    };

    vscode.workspace.textDocuments.forEach((document) => showErrors(document, collection));

    context.subscriptions.push(
        collection,
        vscode.workspace.onDidOpenTextDocument((document) => showErrors(document, collection)),
        vscode.workspace.onDidChangeTextDocument((event) => later(event.document)),
        // Un fichier enregistré peut être importé par les autres.
        vscode.workspace.onDidSaveTextDocument(() => {
            vscode.workspace.textDocuments.forEach((document) => showErrors(document, collection));
        }),
        vscode.workspace.onDidCloseTextDocument((document) => collection.delete(document.uri))
    );
}

function activate(context) {
    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider("ceflo", provider, ".", ":", "@", '"', "[", ",", " ", "/")
    );
    watchErrors(context);
}

module.exports = { activate };
