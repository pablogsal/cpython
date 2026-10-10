import collections
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FG = Path(sys.argv[1])
DEPTH = 5
WORK = {'ast': 'stdlib ASTs, 720k tracked objects', 'pylint': 'pylint on five stdlib packages'}
PRS = {'fold': 'PR 159154', 'order': 'PR 159152'}


def load(name):
    out = collections.Counter()
    for line in (HERE / 'data' / f'{name}.gc.folded').read_text().splitlines():
        stack, count = line.rsplit(' ', 1)
        frames = [f for f in stack.split(';') if f][1:DEPTH + 1]
        out[';'.join(frames)] += int(count)
    return out


for work, what in WORK.items():
    before = load(f'{work}-base')
    for who, label in PRS.items():
        after = load(f'{work}-{who}')
        b, a = sum(before.values()), sum(after.values())
        for name, data in (('b', before), ('a', after)):
            (HERE / 'data' / f'{name}.tmp').write_text(''.join(f'{k} {v}\n' for k, v in sorted(data.items())))
        diff = subprocess.run(['perl', str(FG / 'difffolded.pl'), 'a.tmp', 'b.tmp'], cwd=HERE / 'data',
                              capture_output=True, text=True, check=True).stdout
        title = f'{label}: collector time on {what}'
        sub = (f'width: main ({b / 1e9:.1f}G cycles); blue: fewer cycles with the PR, red: more; '
               f'total with the PR {a / 1e9:.1f}G ({100 * (a / b - 1):+.0f}%)')
        svg = HERE / f'diff-{work}-{who}.svg'
        svg.write_text(subprocess.run(
            ['perl', str(FG / 'flamegraph.pl'), '--negate', '--title', title, '--subtitle', sub,
             '--width', '1200', '--height', '22', '--fontsize', '13', '--countname', 'cycles',
             '--minwidth', '0.4', '--nametype', 'Pass:'],
            input=diff, capture_output=True, text=True, check=True).stdout)
        subprocess.run(['rsvg-convert', '-o', str(svg.with_suffix('.png')), str(svg)], check=True)
        print(svg.name, f'{b / 1e9:.2f}G -> {a / 1e9:.2f}G')
