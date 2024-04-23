# tbt-parser internal docs


When processing MIDI, there are some common variable names and patterns in the code.


## space

"space" is the space on screen for each track

Tracks may have different numbers of spaces.

It is an integer.

The spaces in an Alternate Time Region are the same as regular spaces.

Updating "space" is not monotonic and may skip around because of repeats.


## actualSpace

"actualSpace" is rational and takes alternate time regions into account.

So an "actualSpace" may be something like 100/3

Updating "actualSpace" is not monotonic and may skip around because of repeats.



## tick

"tick" is MIDI ticks

Updating "tick" is monotonic because it measures the span of time.


## Alternate Time Regions

Each space in an Alternate Time Region has a "denominator" and "numerator".

For example, for triplets, denominator is 2.
For example, for triplets, numerator is 3.



