import time


def foo():
    bar()


def bar():
    for _ in range(100):
        time.sleep(1)


foo()
