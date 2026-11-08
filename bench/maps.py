# Map : écriture et lecture par clé
m = {}
for i in range(200000):
    m[i] = i
total = 0
for i in range(200000):
    total += m[i]
mots = {"un": 1, "deux": 2, "trois": 3}
for i in range(1000000):
    total += mots["deux"]
print(total)
