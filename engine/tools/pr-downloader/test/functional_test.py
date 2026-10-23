#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2022 Marek Rusinowski
# SPDX-License-Identifier: GPL-2.0-or-later OR Apache-2.0 OR MIT OR BSL-1.0
#
# mypy: warn-unused-configs, disallow-any-generics, disallow-subclassing-any,
# mypy: disallow-untyped-calls, disallow-untyped-defs, disallow-incomplete-defs,
# mypy: check-untyped-defs, disallow-untyped-decorators, no-implicit-optional,
# mypy: warn-redundant-casts, warn-unused-ignores, warn-return-any,
# mypy: no-implicit-reexport, strict-equality, disallow-any-expr
#
# File formatted with `python3 -m yapf --style=google`.
#
"""Functional tests for pr-downloader.

The tests build a rapid repo in temporary directory, host it from HTTP server
on localhost and run the pr-downloader to verify the behavior.
"""

from __future__ import annotations
from contextlib import contextmanager, AbstractContextManager
from dataclasses import dataclass, field
from http import HTTPStatus
from typing import cast, Callable, ClassVar, Generator, Iterable, Optional, BinaryIO
import argparse
import binascii
import gzip
import hashlib
import http.server
import os
import os.path
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest


class RapidFile:
    """Base class for all files stores under Rapid root directory."""
    __final_hash: Optional[bytes]  # valid only after call to get_contents

    def serialize(self) -> bytes:
        """Produces the uncompressed contents of the file."""
        ...

    def rapid_filename(self) -> str:
        ...

    # -> (subdirectory to use for children, child rapid files)
    def children_files(self) -> tuple[str, Iterable[RapidFile]]:
        ...

    @property
    def final_hash(self) -> bytes:
        if self.__final_hash is None:
            self.get_contents()
        assert self.__final_hash is not None
        return self.__final_hash

    def get_contents(self) -> bytes:
        """Generates final compressed file and caches hash."""
        contents = gzip.compress(self.serialize(), compresslevel=3, mtime=0)
        self.__final_hash = hashlib.blake2b(contents).digest()
        return contents

    def traverse(self) -> Iterable[tuple[str, RapidFile]]:
        """Traverses through full tree of RapidFile children."""
        yield '', self
        sub, children = self.children_files()
        for child in children:
            for d, rf in child.traverse():
                yield os.path.join(sub, d), rf

    def is_identical(self, file_path: str) -> bool:
        with open(file_path, 'rb') as f:
            return hashlib.blake2b(f.read()).digest() == self.final_hash


@dataclass
class ArchiveFile(RapidFile):
    """Represents File referenced by in the Sdp archive stored in Rapid pool."""
    filename: str
    md5: bytes
    crc32: int
    size: int
    contents: bytes

    def serialize(self) -> bytes:
        return self.contents

    def rapid_filename(self) -> str:
        b = self.md5.hex()
        return os.path.join('pool', b[0:2], f'{b[2:]}.gz')

    def children_files(self) -> tuple[str, Iterable[RapidFile]]:
        return '', []


@dataclass
class Archive(RapidFile):
    """Represents Sdp index file stored in the repository."""
    shortname: str
    depends: list[str]
    name: str
    # filename -> ArchiveFile
    files: dict[str, ArchiveFile] = field(default_factory=dict)

    def serialize(self) -> bytes:
        out = []
        for n, f in sorted(list(self.files.items())):
            assert n == f.filename
            filename_bytes = f.filename.encode()
            out.append(struct.pack('B', len(filename_bytes)))
            out.append(filename_bytes)
            out.append(f.md5)
            out.append(struct.pack('>II', f.crc32, f.size))
        return b''.join(out)

    def get_md5(self) -> str:
        digest = hashlib.md5()
        for _, f in sorted(list(self.files.items())):
            digest.update(hashlib.md5(f.filename.encode()).digest())
            digest.update(f.md5)
        return digest.hexdigest()

    def rapid_filename(self) -> str:
        return os.path.join('packages', f'{self.get_md5()}.sdp')

    def children_files(self) -> tuple[str, Iterable[RapidFile]]:
        return '', self.files.values()

    def add_file(self, filename: str, contents: bytes) -> ArchiveFile:
        assert len(filename) > 0 and filename[0] != '/'
        assert filename not in self.files
        assert len(contents) > 0
        file = ArchiveFile(filename=filename,
                           md5=hashlib.md5(contents).digest(),
                           crc32=binascii.crc32(contents),
                           size=len(contents),
                           contents=contents)
        self.files[filename] = file
        return file


