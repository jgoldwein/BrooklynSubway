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


if __name__=='__main__':unittest.main()
