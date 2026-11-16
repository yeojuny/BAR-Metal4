"""PR7 bounded counter and existing five-second preflight memory reader."""

import importlib.util
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parents[1]
ANALYZER = HERE.parent / 'source/engine/tools/material-storage-pool/analyze.py'
MEMORY_HELPER = HERE / 'tools/proc_memory_v4'
MEMORY_SHA256 = '1d0286ce990eafb902df6fba22026e7f7677e1e1a45b9239fe386f6cf4d481ac'


def storage_trace(info, start_sim, end_sim, mode):
    spec = importlib.util.spec_from_file_location('pr7_storage_analyzer', ANALYZER)
    analyzer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(analyzer)
    result = analyzer.analyze(info)
    frames = [int(re.search(r'\bframe=(\d+)', line).group(1)) for line in info.splitlines()
              if '[NativeCPUTrace]' in line and re.search(r'\bkind=47\b', line)
              and re.search(r'\bframe=(\d+)', line)]
    result['sampledFrames'] = sorted(set(frames))
    result['sampledSummaryRecords'] = len(frames)
    result['window'] = [start_sim, end_sim]
    result['requestedMode'] = mode
    result['windowAndModeValid'] = (
        result['status'] == 'VALID_WORK_COUNTS_ONLY' and bool(frames)
        and all(start_sim <= frame <= end_sim for frame in frames)
        and all(group['enabled'] == (mode == 'ON') for group in result['groups']))
    return result


class MemoryRecorder:
    def __init__(self, base):
        assert hashlib.sha256(MEMORY_HELPER.read_bytes()).hexdigest() == MEMORY_SHA256
        self.base = base
        self.samples = {}
        self.prior = base.harness.preflight
        base.harness.preflight = self.preflight

    def preflight(self, remaining):
        result = self.prior(remaining)
        active = self.base.active
        if not active['prepared']:
            return result
        _, out, _, plan = active['prepared']
        owned = out / 'OWNED.json'
        if not owned.exists():
            return result
        pid = int(json.loads(owned.read_text())['pid'])
        binary = plan['product']['binary']
        start = time.time_ns()
        call = subprocess.run([str(MEMORY_HELPER), str(pid), binary],
                              capture_output=True, text=True, timeout=2)
        end = time.time_ns()
        sample = dict(epochStartNS=start, epochEndNS=end, roundTripUS=(end-start)/1000,
                      pid=pid, exactBinary=binary, exitCode=call.returncode)
        if call.returncode == 0:
            observation = json.loads(call.stdout)
            assert observation['pid'] == pid and observation['path_verified'] is True
            sample['observation'] = observation
            successful = [s['observation'] for s in self.samples.get(str(out), [])
                          if s['exitCode'] == 0]
            if successful:
                first = successful[0]
                assert (observation['proc_start_abstime'], observation['proc_uuid']) == (
                    first['proc_start_abstime'], first['proc_uuid']), 'Owned PID changed identity'
        else:
            sample['error'] = call.stderr.strip()[:300]
        self.samples.setdefault(str(out), []).append(sample)
        return result

    def flush(self, out):
        samples = self.samples.get(str(out), [])
        valid = [x for x in samples if x['exitCode'] == 0]
        window_file = out / 'WINDOW.json'
        window = json.loads(window_file.read_text()) if window_file.exists() else None
        roi = [x for x in valid if window and window['start'] <= x['epochStartNS']
               and x['epochEndNS'] <= window['end']]
        def peak(values, key):
            return max((x['observation'][key] for x in values), default=None)
        receipt = dict(method='Existing 5s harness preflight; pinned proc_pid_rusage V4 owned PID/exact path before and after',
            helper=str(MEMORY_HELPER), helperSHA256=MEMORY_SHA256, samples=samples,
            sampleCount=len(valid), roiSampleCount=len(roi), roiWindowEpochNS=window,
            sampledRunRSSPeakBytes=peak(valid, 'resident_size'),
            sampledRunCurrentFootprintPeakBytes=peak(valid, 'phys_footprint'),
            kernelLifetimeMaxFootprintAtLastSampleBytes=(valid[-1]['observation']['lifetime_max_phys_footprint'] if valid else None),
            sampledROIRSSPeakBytes=peak(roi, 'resident_size'),
            sampledROICurrentFootprintPeakBytes=peak(roi, 'phys_footprint'),
            limits='Five-second sparse sample maxima are not exact instantaneous RSS or ROI peaks. Kernel lifetime max is process-wide and only through last sample; process memory is not GPU-exclusive. Pool-retained bytes overlap owners and are never added to footprint.')
        (out / 'MEMORY.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return receipt
