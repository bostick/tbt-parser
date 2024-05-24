// Copyright (C) 2024 by Brenton Bostick
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial
// portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

//
// mask for featureBitfield
//
const uint8_t HASALTERNATETIMEREGIONS_MASK = 0b00010000;

const uint8_t MUTED = 0x11; // 17
const uint8_t STOPPED = 0x12; // 18

//
// masks for barLinesMap (70 <= VERSION)
//
const uint8_t DOUBLEBAR_MASK_GE70 =   0b00000001;
const uint8_t OPENREPEAT_MASK_GE70 =  0b00000010;
const uint8_t CLOSEREPEAT_MASK_GE70 = 0b00000100;

enum tbt_bar_line : uint8_t {
    SINGLE = 1,
    CLOSE = 2,
    OPEN = 3,
    DOUBLE = 4,
};


const std::array<int8_t, 8> OPEN_STRING_TO_MIDI_NOTE = {
    0x28, // MIDI note for open E string
    0x2d, // MIDI note for open A string
    0x32, // MIDI note for open D string
    0x37, // MIDI note for open G string
    0x3b, // MIDI note for open B string
    0x40, // MIDI note for open e string
    0x00,
    0x00,
};

const std::array<int8_t, 6> OPEN_STRING_TO_MIDI_NOTE_LE6A = {
    0x40, // MIDI note for open e string
    0x3b, // MIDI note for open B string
    0x37, // MIDI note for open G string
    0x32, // MIDI note for open D string
    0x2d, // MIDI note for open A string
    0x28, // MIDI note for open E string
};











