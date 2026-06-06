from ntime import parse_time

import csv
import sys

times = []
if len(sys.argv) > 1:
    fn = sys.argv[1]
else:
    fn = input('filename:')
with open(fn, newline='') as csvf:
    reader = csv.reader(csvf)
    for line in reader:
        t = parse_time(line[0])
        if t: times.append(t)
        else:
            print('warning time ignored: {}'.format(line[0]))
times.sort()

dets = dict()
if len(times) > 0:
    t_prev = times[0]
    for t in times[1:]:
        det = t - t_prev
        t_prev = t
        if det in dets: dets[det] += 1
        else: dets[det] = 1

l = list(dets.items())
def k(p): return p[1]
l.sort(key = k, reverse = True)

if len(sys.argv) > 2: den = float(sys.argv[2])
else:
    den = 0
    for p in l:
        if den == 0 or p[0] > 0 and p[0] < den and p[1] >= 10:
            den = p[0]

if den == 0:
    for p in l:
        print('{}'.format(p))
else:
    for p in l:
        print('{}/{}'.format(p, p[0] / den))
