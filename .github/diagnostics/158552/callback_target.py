"""Exercise Python callbacks entered from C, including local cleanup on return."""


def callback(value):
    temporary = list(range(10_000))
    return value + 1


while True:
    list(map(callback, range(1_000)))
