#!/usr/bin/env python3
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
"""Regression coverage for complete raw-bit determinism acceptance."""

import contextlib
import io
import pathlib
import struct
import tempfile
import unittest

import compare_results


class StrictComparisonTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = pathlib.Path(self.temporary.name)

    def write_results(self, name, records, count=None, mode="NEON", arch="arm64"):
        path = self.directory / name
        count = len(records) if count is None else count
        header = b"SFLT" + struct.pack("<I", 1)
        header += mode.encode() + b"\0" + arch.encode() + b"\0" + struct.pack("<I", count)
        path.write_bytes(header + b"".join(struct.pack("<IcQ", tid, prec.encode(), bits) for tid, prec, bits in records))
        return str(path)

    def compare(self, reference, test):
        with contextlib.redirect_stdout(io.StringIO()):
            return compare_results.stream_compare(reference, test)

    def test_identical_records_across_architecture_labels(self):
        records = [(0, "F", 0x3F800000), (1, "D", 0x3FF0000000000000)]
        reference = self.write_results("reference.bin", records, mode="SSE", arch="x86_64")
        test = self.write_results("test.bin", records)
        self.assertEqual(self.compare(reference, test), 0)

    def test_unequal_complete_counts_reject_matching_prefix(self):
        reference = self.write_results("reference.bin", [(0, "F", 0x3F800000)])
        test = self.write_results("test.bin", [(0, "F", 0x3F800000), (1, "F", 0)])
        self.assertEqual(self.compare(reference, test), 1)

    def test_nan_payloads_and_signed_zeros_are_raw_bit_mismatches(self):
        for prec, ref_bits, test_bits in (
            ("F", 0x7FC00001, 0x7FC00002),
            ("D", 0x7FF8000000000001, 0x7FF8000000000002),
            ("F", 0, 0x80000000),
            ("D", 0, 0x8000000000000000),
            ("F", 0x3F800000, 0x100000000 | 0x3F800000),
            ("F", 0x3F800000, 0x3F800001),
        ):
            with self.subTest(prec=prec, ref_bits=ref_bits, test_bits=test_bits):
                reference = self.write_results("reference.bin", [(0, prec, ref_bits)])
                test = self.write_results("test.bin", [(0, prec, test_bits)])
                self.assertEqual(self.compare(reference, test), 1)

    def test_identical_nan_payloads_match(self):
        records = [(0, "F", 0x7FC00001), (1, "D", 0x7FF8000000000001)]
        reference = self.write_results("reference.bin", records)
        test = self.write_results("test.bin", records)
        self.assertEqual(self.compare(reference, test), 0)

    def test_truncated_extra_and_empty_record_data_are_rejected(self):
        reference = self.write_results("reference.bin", [(0, "F", 0x3F800000)])
        for contents in (
            pathlib.Path(reference).read_bytes()[:-1],
            pathlib.Path(reference).read_bytes()[:-13],
            pathlib.Path(reference).read_bytes() + b"\0",
            pathlib.Path(reference).read_bytes() + struct.pack("<IcQ", 1, b"F", 0),
            pathlib.Path(self.write_results("empty.bin", [])).read_bytes(),
        ):
            with self.subTest(length=len(contents)):
                path = self.directory / "test.bin"
                path.write_bytes(contents)
                with self.assertRaises(ValueError):
                    self.compare(reference, str(path))
                with self.assertRaises(ValueError):
                    compare_results.compute_hash(str(path))

    def test_ids_and_precision_must_match(self):
        reference = self.write_results("reference.bin", [(0, "F", 0), (1, "F", 0)])
        for records in ([(0, "F", 0), (2, "F", 0)], [(0, "F", 0), (1, "D", 0)]):
            test = self.write_results("test.bin", records)
            self.assertEqual(self.compare(reference, test), 1)

    def test_incomplete_and_unterminated_headers_are_rejected(self):
        for data in (b"SFLT", b"SFLT\1\0\0\0NEON", b"SFLT\1\0\0\0" + b"N" * 256):
            with self.subTest(length=len(data)):
                with self.assertRaises(ValueError):
                    compare_results.read_header(io.BytesIO(data))

    def test_invalid_precision_is_rejected(self):
        reference = self.write_results("reference.bin", [(0, "X", 0)])
        test = self.write_results("test.bin", [(0, "X", 0)])
        with self.assertRaises(ValueError):
            self.compare(reference, test)


if __name__ == "__main__":
    unittest.main()
