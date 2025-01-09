
# tbt-parser
A C++ parser and MIDI converter for .tbt TabIt files

The .tbt TabIt file format is described here:

* [The .tbt TabIt file format](https://github.com/bostick/tabit-file-format)


All known versions of TabIt are handled.


## How to build

tbt-parser depends on zlib and uses CMake for building.

tbt-parser requires a C++20 compiler because of features such as `__VA_OPT__`.

```
cd tbt-parser
mkdir build
cd build
cmake ..
cmake --build .
```

If building tests with Xcode, must also do:
```
-DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST
```

https://gitlab.kitware.com/cmake/cmake/-/issues/25730


## How to use

Print out tablature from a .tbt TabIt file:
```
% ./tbt-printer --input-file black.tbt                                                                     
tbt printer v1.3.0
Copyright (C) 2024 by Brenton Bostick
input file: black.tbt
output file: out.txt
parsing...
printing...
finished!
% 
```

Generate a MIDI file from a .tbt TabIt file:
```
% ./tbt-converter --input-file black.tbt 
tbt converter v1.3.0
Copyright (C) 2024 by Brenton Bostick
input file: black.tbt
output file: out.mid
emit control change events: 1
emit program change events: 1
emit pitch bend events: 1
exporting...
finished!
% 
```

Generate a MIDI file from a .tbt TabIt file, and do not emit any ControlChange events, ProgramChange events, or PitchBend events:
```
% ./tbt-converter --input-file black.tbt --emit-controlchange-events 0 --emit-programchange-events 0 --emit-pitchbend-events 0
tbt converter v1.3.0
Copyright (C) 2024 by Brenton Bostick
input file: black.tbt
output file: out.mid
emit control change events: 0
emit program change events: 0
emit pitch bend events: 0
exporting...
finished!
% 
```

Print out information about a MIDI file:
```
% ./midi-info --input-file black.mid 
midi info v1.3.0
Copyright (C) 2024 by Brenton Bostick
input file: black.mid
parsing...
tracks:
Track Count: 6
Track Name: TabIt MIDI - Track 1
Track Name: TabIt MIDI - Track 2
Track Name: TabIt MIDI - Track 3
Track Name: TabIt MIDI - Track 4
Track Name: TabIt MIDI - Track 5
times:                  h:mm:sssss
   last Note On (wall): 0:04:17.18
  last Note Off (wall): 0:04:17.51
   End Of Track (wall): 0:04:17.51
 last Note On (micros): 257176681.50000000000000000
last Note Off (micros): 257513760.00000000000000000
 End Of Track (micros): 257513760.00000000000000000
  last Note On (ticks): 73632
 last Note Off (ticks): 73728
  End Of Track (ticks): 73728
finished!
% 
```

Print out information about a .tbt TabIt file:
```
% ./tbt-info --input-file black.tbt
tbt info v1.3.0
Copyright (C) 2024 by Brenton Bostick
input file: black.tbt
tbt file version: 2.0 (0x72)
title: Back In Black
artist: AC/DC
album: Back In Black
transcribed by: Wraith (wraith_anleu@yahoo.com)

let me know if I can fix anything

edit  10-13-05
-more small things changes
edit  2-4-05 
-fixed some small things
% 
```





