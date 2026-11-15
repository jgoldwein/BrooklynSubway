import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from monitor_indications import Collector


def report(now, events=(), more=False, gap=False, oldest=1):
    return dict(schemaVersion=1,uptimeMs=now,historyGap=gap,oldestSequence=oldest,
                events=[dict(sequence=n) for n in events],nextAfter=max(events,default=0),more=more)


class CollectorTests(unittest.TestCase):
    def test_pages_and_duplicates(self):
        c=Collector();calls=[]
        def request(after):
            calls.append(after)
            return report(100,[1,2],True) if after==0 else report(101,[2,3])
        records=c.collect(request)
        self.assertEqual(calls,[0,2]);self.assertEqual(c.cursor,3)
        self.assertEqual([x['sequence'] for x in records if x['type']=='ended'],[1,2,3])
    def test_restart_refetches_new_boot(self):
        c=Collector();c.uptime=50000;c.cursor=80;calls=[]
        def request(after):
            calls.append(after);return report(20,[1] if after==0 else [])
        r=c.collect(request);self.assertEqual(calls,[80,0]);self.assertEqual(c.cursor,1)
        self.assertEqual(r[0]['type'],'restart')
    def test_rollover_is_not_restart(self):
        c=Collector();c.uptime=0xfffffff0;c.cursor=80
        r=c.collect(lambda after:report(30,[81]))
        self.assertEqual(c.cursor,81);self.assertFalse(any(x['type']=='restart' for x in r))
    def test_gap_is_explicit(self):
        c=Collector();r=c.collect(lambda after:report(100,[20],gap=True,oldest=20))
        self.assertEqual(r[0]['type'],'history-gap');self.assertEqual(c.cursor,20)
    def test_failed_request_does_not_advance(self):
        c=Collector();c.cursor=12
        with self.assertRaises(OSError):c.collect(lambda after:(_ for _ in ()).throw(OSError('offline')))
        self.assertEqual(c.cursor,12)
    def test_page_bound_and_schema(self):
        c=Collector();c.collect(lambda after:report(100,[after+1],True),max_pages=3)
        self.assertEqual(c.cursor,3)
        with self.assertRaises(ValueError):c.collect(lambda after:dict(schemaVersion=2))


from monitor_stability import StabilityStats, version_at_least


class StabilityTests(unittest.TestCase):
    def payload(self,boot='a',poll=1,success=8,seq=0):
        status=dict(bootId=boot,fetchingFeeds=False,freeInternalHeap=80000,firmwareVersion='0.2.49',resetReason='software',displayState='Fresh')
        monitor=dict(reportTimestamp=poll,feedsSucceeded=success,feedsTotal=8)
        runtime=dict(bootId=boot,oldestFailureSequence=1 if seq else 0,newestFailureSequence=seq,
                     failures=[dict(sequence=i,category='connect/tls-failure') for i in range(1,seq+1)],unexpectedWarmResets=0)
        return status,monitor,runtime
    def test_baseline_is_not_new_failure_or_poll(self):
        stats=StabilityStats();stats.health(*self.payload(seq=2));self.assertEqual(sum(stats.failureKinds.values()),0)
        self.assertEqual(stats.data['completePolls'],0)
        stats.health(*self.payload(poll=2,seq=3));self.assertEqual(sum(stats.failureKinds.values()),1)
        self.assertEqual(stats.data['completePolls'],1)
        stats.health(*self.payload(poll=3,success=6,seq=3));self.assertEqual(stats.data['partialPolls'],1)
    def test_boot_change_and_consistency(self):
        stats=StabilityStats();stats.health(*self.payload())
        stats.health(*self.payload('b',seq=1));self.assertEqual(stats.data['bootChanges'],1)
        self.assertEqual(sum(stats.failureKinds.values()),1)
        status,monitor,runtime=self.payload();runtime['bootId']='other'
        with self.assertRaises(ValueError):stats.health(status,monitor,runtime)
    def test_history_gap_and_persistence_are_explicit(self):
        stats=StabilityStats();stats.health(*self.payload())
        status,monitor,runtime=self.payload(poll=2,seq=12)
        runtime['oldestFailureSequence']=5;runtime['failures']=runtime['failures'][4:]
        stats.health(status,monitor,runtime);self.assertEqual(stats.data['failureHistoryGaps'],1)
        stats.indication(dict(type='sample',active=[dict(index=2,durationMs=301000,line='F')]),300)
        self.assertEqual(stats.summary()['persistentCount'],1)
        self.assertEqual(stats.summary()['persistentIndicationsForReview'][0]['bootId'],'a')
    def test_versions(self):
        self.assertTrue(version_at_least('0.2.49','0.2.49'))
        self.assertTrue(version_at_least('0.2.50','0.2.49'))
        self.assertFalse(version_at_least('0.2.48','0.2.49'))
        self.assertFalse(version_at_least('unknown','0.2.49'))


if __name__=='__main__':unittest.main()
