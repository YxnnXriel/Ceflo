# Maps de chaînes : compter des mots, comme un vrai programme
syllabes = ["ka", "lo", "mi", "ra", "tu", "ze", "po", "ni"]
mots = []
for i in range(300000):
    mots.append(syllabes[i % 8] + syllabes[(i // 8) % 8] + syllabes[(i * 7) % 5])
compte = {}
for m in mots:
    if m in compte:
        compte[m] += 1
    else:
        compte[m] = 1
plus = ""
max_ = 0
for m, n in compte.items():
    if n > max_:
        max_ = n
        plus = m
print(len(compte), plus, max_)
