# Chaînes : concaténation et parcours
s = ""
for i in range(100000):
    s += "ab"
n = 0
for c in s:
    if c == 'a':
        n += 1
print(n, len(s))
