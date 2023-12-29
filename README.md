
# tbt-parser
A C++ parser and MIDI converter for .tbt TabIt files

The .tbt TabIt file format is described here:

* [The .tbt TabIt file format](https://github.com/bostick/tabit-file-format)


All known versions of TabIt are handled.


## How to build

tbt-parser depends on zlib and uses CMake for building.

tbt-parser requires a C++17 compiler because of features such as `if constexpr`.

```
cd cpp
mkdir build
cd build
cmake -DZLIB_ROOT=/opt/homebrew/Cellar/zlib/1.3/ ..
cmake --build .
```


## How to use

Generate a MIDI file from a .tbt TabIt file:
```
% exe/tbt-converter --input-file twinkle.tbt 
tbt converter v1.0.0
Copyright (C) 2023 by Brenton Bostick
input file: twinkle.tbt
output file: out.mid
parsing...
tbt file version: 0x6f
title: 
artist: 
album: 
transcribed by: 
exporting...
finished!
% 
```



