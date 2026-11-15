#!/usr/bin/env python3
"""Verify that every archive member is a current loose object; never edits a build."""
import argparse, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
def sha(data):return hashlib.sha256(data).hexdigest()
objects={}
for f in args.build.rglob('*.o'):
 objects.setdefault(f.name,[]).append(dict(path=str(f.relative_to(args.build)),sha256=sha(f.read_bytes())))
archives=[];mismatches=[]
for f in args.build.rglob('*.a'):
 data=f.read_bytes()
 if not data.startswith(b'!<arch>\n'):continue
 pos=8;members=[]
 while pos<len(data):
  h=data[pos:pos+60];size=int(h[48:58]);name=h[:16].decode().strip();payload=data[pos+60:pos+60+size]
  if name.startswith('#1/'):
   n=int(name[3:]);name=payload[:n].rstrip(b'\0').decode();payload=payload[n:]
  name=name.rstrip('/')
  if name.endswith('.o'):
   digest=sha(payload);candidates=objects.get(name,[])
   row=dict(name=name,sha256=digest,matchesCurrentObject=any(x['sha256']==digest for x in candidates),objects=candidates)
   members.append(row)
   if not row['matchesCurrentObject']:mismatches.append(dict(archive=str(f.relative_to(args.build)),**row))
  pos+=60+size+size%2
 archives.append(dict(path=str(f.relative_to(args.build)),sha256=sha(data),members=members))
report=dict(status='PASS' if not mismatches else 'FAIL_STALE_ARCHIVE_MEMBERS',scope='Archive-to-loose-object identity only; fresh build and source receipts establish source closure separately',objectCount=sum(map(len,objects.values())),archiveCount=len(archives),mismatchCount=len(mismatches),mismatches=mismatches,archives=archives)
args.output.write_text(json.dumps(report,indent=2)+'\n');print({k:report[k] for k in ('status','objectCount','archiveCount','mismatchCount')})
raise SystemExit(bool(mismatches))
