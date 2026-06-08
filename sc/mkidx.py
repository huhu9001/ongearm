import json
import os
import sys

if len(sys.argv) < 2:
    print('mltdsong make index v0.1')
else:
    outdir = sys.argv[1]
    songs = {}
    for fn in os.listdir(outdir):
        stem, ext = os.path.splitext(fn)
        if ext != '.svg': continue
        crs_name = stem[-3:]
        if crs_name == '_mm': crs = 0
        elif crs_name == '_6m': crs = 1
        elif crs_name == '_4m': crs = 2
        elif crs_name == '_2m': crs = 3
        elif crs_name == '_2p': crs = 4
        else: continue
        sn = stem[:-3]
        if sn not in songs:
            songs[sn] = [False] * 5
        songs[sn][crs] = True

    with open(os.path.join(outdir, 'sngidx.json'), 'w') as f:
        json.dump(songs, f)
