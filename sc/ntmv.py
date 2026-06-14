from ntime import parse_time, format_time
import re
import sys

def move_time(fi:str, fo:str, offset:int, fro:int, to:int|float):
    with open(fi, newline = '') as f:
        outs = f.readlines()

    for i in range(len(outs)):
        out = outs[i]
    
        try: idx = outs[i].index(',')
        except ValueError: continue
        
        t = parse_time(out[:idx])
        if not t: continue

        if t >= fro and t <= to:
            t += offset
            outs[i] = '{}{}'.format(format_time(t), out[idx:])
            
    with open(fo, 'w') as f:
        f.writelines(outs)

if len(sys.argv) <= 1:
    print('note time mover v0.1')
elif len(sys.argv) == 2:
    print('lacking param 2')
    exit(-2)
else:
    m = re.fullmatch('([-+]?\\d+)(?:\\[(.*?)-(.*?)\\])?', sys.argv[2])
    if not m:
        print('bad param 2')
        exit(-2)
    if m.group(2) != None:
        if len(m.group(2)) > 0:
            fro = parse_time(m.group(2))
            if not fro:
                print('bad time: {}'.format(m.group(2)))
                exit(-2)
        else: fro = 0
        if len(m.group(3)) > 0:
            to = parse_time(m.group(3))
            if not to:
                print('bad time: {}'.format(m.group(3)))
                exit(-2)
        else: to = float('inf')
    else:
        fro = 0
        to = float(inf)
    move_time(
        sys.argv[1],
        sys.argv[3] if len(sys.argv) >= 4 else sys.argv[1],
        int(m.group(1)),
        fro,
        to)
