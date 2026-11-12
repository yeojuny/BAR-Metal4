#!/usr/bin/env python3
"""Eager/lazy terrain observation DESIGN MODEL differential test.

This does not import, execute, or prove production C++ behavior. It explores a
proposed contract: successful source writes change writer content; explicit
Current/Save/capture seal dirty content, while Inspect/kind do not. Saved states
are immutable and do not own the writer lifetime. writer_version is an abstract per-write identity witness, not the real uint64
uniform revision or physical publication count. Real implementation must also
preserve source validation/ABI, allocation exceptions, consumers and GPU rules.
"""
from dataclasses import dataclass
import argparse
import json
import random
import struct


@dataclass(frozen=True)
class Snapshot:
    generation: int
    content: tuple
    writer_version: int  # Logical invalidation witness, not physical seal/revision count.


class Rejected(Exception):
    pass


class Model:
    def __init__(self, lazy, fault=None):
        self.lazy = lazy
        self.fault = fault
        self.generation = 1
        self.alive = True
        self.enabled = True
        self.values = {}
        self.writer_version = 0
        self.current = Snapshot(1, (), 0)
        self.saved = {}
        self.seals = 0

    def content(self):
        return tuple(sorted(self.values.items()))

    def observe(self):
        if self.current == 'dirty':
            self.current = Snapshot(self.generation, self.content(), self.writer_version)
            self.seals += 1
        return self.current

    def inspect(self):
        # Only immediate provenance/kind, never immutable saved content.
        if self.current == 'dirty' or isinstance(self.current, Snapshot):
            return 'terrain'
        return self.current

    def aligned(self):
        return self.current == 'dirty' or self.current == Snapshot(self.generation, self.content(), self.writer_version)

    def write(self, key, value):
        if not self.alive or not self.enabled:
            raise Rejected('writer unavailable')
        if key == -1:
            return
        if key < 0 or value == 'nonfinite':
            raise Rejected('invalid input')
        if self.current != 'dirty' and not (isinstance(self.current, Snapshot) and self.current.generation == self.generation):
            raise Rejected('not current lifetime')
        if self.values.get(key) == value and self.aligned():
            return
        # Validation finished before mutation. Allocation failure is not modeled.
        self.values[key] = value
        self.writer_version += 1
        if self.lazy:
            self.current = 'dirty'
        else:
            self.current = Snapshot(self.generation, self.content(), self.writer_version)
            self.seals += 1

    def run(self, event):
        op, *args = event
        if op == 'write':
            self.write(*args)
        elif op in ('current', 'save', 'capture'):
            state = self.observe()
            if op == 'save':
                self.saved[args[0]] = state
            if op == 'capture':
                if not self.alive or state != Snapshot(self.generation, self.content(), self.writer_version):
                    raise Rejected('stale capture')
            return state
        elif op in ('inspect', 'kind'):
            before = self.seals
            if self.fault == 'inspection-seals':
                self.observe()
            result = self.inspect()
            assert self.seals == before, 'inspection sealed'
            return result
        elif op == 'restore':
            if args[0] not in self.saved:
                raise Rejected('missing save')
            self.current = self.saved[args[0]]
            if self.fault == 'restore-rewinds-writer' and isinstance(self.current, Snapshot):
                self.values = dict(self.current.content)
                self.writer_version = self.current.writer_version
        elif op == 'other':
            self.current = 'other'
        elif op == 'enable':
            if not self.alive:
                raise Rejected('dead enable')
            self.enabled = True
            # Enable publication is itself a binding/observation boundary.
            self.current = Snapshot(self.generation, self.content(), self.writer_version)
            self.seals += 1
        elif op == 'delete':
            self.alive = False
            self.enabled = False
            if self.inspect() == 'terrain':
                self.current = None
        elif op == 'recreate':
            self.generation += 1
            self.alive = True
            self.enabled = False
            self.values = {}
            self.writer_version = 0
            self.current = None
        elif op == 'reload':
            if not self.alive:
                raise Rejected('dead reload')
            self.generation += 1
            self.current = None  # successful original reload unbinds
        elif op == 'exception':
            raise Rejected('callback exception')
        else:
            raise AssertionError(op)


def result(model, event):
    before = (model.content(), model.generation, model.alive, model.enabled)
    try:
        return ('ok', model.run(event))
    except Rejected as error:
        assert before == (model.content(), model.generation, model.alive, model.enabled), 'rejected operation mutated writer'
        return ('reject', str(error))


def compare(trace, fault=None):
    eager, lazy = Model(False), Model(True, fault)
    for index, event in enumerate(trace):
        left, right = result(eager, event), result(lazy, event)
        assert left == right, (index, event, left, right)
        assert eager.content() == lazy.content(), (index, event, 'writer content')
        assert eager.saved == lazy.saved, (index, event, 'saved immutable states')
        assert eager.inspect() == lazy.inspect(), (index, event, 'binding kind')
    return eager.seals, lazy.seals


