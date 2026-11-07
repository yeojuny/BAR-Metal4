# Upstream BARbarIAn build input

The engine tree contains BARb at upstream commit f4a6ca3ae096631894e194cfa9b4e8e53f4fb78c.
The standalone CMake entry point references the extracted engine with a relative-source
parameter and uses the archived generated wrappers. For example, from the extracted
source directory:

```sh
ln -s ../engine/AI/Skirmish/BARb barb-build/BARb
ln -s ../engine/rts barb-build/rts
cmake -S barb-build -B build-barb -G Ninja -DRECOIL_SOURCE="$PWD/engine" -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-barb --target BARb
```

The portable path translation has not been independently rebuilt. The release module
was compiled on Apple Silicon against the 27.2 SDK from the pinned original BARb files.
