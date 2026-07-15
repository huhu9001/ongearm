from ntime import parse_time, format_time

from collections import deque
import csv
import math
import os
import re
import sys

SVG_HEAD = re.sub('\\n\\s*', '', '''
<svg width="700" height="{0}" xmlns="http://www.w3.org/2000/svg">
    <defs>
        <path id="d" d="
            m0,-35l-20 20a25 25 254 1 0 40 0z" stroke="black" stroke-width="3" fill="white"/>
        <path id="a" d="
            m0,-15l-14 14l4 4l7 -7v18h6v-18l7 7l4 -4z" stroke="black" stroke-width="1" fill="white"/>
        <g id="nn">
            <circle r="25" stroke="black" stroke-width="3" fill="white"/>
            <circle r="20" fill="red"/>
        </g>
        <g id="nu">
            <use href="#d"/>
            <circle r="20" fill="green"/>
            <use href="#a"/>
        </g>
        <g id="nl">
            <use href="#d" transform="matrix(0 -1 1 0 0 0)"/>
            <circle r="20" fill="cyan"/>
            <use href="#a" transform="matrix(0 -1 1 0 0 0)"/>
        </g>
        <g id="nr">
            <use href="#d" transform="matrix(0 1 -1 0 0 0)"/>
            <circle r="20" fill="yellow"/>
            <use href="#a" transform="matrix(0 1 -1 0 0 0)"/>
        </g>
        <linearGradient id="stg" gradientTransform="rotate(90) translate(0 0)">
            <stop offset="0%" stop-color="orange"/>
            <stop offset="50%" stop-color="yellow"/>
            <stop offset="100%" stop-color="cyan"/>
        </linearGradient>
        <linearGradient id="nbg" gradientTransform="rotate(-45) translate(-0.5 0)">
            <stop offset="0%" stop-color="#FE4EDA"/>
            <stop offset="50%" stop-color="#B0BF1A"/>
            <stop offset="100%" stop-color="cyan"/>
        </linearGradient>
        <g id="nb">
            <circle r="60" style="fill:url('#nbg')"/>
            <circle r="48" stroke="white" stroke-width="10" fill="none"/>
        </g>
        <line id="vl" y2="{0}" stroke="black" stroke-dasharray="4"/>
    </defs>
    <style>
        text{{font-size:14px}}
        rect{{stroke:black;stroke-width:2;fill:white}}
        polygon{{stroke:black;stroke-width:3;fill:url('#stg')}}
    </style>
''')

N_LANES = [6, 4, 2]

def x2x6(x:float)->float: return x * 102 - 2
def x2x4(x:float)->float: return x * 170 - 70
def x2x2(x:float)->float: return x * 306 - 104

X2X = [x2x6, x2x4, x2x2]

def make_strip_sec(x1:float, y1:float, x2:float, y2:float)->(float, float):
    if x1 == x2: return (25, 0)
    d = math.dist((x1, y1), (x2, y2))
    return (25 * ((y1 - y2) / d), 25 * ((x2 - x1) / d))

def make_strip(x2x:'(float)->float', t2y:'(int)->float', strp:list)->deque:
    pts = deque()
    if len(strp) < 2: return pts
    
    def p2s(x:float, y:float)->str:
        return '{},{}'.format(int(x), int(y))
    
    x_0 = x2x(strp[0][0])
    y_0 = t2y(strp[0][1])
    x0 = x2x(strp[1][0])
    y0 = t2y(strp[1][1])
    dx0, dy0 = make_strip_sec(x_0, y_0, x0, y0)
    pts.append(p2s(x_0 + dx0, y_0 + dy0))
    pts.appendleft(p2s(x_0 - dx0, y_0 - dy0))
    for xx, t in strp[2:]:
        x = x2x(xx)
        y = t2y(t)
        dx, dy = make_strip_sec(x0, y0, x, y)
        den = dx0 * dy - dx * dy0
        if den == 0:
            pts.append(p2s((x0 + dx0), (y0 + dy0)))
            pts.appendleft(p2s((x0 - dx0), (y0 - dy0)))
        else:
            dx_tansect = 625 * ((dy - dy0) / den)
            dy_tansect = 625 * ((dx0 - dx) / den)
            if den > 0:
                # bending right
                if ((x0 - x_0 + dx_tansect) * dy0 - (y0 - y_0 + dy_tansect) * dx0 > 0
                    and (x - x0 - dx_tansect) * dy - (y - y0 - dy_tansect) * dx > 0):
                    pts.append(p2s(x0 + dx_tansect, y0 + dy_tansect))
                else:
                    pts.append(p2s(x0 + dx0, y0 + dy0))
                    pts.append(p2s(x0 + dx, y0 + dy))
                pts.appendleft(p2s(x0 - dx0, y0 - dy0))
                pts.appendleft(p2s(x0 - dx, y0 - dy))
            else:
                # bending left
                pts.append(p2s(x0 + dx0, y0 + dy0))
                pts.append(p2s(x0 + dx, y0 + dy))
                if ((x0 - x_0 - dx_tansect) * dy0 - (y0 - y_0 - dy_tansect) * dx0 > 0
                    and (x - x0 + dx_tansect) * dy - (y - y0 + dy_tansect) * dx > 0):
                    pts.appendleft(p2s(x0 - dx_tansect, y0 - dy_tansect))
                else:
                    pts.appendleft(p2s(x0 - dx0, y0 - dy0))
                    pts.appendleft(p2s(x0 - dx, y0 - dy))
        x_0, y_0 = x0, y0
        x0, y0 = x, y
        dx0, dy0 = dx, dy
    pts.append(p2s(x0 + dx0, y0 + dy0))
    pts.appendleft(p2s(x0 - dx0, y0 - dy0))
    return pts

