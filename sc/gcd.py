from ntime import parse_time, format_time

import csv
from fractions import Fraction
import sys
import re

MODE_CALC = 0
MODE_SHOW = 1
MODE_RF = 2

def by_com0(p): return p[0]
def by_com1(p): return p[1]
def by_com2(p): return p[2]

mode = MODE_SHOW
fn_input = None
fn_output = None
den_show = 0
calc_num = 20
calc_candi = []
nom_rf = 0
den_rf = 0

for arg in sys.argv[1:]:
    if arg == "--show": mode = MODE_SHOW
    elif arg == "--rf": mode = MODE_RF
    elif arg == "--calc": mode = MODE_CALC
    else:
        if fn_input == None:
            fn_input = arg
        else:
            if mode == MODE_SHOW: den_show = float(arg)
            elif mode == MODE_RF:
                if nom_rf == 0 or den_rf == 0:
                    m = re.fullmatch("(\\d+)/(\\d+)", arg)
                    if m:
                        nom_rf = int(m.group(1))
                        den_rf = int(m.group(2))
                elif fn_output == None: fn_output = arg
            elif mode == MODE_CALC:
                if re.fullmatch("\\d+", arg): calc_num = int(arg)
                elif re.fullmatch("(\\d+)/(\\d+)", arg):
                    m = re.fullmatch("(\\d+)/(\\d+)", arg)
                    calc_candi.append(Fraction(int(m.group(1)), int(m.group(2))))
                else: raise Exception("unexpected arg: {}".format(arg))

if fn_input == None:
    fn_input = input('filename:')
    
def get_times()->list:
    times = []
    with open(fn_input, newline='') as csvf:
        reader = csv.reader(csvf)
        for line in reader:
            t = parse_time(line[0])
            if t: times.append(t)
            else:
                print('warning time ignored: {}'.format(line[0]))
    times.sort()
    return times

def get_dets(times)->list:
    dets_dict = dict()
    if len(times) > 0:
        t_prev = times[0]
        for t in times[1:]:
            det = t - t_prev
            t_prev = t
            if det in dets_dict: dets_dict[det] += 1
            else: dets_dict[det] = 1

    dets = list(dets_dict.items())
    dets.sort(key = by_com1, reverse = True)
    return dets

if mode == MODE_SHOW:
    dets = get_dets(get_times())
    if den_show == 0:
        for p in dets:
            if den_show == 0 or p[0] > 0 and p[0] < den_show and p[1] >= 10:
                den_show = p[0]
    if den_show == 0:
        for p in dets:
            print('{}'.format(p))
    else:
        for p in dets:
            print('{}/{}'.format(p, p[0] / den_show))
elif mode == MODE_CALC:
    times = get_times()
    dets = get_dets(times)
    if len(dets) == 0 or len(dets) == 0:
        raise Exception("no data")
    diffs = [p[0] for p in dets]
    valid_range_min = min(diffs) - 1
    valid_range_max = max(diffs) + 1
    time_span = times[-1] - times[0] 
    
    def huber(x):
        THRES = 10
        xabs = abs(x)
        if xabs < THRES: return xabs ** 2 / 2
        else: return THRES * (xabs - THRES / 2)
    
    result = []
    repeat = {}
    def add_result(x_frac, unchecked = False):
        if x_frac in repeat: return
        repeat[x_frac] = True
        x = float(x_frac)
        if unchecked or x > valid_range_min and x < valid_range_max:
            sum_huber = sum([huber(p[0] - x * round(p[0] / x)) * p[1] for p in dets])
            diff_span = time_span - round(time_span / x) * x
            result.append((x_frac, x, sum_huber, diff_span))

    for frac in calc_candi:
        add_result(frac, True)
    for i in range(1, 600):
        add_result(Fraction(60000, i))
        add_result(Fraction(30000, i))
        add_result(Fraction(15000, i))
        add_result(Fraction(7500, i))
    
    result.sort(key = by_com2)
    print('(frac, value, huber, first-last)')
    for i in range(len(result)):
        r = result[i]
        if i < calc_num or r[0] in calc_candi:
            print('{}.{}'.format(i, r))
elif mode == MODE_RF:
    if fn_output == None:
        fn_output = input('output filename:')
        
    if nom_rf == 0 or den_rf == 0:
        m = re.fullmatch("(\\d+)/(\\d+)", input('beat ( nom / den ):'))
        if m:
            nom_rf = int(m.group(1))
            den_rf = int(m.group(2))
        if nom_rf == 0 or den_rf == 0:
            raise Exception('invalid beat')

    times = []
    with open(fn_input, newline='') as csvf:
        reader = csv.reader(csvf)
        for line in reader:
            t = parse_time(line[0])
            if t: times.append((t, line[1]))
            else:
                print('warning time ignored: {}'.format(line[0]))

    if len(times) > 0:
        times.sort(key = by_com0)

        t0 = times[0][0]
        result = ["{},{}\n".format(format_time(t0), times[0][1])]

        for t in times[1:]:
            n = round((t[0] - t0) * den_rf / nom_rf)
            result.append("{},{}\n".format(format_time(t0 + n * nom_rf / den_rf), t[1]))

        with open(fn_output, 'w') as of:
            of.writelines(result)
    else: raise Exception("empty csv")
