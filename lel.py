def blech_fac(n):
    if n <= 1:
        1/0
        return 1
    return blech_fac(n-1) * n

def foo(n=2):
    x = n+1 
    return x

def blech_1(n):
    foo()
    y = blech_2(n)
    foo()
    return y

def blech_2(n):
    foo()
    y = blech_3(n)
    foo()
    return y

def blech_3(n):
    foo()
    return blech_4(n)

def blech_4(n):
    1/0

print(blech_1(1500))

# print(blech_fac(1500))
