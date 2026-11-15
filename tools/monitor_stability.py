#!/usr/bin/env python3
"""Bounded, read-only stability capture for Brooklyn firmware 0.2.49+.

Records local JSONL plus a rolling JSON summary. Does not reboot, change settings,
run tests or install firmware. Keep this computer awake and on the device LAN.
"""
import argparse
import collections
import datetime as dt
import json
from pathlib import Path
import time
import urllib.request
import urllib.parse
from monitor_indications import Collector


def version_at_least(actual, required):
    try:
        a=tuple(int(x) for x in actual.split('.'));b=tuple(int(x) for x in required.split('.'))
        return len(a)==len(b)==3 and a>=b
    except (ValueError, AttributeError):
        return False


class StabilityStats:
    def __init__(self):
        self.data=dict(healthSamples=0,errors=0,coverageGaps=0,bootChanges=0,
                      completePolls=0,partialPolls=0,emptyPolls=0,indicationEndings=0,
                      indicationHistoryGaps=0,indicationRestartNotices=0,failureHistoryGaps=0)
        self.reasons=collections.Counter();self.failureKinds=collections.Counter();self.tlsCodes=collections.Counter();self.httpCodes=collections.Counter()
        self.idle=[];self.last_poll=None;self.boot=None;self.failure_seq=0
        self.longest=[];self.persistent={}

    def health(self,status,monitor,runtime):
        if status['bootId']!=runtime['bootId']:
            raise ValueError('Board restarted between health requests; sample not combined')
        self.data['healthSamples']+=1
        boot=runtime['bootId'];initial=self.boot is None
        if self.boot is not None and boot!=self.boot:
            self.data['bootChanges']+=1;self.last_poll=None;self.failure_seq=0;self.idle=[]
        self.boot=boot
        if initial:self.failure_seq=runtime['newestFailureSequence']
        if not status.get('fetchingFeeds'):
            self.idle.append(status['freeInternalHeap'])
            if len(self.idle)>40:self.idle=self.idle[:20]+self.idle[-20:]
        poll=(boot,monitor['reportTimestamp'])
        if initial:self.last_poll=poll
        if monitor['reportTimestamp'] and poll!=self.last_poll:
            key='completePolls' if monitor['feedsSucceeded']==monitor['feedsTotal'] else 'partialPolls' if monitor['feedsSucceeded'] else 'emptyPolls'
            self.data[key]+=1;self.last_poll=poll
        oldest=runtime['oldestFailureSequence']
        if not initial and oldest and self.failure_seq<oldest-1:self.data['failureHistoryGaps']+=1
        for event in runtime['failures']:
            if event['sequence']>self.failure_seq:
                self.failureKinds[event['category']]+=1
                self.httpCodes[str(event.get('httpCode','not-reported'))]+=1
                if event['category']=='connect/tls-failure':
                    code=event.get('tlsError');self.tlsCodes['not-reported' if code is None else str(code)]+=1
        self.failure_seq=runtime['newestFailureSequence']
        for key in ['minimumInternalHeap','largestInternalBlock','workerStackMinimumBytes','loopStackMinimumBytes','liveStackMinimumBytes']:
            value=runtime.get(key,0)
            if value:self.data['lowestObserved_'+key]=min(self.data.get('lowestObserved_'+key,value),value)
        self.data['latestHealth']=dict(bootId=boot,firmware=status['firmwareVersion'],resetReason=status['resetReason'],
             displayState=status['displayState'],freeInternalHeap=status['freeInternalHeap'],
             unexpectedWarmResets=runtime['unexpectedWarmResets'])

    def indication(self,record,persistent_seconds):
        kind=record['type']
        if kind=='ended':
            self.data['indicationEndings']+=1;self.reasons[record['reason']]+=1
            ms=record['durationMs'];self.data['durationSumMs']=self.data.get('durationSumMs',0)+ms
            self.data['shortestEndedMs']=min(self.data.get('shortestEndedMs',ms),ms)
            self.data['longestEndedMs']=max(self.data.get('longestEndedMs',ms),ms)
            self.longest=sorted(self.longest+[record],key=lambda x:x['durationMs'],reverse=True)[:12]
        elif kind=='history-gap':self.data['indicationHistoryGaps']+=1
        elif kind=='restart':self.data['indicationRestartNotices']+=1
        elif kind=='sample':
            for active in record['active']:
                if active['durationMs']>=persistent_seconds*1000:
                    key=(self.boot,active['index']);old=self.persistent.get(key)
                    if old is None or active['durationMs']>old['durationMs']:self.persistent[key]={**active,'bootId':self.boot}
        elif kind=='error':self.data['errors']+=1

    def summary(self):
        result=dict(self.data);result['endingReasons']=dict(self.reasons);result['feedFailureCategories']=dict(self.failureKinds);result['tlsErrorCodes']=dict(self.tlsCodes);result['httpCodes']=dict(self.httpCodes)
        result['longestCompletedIndications']=self.longest
        result['persistentIndicationsForReview']=sorted(self.persistent.values(),key=lambda x:x['durationMs'],reverse=True)[:12]
        result['persistentCount']=len(self.persistent)
        result['activeSamplingLimit']=12
        if self.idle:
            result['firstIdleHeapMean']=sum(self.idle[:20])/min(20,len(self.idle))
            result['lastIdleHeapMean']=sum(self.idle[-20:])/min(20,len(self.idle))
        return result


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--url',default='http://10.1.1.56')
    ap.add_argument('--seconds',type=float,default=28800)
    ap.add_argument('--interval',type=float,default=2)
    ap.add_argument('--health-interval',type=float,default=30)
    ap.add_argument('--persistent-seconds',type=float,default=300)
    ap.add_argument('--require-version',default='0.2.49')
    ap.add_argument('--expected-device-id')
    ap.add_argument('--start-at',help='ISO timestamp with explicit offset, for example 2026-11-15T22:00:00-05:00')
    ap.add_argument('--ready-wait-seconds',type=float,default=1800)
    ap.add_argument('--output-dir',type=Path,required=True)
    args=ap.parse_args()
    if not 0<args.seconds<=86400 or args.interval<1 or args.health_interval<5:ap.error('Use duration <=24 h, interval >=1 s and health interval >=5 s')
    if urllib.parse.urlsplit(args.url).scheme not in ('http','https'):ap.error('Use an HTTP or HTTPS URL')
    scheduled=dt.datetime.fromisoformat(args.start_at) if args.start_at else dt.datetime.now(dt.timezone.utc)
    if scheduled.tzinfo is None:ap.error('start-at needs a timezone offset')
    folder=args.output_dir;folder.mkdir(parents=True,exist_ok=True)
    summary_file=folder/'summary.json';stats=StabilityStats();collector=Collector()
    state=dict(state='waiting-for-start',scheduledStart=scheduled.isoformat(),requestedSeconds=args.seconds,
               url=args.url,expectedDeviceId=args.expected_device_id,complete=False)

    def save():
        temp=folder/'summary.tmp';temp.write_text(json.dumps({**state,**stats.summary()},indent=2),encoding='utf-8');temp.replace(summary_file)

    def get(endpoint):
        with urllib.request.urlopen(args.url.rstrip('/')+'/api/'+endpoint,timeout=8) as response:return json.load(response)

    save()
    while dt.datetime.now(dt.timezone.utc)<scheduled:
        time.sleep(min(30,max(0,(scheduled-dt.datetime.now(dt.timezone.utc)).total_seconds())))
    state['state']='waiting-for-required-firmware';save()
    ready_deadline=time.monotonic()+args.ready_wait_seconds
    pinned=args.expected_device_id
    while True:
        try:
            status=get('status')
            if version_at_least(status['firmwareVersion'],args.require_version) and status.get('deviceId'):
                if pinned and status['deviceId']!=pinned:
                    state.update(state='target-mismatch',actualDeviceId=status['deviceId']);save();return 2
                pinned=status['deviceId'];break
        except Exception as error:state['lastWaitingError']=str(error)
        if time.monotonic()>=ready_deadline:
            state['state']='required-firmware-unavailable';save();return 1
        save();time.sleep(min(30,max(0,ready_deadline-time.monotonic())))
    state.update(state='running',deviceId=pinned,startedUtc=dt.datetime.now(dt.timezone.utc).isoformat())
    deadline=time.monotonic()+args.seconds;next_health=0;last_sample=time.monotonic();next_save=0;baseline_ready=False
    try:
        with (folder/'capture.jsonl').open('w',encoding='utf-8') as log:
            while time.monotonic()<deadline:
                now=time.monotonic();records=[];stop_for_target=False
                if now-last_sample>max(15,args.health_interval*3):
                    stats.data['coverageGaps']+=1;records.append(dict(type='coverage-gap',seconds=now-last_sample))
                last_sample=now
                try:
                    if now>=next_health:
                        status=get('status');monitor=get('monitor');runtime=get('runtime-diagnostics')
                        if status.get('deviceId')!=pinned or runtime.get('deviceId')!=pinned:
                            raise RuntimeError('TARGET_CHANGED')
                        old_boot=stats.boot;stats.health(status,monitor,runtime)
                        if old_boot is not None and stats.boot!=old_boot:collector=Collector()
                        records.append(dict(type='health',status=status,monitor=monitor,runtime=runtime))
                        next_health=now+args.health_interval
                    if not baseline_ready:
                        baseline=get('indications');collector.cursor=baseline['newestSequence'];collector.uptime=baseline['uptimeMs']
                        records.append(dict(type='baseline-indications',report=baseline));baseline_ready=True
                    for record in collector.collect(lambda cursor:get('indications?after='+str(cursor))):
                        stats.indication(record,args.persistent_seconds)
                        if record['type']=='sample':record={k:v for k,v in record.items() if k!='events'}
                        records.append(record)
                except Exception as error:
                    stats.data['errors']+=1;records.append(dict(type='error',message=str(error)))
                    if str(error)=='TARGET_CHANGED':state['state']='target-changed';stop_for_target=True
                for record in records:
                    record['observedUtc']=dt.datetime.now(dt.timezone.utc).isoformat();log.write(json.dumps(record)+'\n')
                log.flush()
                if stop_for_target:break
                if now>=next_save:save();next_save=now+60
                time.sleep(min(args.interval,max(0,deadline-time.monotonic())))
            else:state.update(state='finished',complete=True)
    except KeyboardInterrupt:state['state']='stopped-by-operator'
    state['endedUtc']=dt.datetime.now(dt.timezone.utc).isoformat();save()
    print(json.dumps(dict(summary=str(summary_file),state=state['state'])),flush=True)
    return 0 if state['complete'] else 1


if __name__=='__main__':raise SystemExit(main())
