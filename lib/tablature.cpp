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

#include "tbt-parser.h"

#include "tbt-parser/rational.h"
#include "tbt-parser/tbt-parser-util.h"
#include "tbt-parser/tbt.h"

#include "common/assert.h"

#include <map>


#define TAG "tablature"


const std::array<std::string, 12> MIDI_NOTE_TO_NAME_STRING = {
  "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B",
};


// Soft: (9)
// Slide up: /9
// Harmonic: <9>
// Slide down: \9
// Bend up: 9^
// Bend: b9
// Hammer on: h9
// Pull off: p9
// Release: r9
// Slap: s9
// Tap: t9
// Whammy bar bend: w9
// Tremolo: {9}
// Vibrato: 9~

const std::map<char, std::string> EFFECT_BEFORE = {
    { '\0', "" },
    { '(', "(" },
    { '/', "/" },
    { '<', "<" },
    { '\\', "\\" },
    { '^', "" },
    { 'b', "b" },
    { 'h', "h" },
    { 'p', "p" },
    { 'r', "r" },
    { 's', "s" },
    { 't', "t" },
    { 'w', "w" },
    { '{', "{" },
    { '~', "" },
};

const std::map<char, std::string> EFFECT_AFTER = {
    { '\0', "" },
    { '(', ")" },
    { '/', "" },
    { '<', ">" },
    { '\\', "" },
    { '^', "^" },
    { 'b', "" },
    { 'h', "" },
    { 'p', "" },
    { 'r', "" },
    { 's', "" },
    { 't', "" },
    { 'w', "" },
    { '{', "}" },
    { '~', "~" },
};


std::string trackEffectChangesString(const std::map<tbt_track_effect, uint16_t> &trackEffectChanges) {

    ASSERT(!trackEffectChanges.empty());

    if (2 <= trackEffectChanges.size()) {
        return "+";
    }

    const auto &it = trackEffectChanges.begin();

    auto effect = it->first;

    switch (effect) {
    case TE_STROKE_DOWN:
        return "D";
    case TE_STROKE_UP:
        return "U";
    case TE_TEMPO:
        return "T";
    case TE_INSTRUMENT:
        return "I";
    case TE_VOLUME:
        return "V";
    case TE_PAN:
        return "P";
    case TE_CHORUS:
        return "C";
    case TE_REVERB:
        return "R";
    case TE_MODULATION:
        return "M";
    case TE_PITCH_BEND:
        return "B";
    default: {
        ABORT("invalid effect: %d", effect);
    }
    }
}

std::string trackEffectString(uint8_t trackEffect) {

    switch (trackEffect) {
    case '\0':
        return "";
    case 'I': // Instrument change
        return "I";
    case 'V': // Volume change
        return "V";
    // NOLINTNEXTLINE(bugprone-branch-clone)
    case 'T': // Tempo change
        return "T";
    case 't': // Tempo change + 250
        return "T";
    case 'D': // Stroke down
        return "D";
    case 'U': // Stroke up
        return "U";
    case 'C': // Chorus change
        return "C";
    case 'P': // Pan change
        return "P";
    case 'R': // Reverb change
        return "R";
    default: {
        ABORT("invalid trackEffect: %c (%d)", trackEffect, trackEffect);
    }
    }
}




//
// bars:
//
//       3      10     7
// [  I  ]  |   I  |   ]
//
// if (0x70 <= VERSION), then bar lines are processed BEFORE the note
// else, then bar lines are processed AFTER the note
//


