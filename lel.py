import _tokenize
import tokenize
import token

def blech():
    return "a+b\n"

with tokenize.open('lel.py') as f:
    tokens = tokenize.generate_tokens(f.readline)
    for token in tokens:
        print(token)