@dataclass
class Repo(RapidFile):
    """Represents Rapid repository containing set of archives."""
    shortname: str
    base_url: str
    # shortname -> Archive
    archives: dict[str, Archive] = field(default_factory=dict)

    def serialize(self) -> bytes:
        out = []
        for s, a in sorted(list(self.archives.items())):
            assert s == a.shortname
            out.append(
                f'{a.shortname},{a.get_md5()},{"|".join(a.depends)},{a.name}\n')
        return ''.join(out).encode()

    def rapid_filename(self) -> str:
        return os.path.join(self.shortname, 'versions.gz')

    def children_files(self) -> tuple[str, Iterable[RapidFile]]:
        return self.shortname, self.archives.values()

    def add_archive(self,
                    tag: str,
                    name: Optional[str] = None,
                    depends: list[str] = []) -> Archive:
        if depends == []:
            depends = []  # we must have new fresh object
        assert len(tag) > 0
        shortname = f'{self.shortname}:{tag}'
        assert shortname not in self.archives
        archive = Archive(shortname=shortname,
                          depends=depends,
                          name=shortname if name is None else name)
        self.archives[shortname] = archive
        return archive


class RapidRoot(RapidFile):
    """Base main index storing references to all available Rapid repos.

    This is the only object that should be created directly, all other should
    be created using respective add_* functions.
    """
    base_url: str
    # shortname -> Repo
    repos: dict[str, Repo]

    def __init__(self, base_url: str):
        assert base_url[-1] != '/'
        self.base_url = base_url
        self.repos = dict()

    def add_repo(self, shortname: str) -> Repo:
        assert len(shortname) > 0 and shortname not in self.repos
        repo = Repo(shortname=shortname,
                    base_url=f'{self.base_url}/{shortname}')
        self.repos[shortname] = repo
        return repo

    def serialize(self) -> bytes:
        out = []
        for shortname, repo in sorted(list(self.repos.items())):
            assert shortname == repo.shortname
            out.append(f'{repo.shortname},{repo.base_url},,\n')
        return ''.join(out).encode()

    def rapid_filename(self) -> str:
        return 'repos.gz'

    def children_files(self) -> tuple[str, Iterable[RapidFile]]:
        return '', self.repos.values()

    def save(self, directory: str) -> None:
        """Saves the created in-memory rapid repo to `directory`."""
        for sub, f in self.traverse():
            path = os.path.join(directory, sub, f.rapid_filename())
            if os.path.dirname(path):
                os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as out:
                out.write(f.get_contents())


class HTTPHandler(http.server.SimpleHTTPRequestHandler):
    server: TestingHTTPServer

    def __init__(self, request: bytes, client_address: tuple[str, int],
                 server: TestingHTTPServer, directory: str):
        super().__init__(request, client_address, server, directory=directory)

    def get_path_etag(self) -> str:
        path = self.translate_path(self.path)
        with open(path, 'rb') as f:
            return f'"{hashlib.blake2b(f.read()).hexdigest()}"'

    def get_request_etag(self) -> Optional[str]:
        return self.headers.get("If-None-Match")

    def send_head(self) -> Optional[BinaryIO]:
        for l, r in zip(self.server._resolver_locks, self.server._resolvers):
            # Call resolver under lock because each request is handled
            # in a separate thread so we need to make sure that there
            # aren't any data races if resolver modifies some state.
            with l:
                handled, f = r(self)
            if handled:
                return f

        path = self.translate_path(self.path)
        try:
            f = open(path, 'rb')
        except OSError:
            self.send_error(HTTPStatus.NOT_FOUND, "File not found")
            return None
        try:
            etag = self.get_path_etag()
            if self.get_request_etag() == etag:
                self.send_response(HTTPStatus.NOT_MODIFIED)
                self.end_headers()
                f.close()
                return None
            self.send_response(HTTPStatus.OK)
            self.send_header("ETag", etag)
            self.send_header("Content-Type", self.guess_type(path))
            self.send_header("Content-Length", str(os.fstat(f.fileno())[6]))
            self.end_headers()
            return f
        except:
            f.close()
            raise

    def do_POST(self) -> None:
        # All POSTSs from pr-downloader are only for the streamer, and
        # this function does a very basic simulation of execution of
        # the real streamer cgi program.

        path, query = self.path.rsplit('?', 1)
        if not path.endswith('streamer.cgi'):
            self.send_error(HTTPStatus.NOT_FOUND, 'no streamer')
            return
        if self.server.rapid is None:
            self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR)
            return
        shortname = path.split('/')[1]
        if shortname not in self.server.rapid.repos:
            self.send_error(HTTPStatus.NOT_FOUND, 'no repo')
            return
        for a in self.server.rapid.repos[shortname].archives.values():
            if a.get_md5() == query:
                archive = a
                break
        else:
            self.send_error(HTTPStatus.NOT_FOUND, 'no archive')
            return

        length = int(cast(str, self.headers.get('content-length')))
        data = self.rfile.read(length)
        files_to_get = []
        for b in gzip.decompress(data):
            files_to_get.extend([bool(b & (1 << i)) for i in range(8)])

        out = []
        files_sorted = [f for _, f in sorted(list(archive.files.items()))]
        for get, f in zip(files_to_get, files_sorted):
            if not get:
                continue
            data = f.get_contents()
            out.append(struct.pack('>I', len(data)))
            out.append(data)

        result = b''.join(out)

        self.send_response(HTTPStatus.OK, "Ok")
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(len(result)))
        self.end_headers()
        self.wfile.write(result)