template <uint8_t STRINGS_PER_TRACK>
uint8_t
spaceWidthOfVsqs(const std::array<uint8_t, STRINGS_PER_TRACK + STRINGS_PER_TRACK + 4u> &vsqs, uint8_t stringCount) {

    uint8_t spaceWidth = 1;

    for (uint8_t string = 0; string < stringCount; string++) {

        uint8_t noteWidth;

        auto on = vsqs[string];

        if (0x80 <= on) {

            auto note = (on - 0x80);

            noteWidth = width(note);

        } else {

            noteWidth = 1;
        }

        auto effect = static_cast<char>(vsqs[STRINGS_PER_TRACK + string]);

        const auto &effectBeforeIt = EFFECT_BEFORE.find(effect);

        auto &effectBefore = effectBeforeIt->second;

        const auto &effectAfterIt = EFFECT_AFTER.find(effect);

        auto &effectAfter = effectAfterIt->second;

        spaceWidth = std::max(spaceWidth, static_cast<uint8_t>(effectBefore.size() + noteWidth + effectAfter.size()));
    }

    return spaceWidth;
}


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, uint8_t STRINGS_PER_TRACK, typename tbt_file_t>
std::string
TtbtFileTablature(const tbt_file_t &t) {

    uint16_t barLinesSpaceCount;
    if constexpr (0x70 <= VERSION) {
        barLinesSpaceCount = t.body.barLinesSpaceCount;
    } else if constexpr (VERSION == 0x6f) {
        barLinesSpaceCount = t.header.spaceCount;
    } else {
        barLinesSpaceCount = 4000;
    }

    //
    // first compute widths
    //

    uint8_t tuningWidth = 1;

    //
    // if not present, then assume 1
    //
    // must check if present in barLinesMap first
    //
    std::map<uint16_t, uint8_t> barLineWidthMap;

    //
    // if not present, then assume 1
    //
    std::map<uint16_t, uint8_t> actualSpaceWidthMap;


    //
    // compute bar line widths
    //
    {
        //
        // make a copy to modify
        //
        auto barLinesMap = t.body.barLinesMap;


        //
        // setup last bar lines
        //
        if constexpr (0x70 <= VERSION) {

            //
            // setup last bar line
            //

            barLinesMap[barLinesSpaceCount] = { 0, 0 };

        } else {

            //
            // setup last bar line
            //

            if (barLinesMap.find(barLinesSpaceCount - 1) == barLinesMap.end()) {
                barLinesMap[barLinesSpaceCount - 1] = { 0b00000001 };
            }
        }


        bool savedClose = false;
        uint8_t savedRepeats = 0;


        for (uint16_t space = 0; space < barLinesSpaceCount;) {

            const auto &barLinesMapIt = barLinesMap.find(space);

            //
            // bar line
            //
            // both bar lines before notes and bar lines after notes are processed at the same time
            //
            if (barLinesMapIt != barLinesMap.end()) {

                if constexpr (0x70 <= VERSION) {

                    auto barLine = barLinesMapIt->second;

                    if (savedClose) {

                        auto w = width(savedRepeats);

                        if (w != 1) {
                            barLineWidthMap[space] = w;
                        }

                        savedClose = false;
                    }

                    if ((barLine[0] & CLOSEREPEAT_MASK_GE70) == CLOSEREPEAT_MASK_GE70) {

                        //
                        // save for next bar line
                        //

                        savedClose = true;
                        savedRepeats = barLine[1];
                    }

                } else {

                    auto barLine = barLinesMapIt->second;

                    auto change = static_cast<tbt_bar_line>(barLine[0] & 0b00001111);

                    switch (change) {
                    case CLOSE: {

                        auto repeats = static_cast<uint8_t>((barLine[0] & 0b11110000) >> 4);

                        auto w = width(repeats);

                        if (w != 1) {
                            barLineWidthMap[space] = w;
                        }

                        break;
                    }
                    case OPEN:
                    case SINGLE:
                    case DOUBLE: {
                        break;
                    }
                    default: {
                        ABORT("invalid change: %d", change);
                    }
                    }
                }

                barLinesMap.erase(barLinesMapIt);
            }

            space++;
        }

        //
        // last bar line
        //
        {
            const auto &barLinesMapIt = barLinesMap.find(barLinesSpaceCount);

            if (barLinesMapIt != barLinesMap.end()) {

                if constexpr (0x70 <= VERSION) {

                    auto barLine = barLinesMapIt->second;

                    if (savedClose) {

                        auto w = width(savedRepeats);

                        if (w != 1) {
                            barLineWidthMap[barLinesSpaceCount] = w;
                        }

                        savedClose = false;
                    }

                    if ((barLine[0] & CLOSEREPEAT_MASK_GE70) == CLOSEREPEAT_MASK_GE70) {

                        //
                        // save for next bar line
                        //

                        savedClose = true;
                        savedRepeats = barLine[1];
                    }

                } else {

                    auto barLine = barLinesMapIt->second;

                    auto change = static_cast<tbt_bar_line>(barLine[0] & 0b00001111);

                    switch (change) {
                    case CLOSE: {

                        auto repeats = static_cast<uint8_t>((barLine[0] & 0b11110000) >> 4);

                        auto w = width(repeats);

                        if (w != 1) {
                            barLineWidthMap[barLinesSpaceCount] = w;
                        }

                        break;
                    }
                    case OPEN:
                    case SINGLE:
                    case DOUBLE: {
                        break;
                    }
                    default: {
                        ABORT("invalid change: %d", change);
                    }
                    }
                }

                barLinesMap.erase(barLinesMapIt);
            }
        }

        ASSERT(barLinesMap.empty());

    } // compute bar line widths


    //
    // compute tuningWidth and spaces widths
    //
    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        const auto &trackMetadata = t.metadata.tracks[track];

        const auto &maps = t.body.mapsList[track];

        uint16_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            //
            // stored as 32-bit int, so must be cast
            //
            trackSpaceCount = static_cast<uint16_t>(t.metadata.tracks[track].spaceCount);
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

            int8_t note;
            if constexpr (0x6b <= VERSION) {
                note = OPEN_STRING_TO_MIDI_NOTE[string];
            } else {
                note = OPEN_STRING_TO_MIDI_NOTE_LE6A[string];
            }

            note += trackMetadata.tuning[string];

            if constexpr (0x6e <= VERSION) {
                note += (trackMetadata.transposeHalfSteps);
            }

            uint8_t noteWidth;

            if constexpr (0x6e <= VERSION) {

                if (trackMetadata.displayMIDINoteNumbers) {

                    noteWidth = width(note);

                } else {

                    const auto &noteStr = MIDI_NOTE_TO_NAME_STRING[static_cast<uint8_t>(euclidean_mod(note, 12))];

                    noteWidth = static_cast<uint8_t>(noteStr.size());
                }

            } else {

                const auto &noteStr = MIDI_NOTE_TO_NAME_STRING[static_cast<uint8_t>(euclidean_mod(note, 12))];

                noteWidth = static_cast<uint8_t>(noteStr.size());
            }

            tuningWidth = std::max(tuningWidth, noteWidth);
        }


        rational actualSpace = 0;

        rational flooredActualSpace = 0;

        uint16_t flooredActualSpaceI = 0;

        uint16_t prevFlooredActualSpaceI = 0;

        //
        // accumulate through an entire actual space
        //
        uint8_t spaceWidthAcc = 0;

        for (uint16_t space = 0; space < trackSpaceCount;) {

            //
            // compute spaceWidth
            //

            const auto &notesMapIt = maps.notesMap.find(space);

            if (notesMapIt != maps.notesMap.end()) {

                const auto &vsqs = notesMapIt->second;

                auto spaceWidth = spaceWidthOfVsqs<STRINGS_PER_TRACK>(vsqs, trackMetadata.stringCount);

                spaceWidthAcc += spaceWidth;

            } else {

                spaceWidthAcc += 1;
            }

            //
            // Compute actual space
            //
            if constexpr (HASALTERNATETIMEREGIONS) {

                const auto &alternateTimeRegionsIt = maps.alternateTimeRegionsMap.find(space);
                if (alternateTimeRegionsIt != maps.alternateTimeRegionsMap.end()) {

                    const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                    auto atr = rational{ alternateTimeRegion[0], alternateTimeRegion[1] };

                    space++;

                    actualSpace += atr;

                } else {

                    space++;

                    ++actualSpace;
                }

                flooredActualSpace = actualSpace.floor();

                prevFlooredActualSpaceI = flooredActualSpaceI;

                flooredActualSpaceI = flooredActualSpace.to_uint16();

            } else {

                space++;

                actualSpace = space;

                flooredActualSpace = space;

                prevFlooredActualSpaceI = flooredActualSpaceI;

                flooredActualSpaceI = space;
            }

            //
            // increase actualSpaceWidth if needed
            //
            if (flooredActualSpaceI != prevFlooredActualSpaceI) {

                ASSERT(spaceWidthAcc != 0);

                if (spaceWidthAcc != 1) {

                    const auto &it = actualSpaceWidthMap.find(prevFlooredActualSpaceI);

                    if (it != actualSpaceWidthMap.end()) {

                        auto &actualSpaceWidth = it->second;

                        actualSpaceWidth = std::max(actualSpaceWidth, spaceWidthAcc);

                    } else {

                        actualSpaceWidthMap[prevFlooredActualSpaceI] = spaceWidthAcc;
                    }
                }

                spaceWidthAcc = 0;
            }

        } // space

    } // compute tuningWidth and spaces widths


    //
    // compute totalWidth
    //

    size_t totalWidth;

    {
        totalWidth = 0;

        totalWidth += tuningWidth;

        //
        // make a copy to modify
        //
        auto barLinesMap = t.body.barLinesMap;

        //
        // setup last bar lines
        //
        if constexpr (0x70 <= VERSION) {

            //
            // setup last bar line
            //

            barLinesMap[barLinesSpaceCount] = { 0, 0 };

        } else {

            //
            // setup last bar line
            //

            if (barLinesMap.find(barLinesSpaceCount - 1) == barLinesMap.end()) {
                barLinesMap[barLinesSpaceCount - 1] = { 0b00000001 };
            }
        }


        if constexpr (0x70 <= VERSION) {

        } else {

            //
            // hard-coded first bar line
            //
            totalWidth += 1;
        }

        for (uint16_t space = 0; space < barLinesSpaceCount;) {

            const auto &barLinesMapIt = barLinesMap.find(space);

            if (barLinesMapIt != barLinesMap.end()) {

                const auto &barLineWidthMapIt = barLineWidthMap.find(space);

                if (barLineWidthMapIt != barLineWidthMap.end()) {

                    auto barLineWidth = barLineWidthMapIt->second;

                    totalWidth += barLineWidth;

                } else {

                    totalWidth += 1;
                }
            }

            const auto &actualSpaceWidthMapIt = actualSpaceWidthMap.find(space);

            if (actualSpaceWidthMapIt != actualSpaceWidthMap.end()) {

                auto spaceWidth = actualSpaceWidthMapIt->second;

                totalWidth += spaceWidth;

            } else {

                totalWidth += 1;
            }

            space++;
        }

        //
        // last bar line
        //
        {
            const auto &barLinesMapIt = barLinesMap.find(barLinesSpaceCount);

            if (barLinesMapIt != barLinesMap.end()) {

                const auto &barLineWidthMapIt = barLineWidthMap.find(barLinesSpaceCount);

                if (barLineWidthMapIt != barLineWidthMap.end()) {

                    auto barLineWidth = barLineWidthMapIt->second;

                    totalWidth += barLineWidth;

                } else {

                    totalWidth += 1;
                }
            }
        }
    }


    //
    // the thing to return at the end
    //
    std::string acc;

    auto info = tbtFileInfo(t);

    acc += info;
    acc += '\n';


    //
    // for each track:
    //   render:
    //     top line text
    //     repeats count
    //     for each string:
    //       render:
    //         tuning, bar lines and notes
    //     track effect changes
    //     bottom line text
    //
    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        const auto &trackMetadata = t.metadata.tracks[track];

        //
        // make a copy to modify
        //
        auto barLinesMap = t.body.barLinesMap;

        const auto &maps = t.body.mapsList[track];

        uint16_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            //
            // stored as 32-bit int, so must be cast
            //
            trackSpaceCount = static_cast<uint16_t>(t.metadata.tracks[track].spaceCount);
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        std::string topLineText;
        std::string repeatsCount;
        std::vector<std::string> notesAndBarLines{ trackMetadata.stringCount };
        std::string trackEffectChanges;
        std::string bottomLineText;

        std::string debugText;

        if (trackMetadata.topLineText) {
            topLineText.reserve(totalWidth);
        }

        repeatsCount.reserve(totalWidth);

        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {
            notesAndBarLines[string].reserve(totalWidth);
        }

        trackEffectChanges.reserve(totalWidth);

        if (trackMetadata.bottomLineText) {
            bottomLineText.reserve(totalWidth);
        }

        debugText.reserve(totalWidth);


        //
        // render tuning
        //
        {
            if (trackMetadata.topLineText) {
                topLineText.append(tuningWidth, ' ');
            }

            repeatsCount.append(tuningWidth, ' ');

            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                int8_t note;
                if constexpr (0x6b <= VERSION) {
                    note = OPEN_STRING_TO_MIDI_NOTE[string];
                } else {
                    note = OPEN_STRING_TO_MIDI_NOTE_LE6A[string];
                }

                note += trackMetadata.tuning[string];

                if constexpr (0x6e <= VERSION) {
                    note += (trackMetadata.transposeHalfSteps);
                }

                uint8_t noteWidth;

                if constexpr (0x6e <= VERSION) {

                    if (trackMetadata.displayMIDINoteNumbers) {

                        auto noteStr = std::to_string(note);

                        notesAndBarLines[string] += noteStr;

                        noteWidth = static_cast<uint8_t>(noteStr.size());

                    } else {

                        const auto &noteStr = MIDI_NOTE_TO_NAME_STRING[static_cast<uint8_t>(euclidean_mod(note, 12))];

                        notesAndBarLines[string] += noteStr;

                        noteWidth = static_cast<uint8_t>(noteStr.size());
                    }

                } else {

                    const auto &noteStr = MIDI_NOTE_TO_NAME_STRING[static_cast<uint8_t>(euclidean_mod(note, 12))];

                    notesAndBarLines[string] += noteStr;

                    noteWidth = static_cast<uint8_t>(noteStr.size());
                }

                notesAndBarLines[string].append(tuningWidth - noteWidth, ' ');
            }

            trackEffectChanges.append(tuningWidth, ' ');

            if (trackMetadata.bottomLineText) {
                bottomLineText.append(tuningWidth, ' ');
            }

            debugText += static_cast<char>(tuningWidth + '0');
            debugText.append(tuningWidth - 1, ' ');
        }


        //
        // setup last bar lines
        // render first bar line
        //
        if constexpr (0x70 <= VERSION) {

            //
            // setup last bar line
            //

            barLinesMap[barLinesSpaceCount] = { 0, 0 };

        } else {

            //
            // render first bar line
            //
            // just hard code this here
            //
            {
                //
                // top line text for bar line
                //
                if (trackMetadata.topLineText) {
                    topLineText += ' ';
                }

                //
                // repeats count for bar line
                //
                repeatsCount += ' ';

                //
                // bar line
                //
                for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {
                    notesAndBarLines[string] += '|';
                }

                //
                // track effect changes for bar line
                //
                trackEffectChanges += ' ';

                //
                // bottom line text for bar line
                //
                if (trackMetadata.bottomLineText) {
                    bottomLineText += ' ';
                }

                debugText += static_cast<char>(1 + '0');
            }

            //
            // setup last bar line
            //

            if (barLinesMap.find(barLinesSpaceCount - 1) == barLinesMap.end()) {
                barLinesMap[barLinesSpaceCount - 1] = { 0b00000001 };
            }
        }


        rational actualSpace = 0;

        rational flooredActualSpace = 0;

        uint16_t flooredActualSpaceI = 0;

        uint16_t prevFlooredActualSpaceI = 0;

        //
        // accumulate through an entire actual space
        //
        uint8_t topLineTextWidthAcc = 0;

        uint8_t repeatsCountWidthAcc = 0;

        std::array<uint8_t, STRINGS_PER_TRACK> notesAndBarLinesWidthAcc{};

        uint8_t trackEffectChangesWidthAcc = 0;

        uint8_t bottomLineTextWidthAcc = 0;

        uint8_t debugTextWidthAcc = 0;


        auto fillin = [&](uint8_t width) {

            if (trackMetadata.topLineText) {

                topLineText.append(width - topLineTextWidthAcc, ' ');

                topLineTextWidthAcc = 0;
            }

            {
                repeatsCount.append(width - repeatsCountWidthAcc, ' ');

                repeatsCountWidthAcc = 0;
            }

            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                notesAndBarLines[string].append(width - notesAndBarLinesWidthAcc[string], '-');

                notesAndBarLinesWidthAcc[string] = 0;
            }

            {
                trackEffectChanges.append(width - trackEffectChangesWidthAcc, ' ');

                trackEffectChangesWidthAcc = 0;
            }

            if (trackMetadata.bottomLineText) {

                bottomLineText.append(width - bottomLineTextWidthAcc, ' ');

                bottomLineTextWidthAcc = 0;
            }

            debugText.append(width - debugTextWidthAcc, ' ');

            debugTextWidthAcc = 0;
        };


        bool savedClose = false;
        uint8_t savedRepeats = 0;


        for (uint16_t space = 0; space < trackSpaceCount;) {

            const auto &barLinesMapIt = barLinesMap.find(flooredActualSpaceI);

            const auto &notesMapIt = maps.notesMap.find(space);

            const auto &actualSpaceWidthMapIt = actualSpaceWidthMap.find(flooredActualSpaceI);

            const auto &barLineWidthMapIt = barLineWidthMap.find(flooredActualSpaceI);

            uint8_t spaceWidth;

            if (actualSpaceWidthMapIt != actualSpaceWidthMap.end()) {

                spaceWidth = actualSpaceWidthMapIt->second;

            } else {

                spaceWidth = 1;
            }

            uint8_t barLineWidth;

            if (barLineWidthMapIt != barLineWidthMap.end()) {

                barLineWidth = barLineWidthMapIt->second;

            } else {

                barLineWidth = 1;
            }

            //
            // save this because barLinesMap may be modified later and invalidate barLinesMapIt
            //
            bool barLinesMapItIsEnd = (barLinesMapIt == barLinesMap.end());

            //
            // bar line (when processed BEFORE the space)
            //
            if (!barLinesMapItIsEnd) {

                if constexpr (0x70 <= VERSION) {

                    auto barLine = barLinesMapIt->second;

                    //
                    // top line text for bar line
                    //
                    // nothing to do here
                    //

                    if (savedClose) {

                        //
                        // repeats count for bar line
                        //
                        
                        auto repeatsStr = std::to_string(savedRepeats);

                        {
                            repeatsCount += repeatsStr;

                            repeatsCountWidthAcc += static_cast<uint8_t>(repeatsStr.size());
                        }

                        //
                        // bar line
                        //
                        if ((barLine[0] & OPENREPEAT_MASK_GE70) == OPENREPEAT_MASK_GE70) {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += 'I';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }

                        } else {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += ']';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }
                        }

                        debugText += static_cast<char>(repeatsStr.size() + '0');

                        debugTextWidthAcc += 1;
                        savedClose = false;

                    } else {

                        //
                        // repeats count for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bar line
                        //
                        if ((barLine[0] & OPENREPEAT_MASK_GE70) == OPENREPEAT_MASK_GE70) {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += '[';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }

                        } else {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += '|';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }
                        }

                        debugText += static_cast<char>(1 + '0');

                        debugTextWidthAcc += 1;
                    }

                    if ((barLine[0] & CLOSEREPEAT_MASK_GE70) == CLOSEREPEAT_MASK_GE70) {

                        //
                        // save for next bar line
                        //

                        savedClose = true;
                        savedRepeats = barLine[1];
                    }

                    //
                    // track effect changes for bar line
                    //
                    // nothing to do here
                    //

                    //
                    // bottom line text for bar line
                    //
                    // nothing to do here
                    //

                    barLinesMap.erase(barLinesMapIt);

                    fillin(barLineWidth);

                } else {

                } // bar line (when processed BEFORE the space)
            }

            //
            // note space
            //
            {
                //
                // top line text for note
                //
                if (trackMetadata.topLineText) {

                    if (notesMapIt != maps.notesMap.end()) {

                        const auto &topLineTextVsqs = notesMapIt->second;

                        auto text = topLineTextVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 1];

                        if (text == 0x00) {

                        } else {

                            topLineText += static_cast<char>(text);

                            topLineTextWidthAcc += 1;
                        }
                    }
                }

                //
                // repeats count for note
                //
                // nothing to do here
                //

                //
                // note
                //
                if (notesMapIt != maps.notesMap.end()) {

                    const auto &vsqs = notesMapIt->second;

                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                        auto effect = static_cast<char>(vsqs[STRINGS_PER_TRACK + string]);

                        //
                        // effect before the note
                        //
                        {
                            const auto &effectBeforeIt = EFFECT_BEFORE.find(effect);

                            const auto &effectBeforeStr = effectBeforeIt->second;

                            notesAndBarLines[string] += effectBeforeStr;

                            notesAndBarLinesWidthAcc[string] += static_cast<uint8_t>(effectBeforeStr.size());
                        }

                        //
                        // note
                        //

                        auto on = vsqs[string];

                        if (on == 0) {

                            notesAndBarLines[string] += '-';

                            notesAndBarLinesWidthAcc[string] += 1;

                        } else if (0x80 <= on) {

                            auto note = (on - 0x80);

                            auto noteStr = std::to_string(note);

                            notesAndBarLines[string] += noteStr;

                            notesAndBarLinesWidthAcc[string] += static_cast<uint8_t>(noteStr.size());

                        } else if (on == MUTED) {

                            notesAndBarLines[string] += 'x';

                            notesAndBarLinesWidthAcc[string] += 1;

                        } else {

                            ASSERT(on == STOPPED);

                            notesAndBarLines[string] += '*';

                            notesAndBarLinesWidthAcc[string] += 1;
                        }

                        //
                        // effect after the note
                        //
                        {
                            const auto &effectAfterIt = EFFECT_AFTER.find(effect);

                            const auto &effectAfterStr = effectAfterIt->second;

                            notesAndBarLines[string] += effectAfterStr;

                            notesAndBarLinesWidthAcc[string] += static_cast<uint8_t>(effectAfterStr.size());
                        }
                    }
                }


                //
                // track effect changes for note
                //
                {
                    if constexpr (VERSION == 0x72) {

                        const auto &trackEffectChangesIt = maps.trackEffectChangesMap.find(space);

                        if (trackEffectChangesIt != maps.trackEffectChangesMap.end()) {

                            const auto &changes = trackEffectChangesIt->second;

                            const auto &trackEffectChangesStr = trackEffectChangesString(changes);

                            trackEffectChanges += trackEffectChangesStr;

                            trackEffectChangesWidthAcc += static_cast<uint8_t>(trackEffectChangesStr.size());
                        }

                    } else {

                        if (notesMapIt != maps.notesMap.end()) {

                            const auto &effectVsqs = notesMapIt->second;

                            auto trackEffect = effectVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 0];

                            const auto &trackEffectChangesStr = trackEffectString(trackEffect);

                            trackEffectChanges += trackEffectChangesStr;

                            trackEffectChangesWidthAcc += static_cast<uint8_t>(trackEffectChangesStr.size());
                        }
                    }
                }

                //
                // bottom line text for note
                //
                if (trackMetadata.bottomLineText) {

                    if (notesMapIt != maps.notesMap.end()) {

                        const auto &bottomLineTextVsqs = notesMapIt->second;

                        auto text = bottomLineTextVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 2];

                        if (text == 0x00) {

                        } else {

                            bottomLineText += static_cast<char>(text);

                            bottomLineTextWidthAcc += 1;
                        }
                    }
                }

                debugText += static_cast<char>(spaceWidth + '0');

                debugTextWidthAcc += 1;

            } // note

            //
            // Compute actual space
            //
            if constexpr (HASALTERNATETIMEREGIONS) {

                const auto &alternateTimeRegionsIt = maps.alternateTimeRegionsMap.find(space);
                if (alternateTimeRegionsIt != maps.alternateTimeRegionsMap.end()) {

                    const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                    auto atr = rational{ alternateTimeRegion[0], alternateTimeRegion[1] };

                    space++;

                    actualSpace += atr;

                } else {

                    space++;

                    ++actualSpace;
                }

                flooredActualSpace = actualSpace.floor();

                prevFlooredActualSpaceI = flooredActualSpaceI;

                flooredActualSpaceI = flooredActualSpace.to_uint16();

            } else {

                space++;

                actualSpace = space;

                flooredActualSpace = space;

                prevFlooredActualSpaceI = flooredActualSpaceI;

                flooredActualSpaceI = space;
            }

            //
            // if crossing a space, then make sure to fill remaining characters in notesAndBarLines
            //
            if (flooredActualSpaceI != prevFlooredActualSpaceI) {
                fillin(spaceWidth);
            }

            //
            // bar line (when processed AFTER the space)
            //
            if (!barLinesMapItIsEnd) {

                if constexpr (0x70 <= VERSION) {

                } else {

                    auto barLine = barLinesMapIt->second;

                    auto change = static_cast<tbt_bar_line>(barLine[0] & 0b00001111);

                    switch (change) {
                    case CLOSE: {

                        //
                        // top line text for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // repeats count for bar line
                        //

                        auto repeats = static_cast<uint8_t>((barLine[0] & 0b11110000) >> 4);

                        auto repeatsStr = std::to_string(repeats);

                        {
                            repeatsCount += repeatsStr;

                            repeatsCountWidthAcc += static_cast<uint8_t>(repeatsStr.size());
                        }

                        //
                        // bar line
                        //
                        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                            notesAndBarLines[string] += ']';

                            notesAndBarLinesWidthAcc[string] += 1;
                        }

                        //
                        // track effect changes for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bottom line text for bar line
                        //
                        // nothing to do here
                        //

                        debugText += static_cast<char>(repeatsStr.size() + '0');

                        debugTextWidthAcc += 1;

                        break;
                    }
                    case OPEN:
                        //
                        // already handled
                        //
                        break;
                    case SINGLE: {

                        const auto &barLinesMapItNext = barLinesMap.find(flooredActualSpaceI + 1);

                        do {

                            if (barLinesMapItNext != barLinesMap.end()) {

                                auto barLineNext = barLinesMapItNext->second;

                                auto changeNext = static_cast<tbt_bar_line>(barLineNext[0] & 0b00001111);

                                if (changeNext == OPEN) {

                                    //
                                    // top line text for bar line
                                    //
                                    // nothing to do here
                                    //

                                    //
                                    // repeats count for bar line
                                    //
                                    // nothing to do here
                                    //

                                    //
                                    // bar line
                                    //
                                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                        notesAndBarLines[string] += '[';

                                        notesAndBarLinesWidthAcc[string] += 1;
                                    }

                                    //
                                    // track effect changes for bar line
                                    //
                                    // nothing to do here
                                    //

                                    //
                                    // bottom line text for bar line
                                    //
                                    // nothing to do here
                                    //

                                    debugText += static_cast<char>(1 + '0');

                                    debugTextWidthAcc += 1;

                                    break;
                                }
                            }

                            //
                            // top line text for bar line
                            //
                            // nothing to do here
                            //

                            //
                            // repeats count for bar line
                            //
                            // nothing to do here
                            //

                            //
                            // bar line
                            //
                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += '|';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }

                            //
                            // track effect changes for bar line
                            //
                            // nothing to do here
                            //

                            //
                            // bottom line text for bar line
                            //
                            // nothing to do here
                            //

                            debugText += static_cast<char>(1 + '0');

                            debugTextWidthAcc += 1;

                        } while (false);

                        break;
                    }
                    case DOUBLE: {

                        const auto &barLinesMapItNext = barLinesMap.find(flooredActualSpaceI + 1);

                        do {

                            if (barLinesMapItNext != barLinesMap.end()) {

                                auto barLineNext = barLinesMapItNext->second;

                                auto changeNext = static_cast<tbt_bar_line>(barLineNext[0] & 0b00001111);

                                if (changeNext == OPEN) {

                                    //
                                    // top line text for bar line
                                    //
                                    // nothing to do here
                                    //

                                    //
                                    // repeats count for bar line
                                    //
                                    // nothing to do here
                                    //

                                    //
                                    // bar line
                                    //
                                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                        notesAndBarLines[string] += '[';

                                        notesAndBarLinesWidthAcc[string] += 1;
                                    }

                                    //
                                    // track effect changes for bar line
                                    //
                                    // nothing to do here
                                    //

                                    //
                                    // bottom line text for bar line
                                    //
                                    // nothing to do here
                                    //

                                    debugText += static_cast<char>(1 + '0');

                                    debugTextWidthAcc += 1;

                                    break;
                                }
                            }

                            //
                            // top line text for bar line
                            //
                            // nothing to do here
                            //

                            //
                            // repeats count for bar line
                            //
                            // nothing to do here
                            //

                            //
                            // bar line
                            //
                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += 'H';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }

                            //
                            // track effect changes for bar line
                            //
                            // nothing to do here
                            //

                            //
                            // bottom line text for bar line
                            //
                            // nothing to do here
                            //

                            debugText += static_cast<char>(1 + '0');

                            debugTextWidthAcc += 1;

                        } while (false);

                        break;
                    }
                    default: {
                        ABORT("invalid change: %d", change);
                    }
                    }

                    barLinesMap.erase(barLinesMapIt);

                    fillin(barLineWidth);
                }

            } // bar line (when processed AFTER the space)

        } // for (space)

        //
        // last bar line
        //
        {
            const auto &barLinesMapIt = barLinesMap.find(barLinesSpaceCount);

            const auto &barLineWidthMapIt = barLineWidthMap.find(barLinesSpaceCount);

            uint8_t barLineWidth;

            if (barLineWidthMapIt != barLineWidthMap.end()) {

                barLineWidth = barLineWidthMapIt->second;

            } else {

                barLineWidth = 1;
            }

            if (barLinesMapIt != barLinesMap.end()) {

                //
                // bar line (when processed BEFORE the space)
                //
                if constexpr (0x70 <= VERSION) {

                    auto barLine = barLinesMapIt->second;

                    //
                    // top line text for bar line
                    //
                    // nothing to do here
                    //

                    if (savedClose) {

                        //
                        // repeats count for bar line
                        //

                        auto repeatsStr = std::to_string(savedRepeats);

                        {
                            repeatsCount += repeatsStr;

                            repeatsCountWidthAcc += static_cast<uint8_t>(repeatsStr.size());
                        }

                        //
                        // bar line
                        //
                        if ((barLine[0] & OPENREPEAT_MASK_GE70) == OPENREPEAT_MASK_GE70) {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += 'I';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }

                        } else {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += ']';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }
                        }

                        debugText += static_cast<char>(repeatsStr.size() + '0');

                        debugTextWidthAcc += 1;

                        savedClose = false;

                    } else {

                        //
                        // repeats count for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bar line
                        //
                        if ((barLine[0] & OPENREPEAT_MASK_GE70) == OPENREPEAT_MASK_GE70) {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += '[';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }

                        } else {

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                notesAndBarLines[string] += '|';

                                notesAndBarLinesWidthAcc[string] += 1;
                            }
                        }

                        debugText += static_cast<char>(1 + '0');

                        debugTextWidthAcc += 1;
                    }

                    if ((barLine[0] & CLOSEREPEAT_MASK_GE70) == CLOSEREPEAT_MASK_GE70) {

                        //
                        // save for next bar line
                        //

                        savedClose = true;
                        savedRepeats = barLine[1];
                    }

                    //
                    // track effect changes for bar line
                    //
                    // nothing to do here
                    //

                    //
                    // bottom line text for bar line
                    //
                    // nothing to do here
                    //

                } else {

                    auto barLine = barLinesMapIt->second;

                    auto change = static_cast<tbt_bar_line>(barLine[0] & 0b00001111);

                    switch (change) {
                    case CLOSE: {

                        //
                        // top line text for bar line
                        //
                        // nothing to do here

                        //
                        // repeats count for bar line
                        //

                        auto repeats = static_cast<uint8_t>((barLine[0] & 0b11110000) >> 4);

                        auto repeatsStr = std::to_string(repeats);

                        {
                            repeatsCount += repeatsStr;

                            repeatsCountWidthAcc += static_cast<uint8_t>(repeatsStr.size());
                        }

                        //
                        // bar line
                        //
                        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                            notesAndBarLines[string] += ']';

                            notesAndBarLinesWidthAcc[string] += 1;
                        }

                        //
                        // track effect changes for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bottom line text for bar line
                        //
                        // nothing to do here
                        //

                        debugText += static_cast<char>(repeatsStr.size() + '0');

                        debugTextWidthAcc += 1;

                        break;
                    }
                    case OPEN: {

                        //
                        // already handled
                        //

                        break;
                    }
                    case SINGLE: {

                        //
                        // top line text for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // repeats count for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bar line
                        //
                        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                            notesAndBarLines[string] += '|';

                            notesAndBarLinesWidthAcc[string] += 1;
                        }

                        //
                        // track effect changes for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bottom line text for bar line
                        //
                        // nothing to do here
                        //

                        debugText += static_cast<char>(1 + '0');

                        debugTextWidthAcc += 1;

                        break;
                    }
                    case DOUBLE: {
                        
                        //
                        // top line text for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // repeats count for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bar line
                        //
                        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                            notesAndBarLines[string] += 'H';

                            notesAndBarLinesWidthAcc[string] += 1;
                        }

                        //
                        // track effect changes for bar line
                        //
                        // nothing to do here
                        //

                        //
                        // bottom line text for bar line
                        //
                        // nothing to do here
                        //

                        debugText += static_cast<char>(1 + '0');

                        debugTextWidthAcc += 1;

                        break;
                    }
                    default: {
                        ABORT("invalid change: %d", change);
                    }
                    }
                }

                barLinesMap.erase(barLinesMapIt);

                fillin(barLineWidth);
            }

        } // last bar line


        ASSERT(barLinesMap.empty());


        if (trackMetadata.topLineText) {
            ASSERT(topLineTextWidthAcc == 0);
        }

        ASSERT(repeatsCountWidthAcc == 0);

        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {
            ASSERT(notesAndBarLinesWidthAcc[string] == 0);
        }

        ASSERT(trackEffectChangesWidthAcc == 0);

        if (trackMetadata.bottomLineText) {
            ASSERT(bottomLineTextWidthAcc == 0);
        }

        ASSERT(debugTextWidthAcc == 0);


        if (trackMetadata.topLineText) {
            ASSERT(topLineText.size() == totalWidth);
        }

        ASSERT(repeatsCount.size() == totalWidth);

        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {
            ASSERT(notesAndBarLines[string].size() == totalWidth);
        }

        ASSERT(trackEffectChanges.size() == totalWidth);

        if (trackMetadata.bottomLineText) {
            ASSERT(bottomLineText.size() == totalWidth);
        }

        ASSERT(debugText.size() == totalWidth);

        acc += std::string("track ") + std::to_string(track + 1) + ":\n";

        {
            size_t toReserve = 0;

            if (trackMetadata.topLineText) {
                toReserve += (totalWidth + 1);
            }

            toReserve += (totalWidth + 1);

            toReserve += (totalWidth + 1) * trackMetadata.stringCount;

            toReserve += (totalWidth + 1);

            if (trackMetadata.bottomLineText) {
                toReserve += (totalWidth + 1);
            }

            toReserve += (totalWidth + 1);

            toReserve += 1;

            acc.reserve(acc.size() + toReserve);
        }

        if (trackMetadata.topLineText) {
            acc += topLineText;
            acc += '\n';
        }

        acc += repeatsCount;
        acc += '\n';

        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {
            acc += notesAndBarLines[trackMetadata.stringCount - string - 1u];
            acc += '\n';
        }

        acc += trackEffectChanges;
        acc += '\n';

        if (trackMetadata.bottomLineText) {
            acc += bottomLineText;
            acc += '\n';
        }

        acc += debugText;
        acc += '\n';

        acc += '\n';

    } // render bar lines and spaces for each track

    return acc;
}

