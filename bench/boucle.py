# Arithmétique et variables locales dans une boucle
def somme(n):
    total = 0
    for i in range(n):
        total += i % 7
    return total
print(somme(10000000))
