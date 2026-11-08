# Gros texte : construire un fichier ligne par ligne, le découper, le relire
def assemble(lignes):
    texte = ""
    for l in lignes:
        texte += l + "\n"
    return texte
lignes = []
for i in range(30000):
    lignes.append("ligne {} : valeur={} ok".format(i, i * 3))
texte = assemble(lignes)
morceaux = texte.split("\n")
chiffres = 0
egal = 0
for l in morceaux:
    for c in l:
        if c >= '0' and c <= '9':
            chiffres += 1
        if c == '=':
            egal += 1
print(len(texte), len(morceaux), chiffres, egal)