std::string tbtFileTablature(const tbt_file &t) {

    auto versionNumber = tbtFileVersionNumber(t);

    switch (versionNumber) {
    case 0x72: {

        auto t71 = std::get<tbt_file71>(t);

        if ((t71.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TtbtFileTablature<0x72, true, 8>(t71);
        } else {
            return TtbtFileTablature<0x72, false, 8>(t71);
        }
    }
    case 0x71: {

        auto t71 = std::get<tbt_file71>(t);

        if ((t71.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TtbtFileTablature<0x71, true, 8>(t71);
        } else {
            return TtbtFileTablature<0x71, false, 8>(t71);
        }
    }
    case 0x70: {

        auto t70 = std::get<tbt_file70>(t);

        if ((t70.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TtbtFileTablature<0x70, true, 8>(t70);
        } else {
            return TtbtFileTablature<0x70, false, 8>(t70);
        }
    }
    case 0x6f: {

        auto t6f = std::get<tbt_file6f>(t);

        return TtbtFileTablature<0x6f, false, 8>(t6f);
    }
    case 0x6e: {

        auto t6e = std::get<tbt_file6e>(t);

        return TtbtFileTablature<0x6e, false, 8>(t6e);
    }
    case 0x6b: {

        auto t6b = std::get<tbt_file6b>(t);

        return TtbtFileTablature<0x6b, false, 8>(t6b);
    }
    case 0x6a: {

        auto t6a = std::get<tbt_file6a>(t);

        return TtbtFileTablature<0x6a, false, 6>(t6a);
    }
    case 0x69: {

        auto t68 = std::get<tbt_file68>(t);

        return TtbtFileTablature<0x69, false, 6>(t68);
    }
    case 0x68: {

        auto t68 = std::get<tbt_file68>(t);

        return TtbtFileTablature<0x68, false, 6>(t68);
    }
    case 0x67: {

        auto t65 = std::get<tbt_file65>(t);

        return TtbtFileTablature<0x67, false, 6>(t65);
    }
    case 0x66: {

        auto t65 = std::get<tbt_file65>(t);

        return TtbtFileTablature<0x66, false, 6>(t65);
    }
    case 0x65: {

        auto t65 = std::get<tbt_file65>(t);

        return TtbtFileTablature<0x65, false, 6>(t65);
    }
    default: {
        ABORT("invalid versionNumber: 0x%02x", versionNumber);
    }
    }
}














