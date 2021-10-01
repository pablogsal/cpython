import sys
sys._deactivate_opcache()

class Person:
    def __init__(self):
        self.value = "Gamma"

def foo(person):
    x = None
    for _ in range(1025):
        x = person.value
    return x


person = Person()
for _ in range(100000):
    foo(person)
