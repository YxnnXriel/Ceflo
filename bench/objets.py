# Création d'objets, attributs et méthodes
class Point:
    __slots__ = ("x", "y")
    def __init__(self, x, y):
        self.x = x
        self.y = y
    def ajoute(self, autre):
        return Point(self.x + autre.x, self.y + autre.y)
p = Point(0, 0)
un = Point(1, 2)
for i in range(1000000):
    p = p.ajoute(un)
print(p.x, p.y)
