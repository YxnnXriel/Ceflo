# Listes : ajout, lecture et parcours
l = []
for i in range(1000000):
    l.append(i * 2)
total = 0
for x in l:
    total += x
for i in range(len(l)):
    total -= l[i] // 2
print(total)
