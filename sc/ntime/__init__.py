import math
import re

def parse_time(s:str)->int|None:
    m = re.fullmatch('(?:(\\d+):)?(\\d+):(\\d+.\\d+)', s)
    if m:
        t = int(m.group(2)) * 60000 + math.floor(float(m.group(3)) * 1000)
        if m.group(1): t += int(m.group(1)) * 3600000
        return t
    m = re.fullmatch('\\d+', s)
    if m:
        return int(m.group(0))
    return None

def format_time(t:int)->str:
    return '{:02}:{:06.3f}'.format(int(t / 60000), t % 60000 / 1000)
