# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Exact filled-area comparison for absolute, axis-aligned Gerber regions.

No coordinate rounding or rasterization. Unsupported commands fail closed.
Used only for printed silkscreen, never copper, masks, drills or outline.
"""
import re


def region_signature(body):
    contours=[];points=[]
    for line in body.splitlines():
        match=re.fullmatch(r'G01X(-?\d+)Y(-?\d+)D0([12])\*',line)
        if not match:return None
        x,y,operation=map(int,match.groups())
        if operation==2:
            if points:contours.append(points)
            points=[(x,y)]
        elif points:points.append((x,y))
        else:return None
    if points:contours.append(points)
    if not contours or sum(map(len,contours))>20000:return None
    edges=[];levels=set()
    for points in contours:
        if len(points)<4 or points[0]!=points[-1]:return None
        for (x1,y1),(x2,y2) in zip(points,points[1:]):
            if x1!=x2 and y1!=y2:return None
            levels.update((y1,y2))
            if y1!=y2:edges.append((x1,min(y1,y2),max(y1,y2)))
    levels=sorted(levels)
    if len(levels)*len(edges)>4000000:return None
    bands=[]
    for low,high in zip(levels,levels[1:]):
        crossings=sorted(x for x,y1,y2 in edges if 2*y1<low+high<2*y2)
        if len(crossings)%2:return None
        # Repeated coincident boundaries cancel under region even/odd fill.
        toggles=[]
        for x in crossings:
            if toggles and toggles[-1]==x:toggles.pop()
            else:toggles.append(x)
        intervals=tuple(zip(toggles[::2],toggles[1::2]))
        if not intervals:continue
        if bands and bands[-1][1]==low and bands[-1][2]==intervals:
            bands[-1]=(bands[-1][0],high,intervals)
        else:bands.append((low,high,intervals))
    return tuple(bands)


def canonical_regions(commands):
    if commands is None:return None
    result=[]
    for item in commands:
        if item[0]!='text':result.append(item);continue
        text=item[1]
        if 'G91*' in text or '%FSLI' in text or '%FSTI' in text:return None
        cursor=0;parts=[]
        for match in re.finditer(r'G36\*\n(.*?)\nG37\*',text,re.S):
            signature=region_signature(match[1])
            region=('raw-region',match[0]) if signature is None else ('filled-region',signature)
            parts.extend((text[cursor:match.start()],region))
            cursor=match.end()
        parts.append(text[cursor:])
        # Consecutive regions with no intervening commands share one polarity.
        # Their painting order is immaterial; keep unsupported regions exact.
        grouped=[];pending=[]
        def flush():
            if pending:
                grouped.append(('region-set',tuple(sorted(pending,key=repr))))
                pending.clear()
        for part in parts:
            if isinstance(part,tuple) and part[0]=='filled-region':pending.append(part)
            elif isinstance(part,str) and not part.strip() and pending:continue
            else:
                flush();grouped.append(part)
        flush()
        result.append(('text-regions',tuple(grouped)))
    return tuple(result)
