#!/usr/bin/env python3

from collections import namedtuple
from itertools import islice, tee
from os.path import realpath
from re import compile

nwise = lambda g,n=3: zip(*(islice(g, i, None) for i, g in enumerate(tee(g, n))))

class MemRange(namedtuple('MemRange', 'from_addr to_addr offset length')):
    def __new__(self, from_addr, to_addr, offset):
        from_addr, to_addr, offset = (int(x, base=16) if not isinstance(x, int) else x
                                      for x in (from_addr, to_addr, offset))
        length = to_addr - from_addr
        return super().__new__(self, from_addr, to_addr, offset, length)

proc_pid_maps_re = '^([0-9a-f]+)-([0-9a-f]+) r[w-][x-]p ([0-9a-f]+) .{5} [0-9]+\s+(.*)$'
proc_pid_maps_match = compile(proc_pid_maps_re).match

if __name__ == '__main__':
    from sys import executable, argv

    outfile = argv[1] if len(argv) == 2 else 'a.out'
    executable = realpath(executable)

    with open('/proc/self/maps') as maps:
        maps = [match.groups() for match in (proc_pid_maps_match(x) for x in maps) if match]

    ranges = [MemRange(from_addr, to_addr, offset) for
              from_addr, to_addr, offset, filename in maps
              if filename == executable]

    with open('/proc/self/mem', 'rb') as mem:
        with open(outfile, 'wb') as out:
            for mem_range in ranges:
                mem.seek(mem_range.from_addr)
                data = mem.read(mem_range.length)
                # if out.tell() < mem_range.offset:
                #     zeros = b'\0' * (mem_range.offset - out.tell())
                #     out.write(zeros)
                out.write(data)
