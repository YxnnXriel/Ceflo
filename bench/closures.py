# Closures : un compteur, des fonctions créées dans une boucle, filter
def compteur():
    n = 0
    def suivant():
        nonlocal n
        n += 1
        return n
    return suivant
c = compteur()
total = 0
for i in range(1000000):
    total += c()

ajouteurs = [(lambda x, k=k: x + k) for k in range(100)]
for i in range(10000):
    for f in ajouteurs:
        total += f(i)

nombres = list(range(200000))
seuil = 1000
total += len(list(filter(lambda x: x % 3 == 0 and x > seuil, nombres)))
print(total)
