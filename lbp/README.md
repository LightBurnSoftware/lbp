# LBP Library

## Simple Build Example
```
cmake -B build
cmake --build build -j
./build/TestLBP
```

## Install Example

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
cmake --install build --prefix /tmp/lbp-install
```