def bits(value):
    return struct.pack('!f', value).hex()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seeds', type=int, default=100)
    parser.add_argument('--steps', type=int, default=1000)
    args = parser.parse_args()
    a, b, c = bits(1), bits(2), bits(3)
    directed = [
        [('write', 0, a), ('save', 0), ('write', 0, b), ('restore', 0), ('capture',), ('write', 0, b), ('capture',)],
        [('write', 0, a), ('save', 0), ('write', 0, b), ('write', 0, a), ('restore', 0), ('capture',), ('write', 0, a), ('capture',)],
        [('write', 0, a), ('save', 0), ('other',), ('save', 1), ('restore', 0), ('write', 0, b), ('exception',), ('restore', 1), ('restore', 0), ('capture',), ('write', 0, b), ('capture',)],
        [('write', 0, a), ('inspect',), ('kind',), ('write', 0, b), ('write', 0, c), ('capture',)],
        [('write', 0, a), ('save', 0), ('delete',), ('restore', 0), ('capture',), ('recreate',), ('enable',), ('restore', 0), ('capture',), ('write', 0, b)],
        [('write', 0, a), ('save', 0), ('reload',), ('enable',), ('restore', 0), ('write', 0, a), ('capture',)],
        # Metadata is opaque model data, not a production ABI/type checker.
        [('write', 0, ('float', 4, 1, 1, False, (a, b, c, a))), ('save', 0),
         ('write', 0, ('float', 1, 1, 4, False, (a, b, c, a))), ('save', 1),
         ('write', 0, ('int32', 1, 1, 4, False, (a, b, c, a))), ('capture',),
         ('write', 0, ('float', 2, 2, 1, False, (a, b, c, a))), ('save', 2),
         ('write', 0, ('float', 2, 2, 1, True, (a, b, c, a))), ('capture',)],
        [('write', 0, bits(0.0)), ('save', 0), ('write', 0, bits(-0.0)), ('save', 1), ('write', 0, 'nonfinite'), ('write', -2, a), ('write', -1, 'nonfinite'), ('capture',)],
    ]
    # Directed accept/reject oracles avoid agreeing on the same incorrect model.
    for lazy_mode in (False, True):
        for trace, rejection_index in ((directed[0], 4), (directed[1], 5)):
            model = Model(lazy_mode)
            outcomes = [result(model, event) for event in trace]
            assert outcomes[rejection_index] == ('reject', 'stale capture')
            assert outcomes[-1][0] == 'ok'
    negative_controls = {}
    for fault, trace in [('restore-rewinds-writer', directed[0]), ('inspection-seals', directed[3])]:
        try:
            compare(trace, fault=fault)
        except AssertionError:
            negative_controls[fault] = 'detected'
        else:
            raise AssertionError(('negative control escaped', fault))
    totals = [0, 0]
    for trace in directed:
        counts = compare(trace)
        totals = [x+y for x, y in zip(totals, counts)]
    # The unobserved run above must actually coalesce, not merely pass equality.
    e, l = compare(directed[3])
    assert e == 3 and l == 1
    operations = ['write']*7 + ['save', 'current', 'capture', 'inspect', 'kind', 'restore', 'other', 'enable', 'exception', 'reload', 'delete', 'recreate']
    values = [a, b, c, bits(0.0), bits(-0.0), 'nonfinite']
    for seed in range(args.seeds):
        rng = random.Random(seed)
        trace = []
        for _ in range(args.steps):
            op = rng.choice(operations)
            if op == 'write':
                event = (op, rng.choice([-2, -1, 0, 1, 2]), rng.choice(values))
            elif op in ('save', 'restore'):
                event = (op, rng.randrange(4))
            else:
                event = (op,)
            trace.append(event)
        try:
            counts = compare(trace)
        except AssertionError as failure:
            # Find shortest failing prefix for deterministic reproduction.
            prefix_length = len(trace)
            for length in range(1, len(trace)+1):
                try:
                    compare(trace[:length])
                except AssertionError:
                    prefix_length = length
                    break
            raise AssertionError({'seed': seed, 'failingPrefix': trace[:prefix_length], 'failure': repr(failure)}) from failure
        totals = [x+y for x, y in zip(totals, counts)]
    print(json.dumps({'scope': 'DESIGN MODEL ONLY; not production or allocation/GPU proof', 'passed': True, 'directedTraces': len(directed), 'negativeControls': negative_controls, 'randomSeeds': args.seeds, 'randomEvents': args.seeds*args.steps, 'eagerModeledSeals': totals[0], 'lazyModeledSeals': totals[1], 'unobservedThreeWrites': {'eager': e, 'lazy': l}}, indent=2))


if __name__ == '__main__':
    main()
