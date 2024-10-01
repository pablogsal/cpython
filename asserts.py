code = """
# Python Assert Statement Examples

# Basic assertions
assert True
assert not False
assert 1 == 1
assert 2 + 2 == 4
assert len("hello") == 5
assert "python" in "I love python programming"
assert isinstance(42, int)
assert callable(len)

# Numeric assertions
assert 10 > 5
assert 7 < 9
assert 3 <= 3
assert 4 >= 4
assert 2**3 == 8
assert 10 % 3 == 1
assert abs(-5) == 5
assert round(3.14159, 2) == 3.14

# String assertions
assert "hello".upper() == "HELLO"
assert "WORLD".lower() == "world"
assert "python".capitalize() == "Python"
assert "  strip  ".strip() == "strip"
assert "hello world".split() == ["hello", "world"]
assert ",".join(["a", "b", "c"]) == "a,b,c"
assert "python".startswith("py")
assert "python".endswith("on")

# List assertions
assert [1, 2, 3] + [4, 5] == [1, 2, 3, 4, 5]
assert [1, 2, 3] * 2 == [1, 2, 3, 1, 2, 3]
assert 2 in [1, 2, 3]
assert 4 not in [1, 2, 3]
assert [1, 2, 3].index(2) == 1
assert [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5].count(5) == 3
assert sorted([3, 1, 4, 1, 5, 9, 2]) == [1, 1, 2, 3, 4, 5, 9]
assert list(reversed([1, 2, 3])) == [3, 2, 1]

# Dictionary assertions
assert {"a": 1, "b": 2} == {"b": 2, "a": 1}
assert "key" in {"key": "value"}
assert "value" in {"key": "value"}.values()
assert len({"a": 1, "b": 2, "c": 3}) == 3
assert {"a": 1, "b": 2}.get("c", 3) == 3
assert list({"a": 1, "b": 2}.keys()) == ["a", "b"]
assert list({"a": 1, "b": 2}.values()) == [1, 2]
assert list({"a": 1, "b": 2}.items()) == [("a", 1), ("b", 2)]

# Set assertions
assert {1, 2, 3} == {3, 2, 1}
assert {1, 2, 3}.issubset({1, 2, 3, 4, 5})
assert {1, 2, 3, 4, 5}.issuperset({1, 2, 3})
assert {1, 2, 3}.union({3, 4, 5}) == {1, 2, 3, 4, 5}
assert {1, 2, 3}.intersection({2, 3, 4}) == {2, 3}
assert {1, 2, 3}.difference({2, 3, 4}) == {1}
assert {1, 2, 3}.symmetric_difference({2, 3, 4}) == {1, 4}

# Type assertions
assert type(42) is int
assert type("hello") is str
assert type([1, 2, 3]) is list
assert type({"a": 1}) is dict
assert type({1, 2, 3}) is set
assert type((1, 2, 3)) is tuple

# Identity assertions
a = [1, 2, 3]
b = a
c = [1, 2, 3]
assert a is b
assert a is not c
assert a == c

# Membership assertions
assert 2 in [1, 2, 3]
assert 4 not in [1, 2, 3]
assert "h" in "hello"
assert "x" not in "hello"

# Boolean logic assertions
assert True and True
assert not (True and False)
assert True or False
assert not (False or False)
assert True is not False
assert (True or False) and (True and not False)

# Comparison assertions
assert 1 < 2 < 3
assert 3 > 2 > 1
assert 1 <= 1 <= 2
assert 2 >= 2 >= 1
assert 1 != 2 != 3

# Math function assertions
import math

assert math.sqrt(16) == 4
assert math.ceil(3.14) == 4
assert math.floor(3.14) == 3
assert math.isclose(0.1 + 0.2, 0.3, rel_tol=1e-9)
assert math.gcd(18, 24) == 6
assert math.lcm(4, 6) == 12

# String method assertions
assert "hello".replace("l", "L") == "heLLo"
assert "hello world".title() == "Hello World"
assert "  hello  ".strip() == "hello"
assert "hello".zfill(10) == "00000hello"
assert "hello".center(11, "-") == "---hello---"
assert "hello".isalpha()
assert "123".isdigit()
assert "hello123".isalnum()
assert "HELLO".isupper()
assert "hello".islower()

# List comprehension assertion
assert [x**2 for x in range(5)] == [0, 1, 4, 9, 16]

# Generator expression assertion
assert list(x**2 for x in range(5)) == [0, 1, 4, 9, 16]

# Zip function assertion
assert list(zip([1, 2, 3], ["a", "b", "c"])) == [(1, "a"), (2, "b"), (3, "c")]

# Enumerate function assertion
assert list(enumerate(["a", "b", "c"])) == [(0, "a"), (1, "b"), (2, "c")]

# Any and all function assertions
assert any([False, True, False])
assert all([True, True, True])
assert not any([False, False, False])
assert not all([True, False, True])

# Map function assertion
assert list(map(str, [1, 2, 3])) == ["1", "2", "3"]

# Filter function assertion
assert list(filter(lambda x: x % 2 == 0, [1, 2, 3, 4, 5])) == [2, 4]

# Sum function assertion
assert sum([1, 2, 3, 4, 5]) == 15

# Max and min function assertions
assert max([1, 5, 3, 2, 4]) == 5
assert min([1, 5, 3, 2, 4]) == 1

# Sorted function with key assertion
assert sorted(["apple", "banana", "cherry"], key=len) == ["apple", "cherry", "banana"]

# Lambda function assertion
assert (lambda x: x**2)(4) == 16


# Class and instance assertions
class TestClass:
    def __init__(self, value):
        self.value = value

    def double(self):
        return self.value * 2


test_instance = TestClass(5)
assert isinstance(test_instance, TestClass)
assert hasattr(test_instance, "value")
assert hasattr(test_instance, "double")
assert test_instance.value == 5
assert test_instance.double() == 10

# Exception assertions
try:
    1 / 0
except ZeroDivisionError:
    assert True
else:
    assert False

"""
compile(code, "a", "exec")
