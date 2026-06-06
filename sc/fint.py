from ntime import parse_time

import csv
import sys


if len(sys.argv) > 1:
    fn = sys.argv[1]
else:
    fn = input('filename:')

if len(sys.argv) > 2:
    dif = int(sys.argv[2])
else:
    dif = int(input('diff:'))

with open(fn, newline='') as csvf:
    csvl = csvf.readlines()
    
reader = csv.reader(csvl)
result = []
n = 0
for line in reader:
    t = parse_time(line[0])
    if t: result.append((t, n))
    else:
        print('warning time ignored: {}'.format(line[0]))
    n += 1
result.sort()

if len(result) == 0:
    print('warning no note')
else:
    r_prev = result[0]
    for r in result[1:]:
        if r[0] - r_prev[0] == dif:
            print('{}{}----'.format(csvl[r_prev[1]], csvl[r[1]]))
        r_prev = r