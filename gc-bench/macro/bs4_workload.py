import os
import pathlib
from bs4 import BeautifulSoup
from pygments import highlight
from pygments.formatters import HtmlFormatter
from pygments.lexers import PythonLexer

LIB = pathlib.Path(os.__file__).parent
soups = []
for path in sorted(LIB.glob('*.py'))[:90]:
    page = highlight(path.read_text(encoding='utf-8'), PythonLexer(), HtmlFormatter(full=True))
    soups.append(BeautifulSoup(page, 'html.parser'))
print(len(soups), sum(len(s.find_all('span')) for s in soups))
