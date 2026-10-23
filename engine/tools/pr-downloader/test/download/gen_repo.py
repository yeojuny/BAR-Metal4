#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2022 Marek Rusinowski
# SPDX-License-Identifier: GPL-2.0-or-later OR Apache-2.0 OR MIT OR BSL-1.0
"""Builds big repo for performance testing."""
import sys
import argparse
import random
import string
# Whatever, for performance testing only
sys.path.append('..')
import functional_test

def gen_repo(url, dir, num_files, file_size):
	root = functional_test.RapidRoot(url)
	repo = root.add_repo('main')
	archive = repo.add_archive('pkg:1')

	for i in range(num_files):
		chars = string.ascii_letters+string.digits
		name = ''.join([random.choice(chars) for _ in range(10)])
		archive.add_file(name, random.randbytes(file_size))

	root.save(dir)


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Generate repo')
	parser.add_argument('url')
	parser.add_argument('--dir', default='repo')
	parser.add_argument('--num-files', type=int, default=10000)
	parser.add_argument('--file-size', type=int, default=102400)
	args = parser.parse_args()
	gen_repo(args.url, args.dir, args.num_files, args.file_size)
