import dis

def compteur():
    n = 0
    def f():
        nonlocal n
        n += 1
        return n
    return f

dis.dis(compteur)
