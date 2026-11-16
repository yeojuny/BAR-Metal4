#!/usr/bin/env python3
import copy
import unittest
from analyze import analyze, IDENTITY


def rows():
    common = {k: 1 for k in IDENTITY}
    pairs = {47:(16,1),43:(0,16),44:(384,0),45:(16,8),46:(8,1536),48:(1,7),49:(4096,561152),50:(137,256)}
    return [dict(common, id=i+100, parent=0 if k==47 else 100, kind=k, status=0, count_a=a, count_b=b)
            for i,(k,(a,b)) in enumerate(pairs.items())]


def text(data):
    return '\n'.join('[NativeCPUTrace] '+ ' '.join(f'{k}={v}' for k,v in r.items()) for r in data)


class Test(unittest.TestCase):
    def test_valid(self):
        r=analyze(text(rows()));self.assertEqual(r['status'],'VALID_WORK_COUNTS_ONLY')
        self.assertEqual(r['aggregate']['new_buffers'],1);self.assertEqual(r['aggregate']['encodes'],8)
    def test_bad(self):
        valid=rows();damaged=[]
        for i in range(len(valid)):damaged.append(valid[:i]+valid[i+1:])
        for key,value in [('job',2),('status',2),('parent',999),('count_a',-1)]:
            d=copy.deepcopy(valid);d[-1][key]=value;damaged.append(d)
        d=copy.deepcopy(valid);d[5]['count_a']=0;damaged.append(d)
        d=copy.deepcopy(valid);d[-1]['count_a']=257;damaged.append(d)
        d=copy.deepcopy(valid);d[0]['count_b']=0;damaged.append(d)
        damaged.append(valid+valid)
        for d in damaged:
            with self.subTest(data=d):self.assertEqual(analyze(text(d))['status'],'INVALID')
        self.assertEqual(analyze(text(valid)+'\n[NativeCPUTraceBuffer] dropped=1')['status'],'INVALID')
        self.assertEqual(analyze('')['status'],'INVALID')

if __name__=='__main__':unittest.main()
