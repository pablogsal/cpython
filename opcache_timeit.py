import sys
# sys._deactivate_opcache()

setup = """
class Person:
    def __init__(self):
        self.value = "Gamma"

def foo(person):
    x = None
    for _ in range(1025):
        x = person.value
    return x

person = Person()
"""

if __name__ == "__main__":
    from timeit import Timer
    t = Timer("foo(person)", setup)
    print(t.timeit())
