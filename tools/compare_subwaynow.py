#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Read-only, timestamped MTA/Subway Now comparison. Standard library only.

Capture public feeds; compare native host audit results without changing a PCB.

"""

import argparse, concurrent.futures, hashlib, ipaddress, json, re, socket, ssl, subprocess, time

from pathlib import Path

from urllib.request import Request, urlopen

ROOT=Path(__file__).resolve().parents[1]

def capture(args):

    out=Path(args.out);out.mkdir(parents=True,exist_ok=True)

    context=ssl.create_default_context(cafile=args.ca_file)

    original=socket.getaddrinfo;addresses={}

    if args.doh:

        def resolve(host,port,*rest,**kw):

            try:ipaddress.ip_address(host);literal=True
            except ValueError:literal=False
            if not literal:

                if host not in addresses:

                    request=Request('https://1.1.1.1/dns-query?name='+host+'&type=A',headers={'Accept':'application/dns-json'})

                    with urlopen(request,context=context,timeout=15) as response:data=json.load(response)

                    addresses[host]=next(item['data'] for item in data['Answer'] if item['type']==1)

                host=addresses[host]

            return original(host,port,*rest,**kw)

        socket.getaddrinfo=resolve

    feeds=re.findall(r'\{"([^\"]+)", "(https://[^\"]+)"\}',(ROOT/'include/network_config.h').read_text())

    if len(feeds)!=8:raise ValueError('Expected the eight firmware feed definitions')

    def fetch(item):

        name,url=item;started=time.time()

        try:

            with urlopen(Request(url,headers={'User-Agent':'BrooklynMap-readonly-comparison/1'}),context=context,timeout=20) as response:data=response.read(2000000)

            return name,data,dict(url=url,startedUnix=started,finishedUnix=time.time(),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())

        except Exception as error:return name,None,dict(url=url,startedUnix=started,finishedUnix=time.time(),error=str(error))

    try:

        for cycle in range(args.cycles):

            directory=out/f'cycle-{cycle:02}';directory.mkdir(exist_ok=True);started=time.time()

            jobs=[(name+'.pb',url) for name,url in feeds]+[('subwaynow.json','https://api.subwaynow.app/routes/?detailed=1')]

            if args.board:jobs.append(('board.json',args.board.rstrip('/')+'/api/status'))

            metadata={}

            with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:

                for name,data,info in pool.map(fetch,jobs):

                    metadata[name]=info

                    if data is not None:(directory/name).write_bytes(data)

            (directory/'capture.json').write_text(json.dumps(dict(startedUnix=started,finishedUnix=time.time(),requests=metadata),indent=2))

            print(directory, 'captured',sum('error' not in item for item in metadata.values()),'/',len(jobs),flush=True)

            if cycle+1<args.cycles:time.sleep(max(0,args.interval-(time.time()-started)))

    finally:socket.getaddrinfo=original



def compare(args):

    reports=[]

    for directory in sorted(Path(args.input).glob('cycle-*')):

        capture=json.loads((directory/'capture.json').read_text());now=int(max(info['finishedUnix'] for name,info in capture['requests'].items() if name!='board.json' and 'error' not in info))

        for name,info in capture['requests'].items():
            if 'error' in info:continue
            payload=(directory/name).read_bytes()
            if len(payload)!=info['bytes'] or hashlib.sha256(payload).hexdigest()!=info['sha256']:
                raise ValueError('Capture integrity check failed: '+str(directory/name))
        files=sorted(directory.glob('*.pb'))

        if not files or not (directory/'subwaynow.json').exists():continue

        current=json.loads(subprocess.check_output([str(Path(args.audit).resolve()),str(now),*map(str,files)],text=True))

        (directory/(args.label+'-audit.json')).write_text(json.dumps(current,indent=2))

        web=json.loads((directory/'subwaynow.json').read_text());webtrips={}

        for route in web['routes'].values():

            for direction,trips in route.get('trips',{}).items():

                for trip in trips:

                    stops=sorted(trip.get('stops',{}),key=trip.get('stops',{}).get);previous=trip.get('last_stop_made')

                    following=stops[stops.index(previous)+1] if previous in stops and stops.index(previous)+1<len(stops) else None

                    webtrips[(route['id'],trip['id'])]=dict(previous=previous,next=following,direction=direction)

        routes={route:dict(coveredTrips=0,matchedTrips=0,webPositionAvailable=0,estimatedDepartures=0,approaching=0,reportedStopDiffersFromWebNext=0,webNextMatchesAfter=0,stale=0,unmatched=0,examples=[]) for route in ['2','3','4','5','A','B','C','D','F','G','J','L','M','N','Q','R','S','Z']}

        for name,values in routes.items():
            services=[]
            for reference in (['FS','H'] if name=='S' else [name]):
                info=web['routes'].get(reference)
                if info:services.append(dict(route=reference,status=info.get('status'),scheduled=info.get('scheduled')))
            values['referenceServices']=services
        for feed in current['feeds']:

            for trip in feed['trips']:

                route=routes.get(trip['canonicalRoute'])

                if not route or not(trip['before'] or trip['after']):continue

                route['coveredTrips']+=1

                other=webtrips.get((trip['route'],trip['trip']))

                if not other:route['unmatched']+=1;continue

                route['matchedTrips']+=1

                route['estimatedDepartures']+=int(trip['inferred'])

                route['approaching']+=int(bool(trip['after'] and trip['after']['approaching']))

                route['stale']+=int(trip['afterReason']=='Stale')

                route['webPositionAvailable']+=int(other['next'] is not None)

                route['reportedStopDiffersFromWebNext']+=int(other['next'] is not None and trip['vehicleStop'][:3]!=other['next'])

                route['webNextMatchesAfter']+=int(other['next'] is not None and trip['afterStop'][:3]==other['next'])

                if len(route['examples'])<12:route['examples'].append(dict(trip=trip['trip'],vehicle=trip['vehicleStop'],after=trip['afterStop'],inferred=trip['inferred'],web=other,afterCategory=trip['afterCategory'],afterReason=trip['afterReason']))

        reports.append(dict(cycle=directory.name,referenceUnix=now,subwayNowTimestamp=web.get('timestamp'),feeds=[dict(timestamp=f['timestamp'],decodeStatus=f['decodeStatus']) for f in current['feeds']],routes=routes,sourceErrors={name:info['error'] for name,info in capture['requests'].items() if 'error' in info},pipeline=current.get('pipeline',{})))

    if not reports:raise ValueError('No comparable snapshots were available')
    Path(args.output).write_text(json.dumps(dict(label=args.label,cycles=reports,limitations=['Independent snapshots; compare timestamps, not marker pixels.','The native audit uses merged records; firmware also rejects conflicting duplicate predictions.','No observations means unassessed service, not a passing result.','Subway Now positions are estimates, not ground-truth GPS.']),indent=2))

    for report in reports:

        print(report['cycle'], 'routes with matching mapped trips',sum(v['matchedTrips']>0 for v in report['routes'].values()))

        for route,values in report['routes'].items():print(route,{k:v for k,v in values.items() if k!='examples'})

if __name__=='__main__':

    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)

    cap=sub.add_parser('capture');cap.add_argument('--out',required=True);cap.add_argument('--cycles',type=int,default=3);cap.add_argument('--interval',type=float,default=20);cap.add_argument('--board');cap.add_argument('--doh',action='store_true');cap.add_argument('--ca-file')

    comp=sub.add_parser('compare');comp.add_argument('--input',required=True);comp.add_argument('--audit',required=True);comp.add_argument('--label',default='candidate');comp.add_argument('--output',required=True)

    build=sub.add_parser('build');build.add_argument('--out',required=True)

    args=parser.parse_args()

    if args.command=='build':

        import phase4e_host

        Path(args.out).parent.mkdir(parents=True,exist_ok=True)

        raise SystemExit(phase4e_host.build(ROOT/'tools/route_snapshot_audit.cpp',Path(args.out)))

    if args.command=='capture' and (not 1<=args.cycles<=100 or args.interval<0):parser.error('Use 1-100 cycles and a nonnegative interval')

    capture(args) if args.command=='capture' else compare(args)
