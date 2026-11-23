#!/usr/bin/env python3
"""Portable ownership tests and real-header ARC syntax; optional Apple test.

--native compiles/runs test_native.mm against the installed Apple SDK. Even that
preparation test submits NO GPU commands and does not replace the engine/game
validation. Linux syntax uses declaration-only stubs, not an Apple SDK.
"""
from __future__ import annotations
import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ENGINE = HERE.parents[1]
RTS = ENGINE / 'rts'


def run(argv, *, env=None, expected_failure=False):
    p = subprocess.run([str(x) for x in argv], text=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, env=env)
    if expected_failure:
        if p.returncode == 0 or 'Assertion' not in p.stdout:
            raise RuntimeError('negative control did not fail its ownership assertion\n' + p.stdout)
        print('unsafe sole-owner negative control: expected assertion failure PASS')
        return 'negative_control_assertion=PASS'
    if p.returncode:
        raise RuntimeError(f'command failed ({p.returncode}): {argv}\n{p.stdout}')
    if p.stdout: print(p.stdout, end='')
    return p.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    compiler = shutil.which('clang++')
    if not compiler: parser.error('clang++ is required')
    common = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation']
    outputs = []
    with tempfile.TemporaryDirectory(prefix='material-storage-') as td:
        tmp = Path(td);exe = tmp/'test_storage'
        run([*common, '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
             '-I', RTS, HERE/'test_storage.cpp', '-o', exe])
        outputs.append(run([exe]))
        outputs.append(run([sys.executable, HERE/'test_analyze.py']))
        syntax = ENGINE/'tools/material-preparation-perf/syntax_material.mm'
        if platform.system() == 'Darwin':
            sdk = subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
            syntax_args = ['-isysroot',sdk]
        else:
            syntax_args = ['-fobjc-runtime=macosx-13.0','-I',ENGINE/'tools/material-preparation-perf/stubs']
        run([*common,'-x','objective-c++','-fobjc-arc','-fsyntax-only',*syntax_args,'-I',RTS,syntax])
        outputs.append('actual material header ARC syntax=PASS; Apple SDK=' + str(platform.system()=='Darwin'))
        # Change only the existing production sole-owner check in a temporary
        # include overlay. A missing consumer pin must be caught, not blessed.
        backend = tmp/'Rendering/Backend';backend.mkdir(parents=True)
        for name in ('NativeRetiredBufferStorage.h','NativeRetiredResourcePool.h'):
            source=(RTS/'Rendering/Backend'/name).read_text()
            if name=='NativeRetiredResourcePool.h':
                old='if (slot.use_count() != 1) continue;'
                assert source.count(old)==1
                source=source.replace(old,'if (false) continue;')
            (backend/name).write_text(source)
        broken = tmp/'unsafe'
        run([*common,'-O0','-I',tmp,'-I',RTS,HERE/'test_storage.cpp','-o',broken])
        outputs.append(run([broken],expected_failure=True))
        if args.native:
            if platform.system()!='Darwin':parser.error('--native requires macOS and an Apple SDK')
            native=tmp/'native'
            run([*common,'-O1','-g','-fobjc-arc','-isysroot',sdk,'-I',RTS,
                 HERE/'test_native.mm','-framework','Metal','-framework','Foundation','-o',native])
            for mode in (0,1):
                for identity in (0,1):
                    env=dict(os.environ,BAR_NATIVE_MATERIAL_STORAGE_POOL=str(mode),
                             BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP=str(identity))
                    outputs.append(run([native],env=env))
    report={'status':'PASS','compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],
            'platform':platform.system(),'native_preparation_test':args.native,
            'full_engine_build':False,'gpu_execution':False,'fps_measured':False,'outputs':outputs}
    if args.output:args.output.write_text(json.dumps(report,indent=2)+'\n')
    else:print(json.dumps(report,indent=2))

if __name__=='__main__':main()
