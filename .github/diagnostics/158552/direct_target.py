"""Run the same callback body through direct Python calls."""


def callback(value):
    temporary = list(range(10_000))
    return value + 1


while True:
    for value in range(1_000):
        callback(value)