Resolver = Callable[[HTTPHandler], tuple[bool, Optional[BinaryIO]]]


class TestingHTTPServer(http.server.ThreadingHTTPServer):
    directory: str
    rapid: Optional[RapidRoot]
    _resolvers: list[Resolver]
    _resolver_locks: list[threading.Lock]

    def __init__(self, directory: str) -> None:
        self.directory = directory
        self._resolvers = []
        self._resolver_locks = []
        self.rapid = None
        super().__init__(('localhost', 0), self.create_handler)

    def create_handler(self, request: bytes, client_address: tuple[str, int],
                       server: TestingHTTPServer) -> HTTPHandler:
        return HTTPHandler(request,
                           client_address,
                           server,
                           directory=self.directory)

    def add_resolver(self, resolver: Resolver) -> None:
        self._resolvers.append(resolver)
        self._resolver_locks.append(threading.Lock())

    def clear_resolvers(self) -> None:
        self._resolvers = []
        self._resolver_locks = []

    @contextmanager
    def serve(self) -> Generator[None, None, None]:
        t = threading.Thread(
            target=lambda: self.serve_forever(poll_interval=0.02))
        t.start()
        try:
            yield
        finally:
            self.shutdown()
            t.join()


# Related closed issue upstream: https://bugs.python.org/issue25024
def create_temp_dir(suffix: Optional[str] = None,
                    prefix: Optional[str] = None,
                    dir: Optional[str] = None,
                    delete: bool = True) -> AbstractContextManager[str]:
    """Wrapper to support not-deleting temporary directory."""
    if delete:
        return tempfile.TemporaryDirectory(suffix=suffix,
                                           prefix=prefix,
                                           dir=dir)

    @contextmanager
    def h() -> Generator[str, None, None]:
        yield tempfile.mkdtemp(suffix=suffix, prefix=prefix, dir=dir)

    return h()


def fail_requests_resolver(paths: dict[str, HTTPStatus]) -> Resolver:

    def resolver(handler: HTTPHandler) -> tuple[bool, Optional[BinaryIO]]:
        if handler.path in paths:
            handler.send_error(paths[handler.path])
            return True, None
        return False, None

    return resolver


