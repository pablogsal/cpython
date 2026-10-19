from types import SimpleNamespace


def normalize(name):
    return name.strip().lower()


def valid(user):
    raise RuntimeError('the assertion must skip this call')


user = SimpleNamespace(name=' ALICE ')
expected = 'bob'
assert normalize(user.name) == expected and valid(user)