def get_crs(fn:str)->int:
    crs = os.path.splitext(fn)[0][-3:]
    if len(crs) == 3 and crs[0] == '_':
        return 2 if crs[1] == '2' else 1 if crs[1] == '4' else 0
    return 0
    
def make_svg(fn:str, crs:int)->list:
    n_lanes = N_LANES[crs]

    csvl = []
    with open(fn, newline = '') as csvf:
        for line in csv.reader(csvf):
            t = parse_time(line[0])
            if not t:
                print('warning time ignored: {}'.format(line[0]))
                continue
            csvl.append((t, line[1]))
    def k0(p): return p[0]
    csvl.sort(key = k0)
    
    notes = []
    strps = [[], []]
    busy = [False, False]
    t_min = float('inf')
    t_max = 0
    for line in csvl:
        t = line[0]
        if t > t_max: t_max = t
        if t < t_min: t_min = t
    
        for m in re.finditer('(?:(\\d)|\\((.*?)\\))([sSdD]?)([-+=uUlLrR]?)', line[1]):
            if m.group(1):
                x = float(m.group(1))
            elif m.group(2):
                try:
                    x = float(m.group(2))
                except SyntaxError:
                    print('warning bad note: {}'.format(m.group(0)))
                    continue
    
            d = m.group(3)
            dex = (
                0 if d == 's' or d == 'S' else
                1 if d == 'd' or d == 'D' else
                0 if x * 2 <= n_lanes + 1 else 1)
            
            k = m.group(4)
            kid = ''
            append_strip = False
            if k == 'u' or k == 'U':
                kid = 'nu'
                if busy[dex]:
                    busy[dex] = False
                    append_strip = True
            elif k == 'l' or k == 'L':
                kid = 'nl'
                if busy[dex]:
                    busy[dex] = False
                    append_strip = True
            elif k == 'r' or k == 'R':
                kid = 'nr'
                if busy[dex]:
                    busy[dex] = False
                    append_strip = True
            elif k == '=':
                append_strip = True
            elif k == '+':
                busy[dex] = True
                strps[dex].append([(x, t)])
                kid = 'nn'
            elif k == '-':
                busy[dex] = False
                append_strip = True
                kid = 'nn'
            else:
                if m.group(1):
                    kid = 'nn'
                else:
                    kid = 'nb'
        
            if kid: notes.append((kid, x, t))
            if append_strip:
                if len(strps[dex]) > 0:
                    strps[dex][-1].append((x, t))
                else:
                    print('warning bad strip: {}'.format(m.group(0)))
    if t_min > t_max: t_min = t_max
    notes.reverse()
    
    x2x = X2X[crs]
    def t2y(t:int)->float: return (t_max - t) / 2 + 100
    
    outs = [SVG_HEAD.format(int((t_max - t_min) / 2) + 200)]

    for i in range(1, n_lanes + 1):
        outs.append('<use href="#vl" x="{}"/>'.format(x2x(i)))

    for t, _ in csvl:
        txt = '<text y="{}">{}</text>'.format(int(t2y(t)), format_time(t))
        if txt != outs[-1]: outs.append(txt)
            
    i = 0
    while i < len(notes):
        i1 = i + 1
        while i1 < len(notes) and notes[i1][2] == notes[i][2]:
            i1 += 1
        for j in range(i + 1, i1):
            x0 = x2x(notes[j - 1][1])
            x1 = x2x(notes[j][1])
            outs.append('<rect x="{}" y="{}" width="{}" height="4"/>'
                .format(int(x0 if x0 < x1 else x1), int(t2y(notes[j][2])) - 2, int(x1 - x0 if x0 < x1 else x0 - x1)))
        i = i1

    for strp in strps[0] + strps[1]:
        outs.append('<polygon points="{}"/>'
            .format(' '.join(make_strip(x2x, t2y, strp))))
    
    for note in notes:
        outs.append('<use href="#{}" x="{}" y="{}"/>'
            .format(note[0], int(x2x(note[1])), int(t2y(note[2]))))
    outs.append('</svg>')

    return outs

def svg_from_file(fn:str, outdir:str|None, force:bool):
    dirstem, ext = os.path.splitext(fn)
    if ext != '.csv':
        return
    if outdir:
        _, stem = os.path.split(dirstem)
        op = os.path.join(outdir, stem) + '.svg'
    else:
        op = dirstem + '.svg'
    if not force and os.path.exists(op) and os.path.getmtime(op) >= os.path.getmtime(fn):
        return
    outs = make_svg(fn, get_crs(fn))
    with open(op, 'w') as svgf:
        for out in outs:
            svgf.write(out)
    print('saved to {}'.format(op))

if len(sys.argv) < 2:
    print('mltdsong visualizer v0.1')
else:
    args = sys.argv[1:]
    force = False
    while '-f' in args:
        args.remove('-f')
        force = True

    if len(args) > 0 and os.path.isdir(args[0]):
        outdir = args[0]
        args = args[1:]
    else: outdir = None

    for fn in args:
        if os.path.isdir(fn):
            for ffn in os.listdir(fn):
                try: svg_from_file(os.path.join(fn, ffn), outdir, force)
                except Exception as e:
                    print('{} failed'.format(ffn))
                    raise e
        else: svg_from_file(fn, outdir, force)