class TestDownloading(unittest.TestCase):
    pr_downloader_path: ClassVar[str]
    keep_temp_files: ClassVar[bool]
    coverage_profiles_path: ClassVar[Optional[str]]
    rapid: RapidRoot
    serving_root: str
    dest_root: str
    server: TestingHTTPServer

    def call_rapid_download(self,
                            shortnames: str | list[str],
                            use_streamer: bool = False) -> int:
        with tempfile.NamedTemporaryFile(
                prefix='pr-run-', delete=not self.keep_temp_files) as out:
            if self.keep_temp_files:
                print(f'run output: {out.name}')
            env = {
                'PRD_RAPID_REPO_MASTER':
                    f'{self.rapid.base_url}/{self.rapid.rapid_filename()}',
                'PRD_RAPID_USE_STREAMER':
                    'true' if use_streamer else 'false',
            }
            env.update(os.environ)
            if self.coverage_profiles_path is not None:
                env['LLVM_PROFILE_FILE'] = os.path.join(
                    self.coverage_profiles_path,
                    f'{os.path.basename(out.name)}.profraw')

            if isinstance(shortnames, str):
                shortnames = [shortnames]
            dl_args = []
            for sn in shortnames:
                dl_args.extend(['--download-game', sn])

            res = subprocess.run([
                self.pr_downloader_path, '--filesystem-writepath',
                self.dest_root
            ] + dl_args,
                                 stderr=subprocess.STDOUT,
                                 stdout=out,
                                 timeout=10,
                                 env=env)
            return res.returncode

    def verify_downloaded_rapid(self, archive: str | Archive) -> bool:
        if isinstance(archive, str):
            repo = archive.split(':', 1)[0]
            archive = self.rapid.repos[repo].archives[archive]
        # We ignore subdir because in destination it's always flat structure.
        for _, rf in archive.traverse():
            dest_file = os.path.join(self.dest_root, rf.rapid_filename())
            if not os.path.exists(dest_file) or not rf.is_identical(dest_file):
                return False
        return True

    def clear_dest_root(self) -> None:
        for f in os.listdir(self.dest_root):
            path = os.path.join(self.dest_root, f)
            if os.path.isdir(path):
                shutil.rmtree(path)
            else:
                os.remove(path)

    def run(self, result: Optional[unittest.TestResult] = None) -> None:
        with create_temp_dir(prefix='pr-serv-',
                             delete=not self.keep_temp_files) as serving_root,\
             create_temp_dir(prefix='pr-dest-',
                             delete=not self.keep_temp_files) as dest_root,\
             TestingHTTPServer(serving_root) as server:
            self.serving_root = serving_root
            self.dest_root = dest_root
            if self.keep_temp_files:
                print(f'serving: {serving_root}')
                print(f'destination: {dest_root}')
            self.server = server
            self.rapid = RapidRoot(
                f'http://{server.server_address[0]}:{server.server_address[1]}')
            self.server.rapid = self.rapid
            super().run(result)

    def _base_simple_download_ok(self, use_streamer: bool) -> None:
        repo = self.rapid.add_repo('testrepo')
        archive = repo.add_archive('pkg:1')
        archive.add_file('a.txt', b'a')
        archive.add_file('b.txt', b'aa')
        repo = self.rapid.add_repo('testrepo2')
        archive = repo.add_archive('pkg:2')
        archive.add_file('a.txt', b'a')
        archive.add_file('b.txt', b'aacc')
        archive = repo.add_archive('pkg:3')
        archive.add_file('a.txt', b'aeee')
        archive = repo.add_archive('pkg:4', 'Package 4')
        archive.add_file('a.txt', b'aeeessss')
        self.rapid.save(self.serving_root)

        with self.server.serve():
            self.assertEqual(
                self.call_rapid_download('testrepo:pkg:1',
                                         use_streamer=use_streamer), 0)
            self.assertEqual(
                self.call_rapid_download('rapid://testrepo2:pkg:2',
                                         use_streamer=use_streamer), 0)
            self.assertEqual(
                self.call_rapid_download('Package 4',
                                         use_streamer=use_streamer), 0)

        self.assertTrue(self.verify_downloaded_rapid('testrepo:pkg:1'))
        self.assertTrue(self.verify_downloaded_rapid('testrepo2:pkg:2'))
        self.assertTrue(self.verify_downloaded_rapid('testrepo2:pkg:4'))
        self.assertFalse(self.verify_downloaded_rapid('testrepo2:pkg:3'))

    def test_simple_download_ok(self) -> None:
        self._base_simple_download_ok(use_streamer=False)

    def test_simple_download_ok_streamer(self) -> None:
        self._base_simple_download_ok(use_streamer=True)

    def test_all_download_failures_fail(self) -> None:
        repo = self.rapid.add_repo('testrepo')
        archive = repo.add_archive('pkg:1')
        archive.add_file('a.txt', b'a')
        archive.add_file('b.txt', b'aa')
        self.rapid.save(self.serving_root)

        # Make sure that when no errors are injected it succeeds.
        with self.server.serve():
            self.assertEqual(self.call_rapid_download('testrepo:pkg:1'), 0)

        for sub, rf in self.rapid.traverse():
            self.clear_dest_root()
            # we do replace as hacky way to support windows paths.
            path = os.path.join('/', sub,
                                rf.rapid_filename()).replace('\\', '/')
            self.server.add_resolver(
                fail_requests_resolver({path: HTTPStatus.NOT_FOUND}))
            with self.server.serve():
                self.assertNotEqual(
                    self.call_rapid_download('testrepo:pkg:1'),
                    0,
                    msg=f'downloading {path} didn\'t cause failure as expected')
            self.server.clear_resolvers()

    def test_sdp_not_complete_on_failure(self) -> None:
        repo = self.rapid.add_repo('testrepo')
        archive = repo.add_archive('pkg:1')
        archive.add_file('a.txt', b'a')
        b_file = archive.add_file('b.txt', b'aa')
        self.rapid.save(self.serving_root)

        path = os.path.join('/testrepo',
                            b_file.rapid_filename()).replace('\\', '/')
        self.server.add_resolver(
            fail_requests_resolver({path: HTTPStatus.NOT_FOUND}))
        with self.server.serve():
            self.assertNotEqual(self.call_rapid_download('testrepo:pkg:1'), 0)

        self.assertFalse(
            os.path.exists(
                os.path.join(self.dest_root, archive.rapid_filename())))
        # leaking a bit implementation detail but makes bug in test less likely
        self.assertTrue(
            os.path.exists(
                os.path.join(self.dest_root,
                             archive.rapid_filename() + '.incomplete')))

    def test_downloading_dependencies(self) -> None:
        repo = self.rapid.add_repo('base')
        archive = repo.add_archive('old_stable', 'Base 1.0')
        archive.add_file('a.txt', b'_')
        archive = repo.add_archive('stable', 'Base 2.0')
        archive.add_file('a.txt', b'a')
        repo = self.rapid.add_repo('repo1')
        archive = repo.add_archive('test', 'Repo1 test',
                                   ['rapid://base:stable'])
        archive.add_file('b.txt', b'a')
        repo = self.rapid.add_repo('repo2')
        archive = repo.add_archive('test', 'Repo2 test', ['Base 2.0'])
        archive.add_file('c.txt', b'a')
        repo = self.rapid.add_repo('top_repo')
        archive = repo.add_archive('pkg',
                                   'pkg',
                                   depends=['Repo1 test', 'rapid://repo2:test'])
        archive.add_file('d.txt', b'aeee')
        self.rapid.save(self.serving_root)

        with self.server.serve():
            self.assertEqual(self.call_rapid_download('rapid://top_repo:pkg'),
                             0)

        self.assertTrue(self.verify_downloaded_rapid('top_repo:pkg'))
        self.assertTrue(self.verify_downloaded_rapid('repo1:test'))
        self.assertTrue(self.verify_downloaded_rapid('repo2:test'))
        self.assertTrue(self.verify_downloaded_rapid('base:stable'))
        self.assertFalse(self.verify_downloaded_rapid('base:old_stable'))

    def test_handle_multiple_rapid_tags_resolved(self) -> None:
        repo = self.rapid.add_repo('base')
        archive1 = repo.add_archive('stable', 'Stable')
        archive1.add_file('a.txt', b'a')
        archive2 = repo.add_archive('git:198239',
                                    'Stable')  # exactly the same file
        archive2.add_file('a.txt', b'a')
        self.rapid.save(self.serving_root)

        self.assertEqual(archive1.rapid_filename(), archive2.rapid_filename())

        with self.server.serve():
            self.assertEqual(self.call_rapid_download('Stable'), 0)

        self.assertTrue(self.verify_downloaded_rapid('base:stable'))

    def test_multiple_packages_single_invocation(self) -> None:
        repo = self.rapid.add_repo('base')
        num_archives = 10
        for i in range(num_archives):
            archive = repo.add_archive(f'pkg:{i}')
            archive.add_file('f.txt', str(i).encode())
        self.rapid.save(self.serving_root)
        packages = [f'base:pkg:{i}' for i in range(num_archives)]

        with self.server.serve():
            self.assertEqual(self.call_rapid_download(packages), 0)

        for pkg in packages:
            self.assertTrue(self.verify_downloaded_rapid(pkg))

    def test_redownload_is_ok(self) -> None:
        repo = self.rapid.add_repo('repo')
        archive = repo.add_archive('pkg')
        archive.add_file('a.txt', b'a')
        self.rapid.save(self.serving_root)

        with self.server.serve():
            self.assertEqual(self.call_rapid_download('repo:pkg'), 0)
            self.assertTrue(self.verify_downloaded_rapid('repo:pkg'))
            self.assertEqual(self.call_rapid_download('repo:pkg'), 0)
            self.assertTrue(self.verify_downloaded_rapid('repo:pkg'))

    def test_sdp_download_all_pool_files_present(self) -> None:
        repo = self.rapid.add_repo('repo')
        archive = repo.add_archive('pkg:1')
        archive.add_file('a.txt', b'a')
        archive.add_file('b.txt', b'b')
        archive = repo.add_archive('pkg:2')
        archive.add_file('a.txt', b'a')
        self.rapid.save(self.serving_root)

        with self.server.serve():
            self.assertEqual(self.call_rapid_download('repo:pkg:1'), 0)
            self.assertTrue(self.verify_downloaded_rapid('repo:pkg:1'))
            self.assertEqual(self.call_rapid_download('repo:pkg:2'), 0)
            self.assertTrue(self.verify_downloaded_rapid('repo:pkg:2'))

    def test_io_error_fails_download(self) -> None:
        repo = self.rapid.add_repo('repo')
        archive = repo.add_archive('pkg')
        archive.add_file('a.txt', b'a')
        af = archive.add_file('b.txt', b'b')
        self.rapid.save(self.serving_root)

        # We simulate permission danied on writing to temporary file in pool
        path = os.path.join(self.dest_root, af.rapid_filename()) + '.tmp'
        os.makedirs(os.path.dirname(path))
        with open(path, 'wb') as f:
            f.write(b'asd')
        os.chmod(path, 0)

        with self.server.serve():
            self.assertNotEqual(self.call_rapid_download('repo:pkg'), 0)

    def _base_detect_file_corruption(self, use_streamer: bool) -> None:
        repo = self.rapid.add_repo('testrepo')
        archive = repo.add_archive('pkg:1')
        file = archive.add_file('a.txt', b'aakjsdifnsdfef')
        file.contents = b'ufeshfisuefe'
        self.rapid.save(self.serving_root)

        with self.server.serve():
            self.assertNotEqual(
                self.call_rapid_download('testrepo:pkg:1',
                                         use_streamer=use_streamer), 0)
            self.assertFalse(
                os.path.exists(
                    os.path.join(self.dest_root, file.rapid_filename())))

    def test_detect_file_corruption(self) -> None:
        self._base_detect_file_corruption(use_streamer=False)

    def test_detect_file_corruption_streamer(self) -> None:
        self._base_detect_file_corruption(use_streamer=True)

    def test_sdp_corruption_fails(self) -> None:
        repo = self.rapid.add_repo('testrepo')
        archive = repo.add_archive('pkg:1')
        file = archive.add_file('a.txt', b'a')
        self.rapid.save(self.serving_root)

        with open(
                os.path.join(self.serving_root, 'testrepo',
                             archive.rapid_filename()), 'wb') as f:
            f.write(b'asdiiada')

        with self.server.serve():
            self.assertNotEqual(self.call_rapid_download('testrepo:pkg:1'), 0)

    def test_retries_server_issues(self) -> None:
        repo = self.rapid.add_repo('testrepo')
        archive = repo.add_archive('pkg:1')
        archive.add_file('a.txt', b'a')
        failing_file = archive.add_file('b.txt', b'aa')
        self.rapid.save(self.serving_root)

        retries_left = 3

        def resolver(handler: HTTPHandler) -> tuple[bool, Optional[BinaryIO]]:
            nonlocal retries_left

            self.assertEqual(handler.headers.get("X-Prd-Retry-Num"), str(3 - retries_left))

            if handler.path.endswith(failing_file.rapid_filename().replace(
                    '\\', '/')):
                if retries_left > 0:
                    retries_left -= 1
                    handler.send_error(HTTPStatus.INTERNAL_SERVER_ERROR)
                    return True, None
            return False, None

        self.server.add_resolver(resolver)
        with self.server.serve():
            self.assertEqual(self.call_rapid_download('testrepo:pkg:1'), 0)

        self.assertEqual(retries_left, 0)

    def test_no_partial_overrides_to_files(self) -> None:
        repo = self.rapid.add_repo('rep1')
        archive = repo.add_archive('pkg:1')
        archive.add_file('a.txt', b'a')
        repo = self.rapid.add_repo('rep2')
        archive = repo.add_archive('pkg:2')
        archive.add_file('b.txt', b'b')
        self.rapid.save(self.serving_root)

        with self.server.serve():
            self.assertEqual(self.call_rapid_download('rep1:pkg:1'), 0)

        self.rapid.repos = {}
        self.rapid.__final_hash = None
        repo = self.rapid.add_repo('rep3')
        archive = repo.add_archive('pkg:1')
        archive.add_file('c.txt', b'c')
        self.rapid.save(self.serving_root)

        # This is hardcoding implementation specific path of pr-downloader
        # so it's not the best, but that's actual failure case <shrug>
        dest_repogz = os.path.join(
            self.dest_root,
            f'rapid/{self.server.server_address[0]}-{self.server.server_address[1]}/repos.gz'
        )

        # Make sure that pr-downloader will refresh repo
        os.utime(dest_repogz, (0, 0))

        with self.server.serve():
            self.call_rapid_download('rep2:pkg:1')

        with open(dest_repogz, 'rb') as f:
            h = hashlib.blake2b(f.read()).digest()

        self.assertEqual(h, self.rapid.final_hash)

    def test_etags_used(self) -> None:
        repo = self.rapid.add_repo('repo')
        archive = repo.add_archive('pkg')
        archive.add_file('a.txt', b'a')
        af = archive.add_file('b.txt', b'b')
        self.rapid.save(self.serving_root)

        with self.server.serve():
            self.assertEqual(self.call_rapid_download('repo:pkg'), 0)
            self.assertTrue(self.verify_downloaded_rapid('repo:pkg'))

        visited_file = False

        def resolver1(handler: HTTPHandler) -> tuple[bool, Optional[BinaryIO]]:
            nonlocal visited_file
            if handler.path.endswith(repo.rapid_filename().replace('\\', '/')):
                visited_file = True
                self.assertIsNotNone(handler.get_request_etag())
                self.assertEqual(handler.get_path_etag(),
                                 handler.get_request_etag())
            return False, None

        self.server.add_resolver(resolver1)
        with self.server.serve():
            self.assertEqual(self.call_rapid_download('repo:pkg'), 0)
            self.assertTrue(visited_file)
        self.server.clear_resolvers()

        # Change contents of version.gz on disk so that ETag is not send
        server = f'{self.server.server_address[0]}-{self.server.server_address[1]}'
        with open(
                os.path.join(self.dest_root,
                             f'rapid/{server}/repo/versions.gz'), 'wb') as f:
            f.write(b'asdiiada')

        visited_file = False

        def resolver2(handler: HTTPHandler) -> tuple[bool, Optional[BinaryIO]]:
            nonlocal visited_file
            if handler.path.endswith(repo.rapid_filename().replace('\\', '/')):
                visited_file = True
                self.assertIsNone(handler.get_request_etag())
            return False, None

        self.server.add_resolver(resolver2)
        with self.server.serve():
            self.assertEqual(self.call_rapid_download('repo:pkg'), 0)
            self.assertTrue(visited_file)

    # TODO(marekr): Fix bugs that are being reproduced by the tests below.

    # def test_streamer_not_returning_all_files_fails(self) -> None:
    #     repo = self.rapid.add_repo('testrepo')
    #     archive = repo.add_archive('pkg:1')
    #     archive.add_file('a.txt', b'a')
    #     archive.add_file('b.txt', b'aa')
    #     self.rapid.save(self.serving_root)

    #     del archive.files['b.txt']

    #     with self.server.serve():
    #         self.assertNotEqual(
    #             self.call_rapid_download('testrepo:pkg:1'), 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--pr-downloader-path', required=True)
    parser.add_argument(
        '--coverage-profiles-path',
        help='Path to directory where to store LLVM code coverage profiles.')
    parser.add_argument('-k',
                        '--keep-temp-files',
                        action=argparse.BooleanOptionalAction,
                        default=False)
    args, remaining_argv = parser.parse_known_args()
    TestDownloading.pr_downloader_path = args.pr_downloader_path
    TestDownloading.keep_temp_files = args.keep_temp_files
    TestDownloading.coverage_profiles_path = args.coverage_profiles_path
    unittest.main(argv=sys.argv[:1] + remaining_argv)
