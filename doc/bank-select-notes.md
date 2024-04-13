


http://www.somascape.org/midi/tech/spec.html#banksel




For FluidSynth,

> Interpretation of MSB and LSB depends on synth.midi-bank-select

https://github.com/FluidSynth/fluidsynth/wiki/FluidFeatures



synth.midi-bank-select
Type:
Selection (str)
Options:
gs, gm, xg, mma
Default:
gs
This setting defines how the synthesizer interprets Bank Select messages.

    gs: (default) CC0 becomes the bank number, CC32 is ignored.
    gm: ignores CC0 and CC32 messages.
    xg: if CC0<120 then channel is set to melodic and CC32 is the bank number. If CC0>=120 then channel is set to drum and the bank number is set to 128 (CC32 is ignored).
    mma: bank is calculated as CC0*128+CC32.




