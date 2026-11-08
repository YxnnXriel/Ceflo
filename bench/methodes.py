# Méthodes : un objet qui s'appelle lui-même, et modifie ses attributs
class Compte:
    __slots__ = ("solde", "operations")
    def __init__(self):
        self.solde = 0
        self.operations = 0
    def depose(self, n):
        self.solde += n
        self.compte_operation()
        return self
    def retire(self, n):
        if self.peut_retirer(n):
            self.solde -= n
            self.compte_operation()
        return self
    def peut_retirer(self, n):
        return self.solde >= n
    def compte_operation(self):
        self.operations += 1
comptes = [Compte() for i in range(100)]
for tour in range(10000):
    for c in comptes:
        c.depose(3).retire(2)
total = 0
ops = 0
for c in comptes:
    total += c.solde
    ops += c.operations
print(total, ops)
