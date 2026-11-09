# Vérifie que chaque extrait `fichier="..." lignes="A-B"` est identique au dépôt.
import re, sys, os
root = sys.argv[1]
for chap in sys.argv[2:]:
    text = open(chap).read()
    for m in re.finditer(r'```(\w+) ([^\n]*)\n(.*?)\n```', text, re.S):
        attrs = m.group(2)
        f = re.search(r'fichier="([^"]+)"', attrs)
        l = re.search(r'lignes="(\d+)-(\d+)"', attrs)
        if not (f and l):
            continue
        a, b = int(l.group(1)), int(l.group(2))
        path = os.path.join(root, f.group(1))
        lines = open(path).read().split('\n')[a-1:b]
        if '\n'.join(lines) != m.group(3):
            print("DIFF", chap, f.group(1), a, b)
        else:
            print("ok  ", f.group(1), a, b)
