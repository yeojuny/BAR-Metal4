"""Run the frozen G02 Metal parity helper once; never launches a game."""
import argparse
import importlib.util
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
G02 = HERE.parent
SOURCE = G02 / 'validation/gpu_upload_parity.mm'
HELPER = G02 / 'validation/gpu_upload_parity'
OUTPUT = G02 / 'validation/runs/g02_parity01'
SOURCE_SHA = 'd46561400ea53aee6f88613cf0cd89d64c2de3c648943e8fab74931da594b916'
HELPER_SHA = '00d8233a2c1d0235f3d39e92fe678a8d1f3ff7ca82217d9bbd7e8285234b684b'
CANDIDATE_SHA = 'f83dae3cde8cd2a752d1d7ff9fc7629f8a5820f72e3f05306f7c57ab5ec2fca2'
spec = importlib.util.spec_from_file_location('g01_frozen_helper_lifecycle',
    G02.parent / 'ground-decal-index-g01-20261116-01/measurement/execute_first_gate.py')
frozen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(frozen)
frozen.SOURCE = SOURCE
frozen.HELPER = HELPER
frozen.OUTPUT = OUTPUT
frozen.PASS_PATTERN = re.compile(
    r'^PASS cases=32 actual_GroundDecalVertex_Fragment=1 '
    r'old36_separate36_combined36=1 combined_uniform_bytes=368 index_offset=368 '
    r'index_bytes=72 multiinstance=3 visible=(\d+) depthChanged=(\d+) '
    r'stencilChanged=(\d+) degenerateCase=10 color_depth_stencil_byte_equal=32$')

def check():
    assert frozen.sha(SOURCE) == SOURCE_SHA
    assert frozen.sha(HELPER) == HELPER_SHA
    assert frozen.read(HERE / 'PRODUCT.json')['sha256'] == CANDIDATE_SHA
    assert frozen.sha(frozen.read(HERE / 'PRODUCT.json')['binary']) == CANDIDATE_SHA
    assert not OUTPUT.exists(), 'First helper output already exists; no unchanged retry'
    return {'status': 'READY_NO_GPU', 'sourceSHA256': SOURCE_SHA,
            'helperSHA256': HELPER_SHA, 'candidateSHA256': CANDIDATE_SHA,
            'output': str(OUTPUT), 'timeoutSeconds': 180,
            'MetalValidation': 'MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--check-only', action='store_true')
    group.add_argument('--run', action='store_true')
    args = parser.parse_args()
    readiness = check()
    if args.check_only:
        print(readiness, flush=True)
    else:
        result = frozen.run_helper()
        print(result, flush=True)
        raise SystemExit(result['status'] != 'PASS')
