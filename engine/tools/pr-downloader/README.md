# pr-downloader

pr-downloader is tool to download maps and games for the Spring RTS engine.

This repository is a Beyond All Reason (BAR) fork of the upstream
[spring/pr-downloader] with additional improvements and features that are
compatible with the BAR's fork of the Spring RTS engine.

The official compiled distribution of pr-downloader happens currently via the
engine releases in the [beyond-all-reason/spring] repository.

## Usage

The basic usage to download a game distributed via [rapid] and a map looks like:

```
$ pr-downloader dev-game:test "Angel Crossing 1.4"
```

Run program with `--help` to see all options.

## Development

### Compile

You can ehter follow compilation instruction in the [beyond-all-reason/spring]
or compile it separately using steps below.

On top of dependencies that are vendored in this repository, you will also neeed
to provide: curl, zlib, and Boost.Test for unit tests. On Debian based
distribution the packages are: `libcurl4-openssl-dev`, `libboost-test-dev`,
`zlib1g-dev`.

The project uses CMake so it's a standard set of steps to compile binary:

```
$ mkdir builddir
$ cd builddir
$ cmake ..
$ cmake --build .
```

### Testing

The basic C++ unit tests can be run using `ctest` command. There are also
written in Python functional tests that are testing binary behavior end-to-end.
To run them:

```
./test/functional_test.py --pr-downloader-path builddir/src/pr-downloader
```

[test/download](test/download) contains some scripts for setting up performance
tests.

## License

The program is licensed under license GPL version 2 or later, see
[LICENSE](LICENSE) file for full terms.

This repository also distributes vendored source code of other projects in
[src/lib](src/lib) directory that are distributed under different licenses.


[spring/pr-downloader]: https://github.com/spring/pr-downloader
[beyond-all-reason/spring]: https://github.com/beyond-all-reason/spring
[rapid]: https://springrts.com/wiki/Rapid
