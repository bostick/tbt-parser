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


#define TAG "expandDeltaList"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#a-note-on-iterating-through-deltalists
//


template <uint32_t S>
Status
expandDeltaList(
    const std::vector<uint8_t> &deltaList,
    uint32_t unitCount,
    uint8_t x,
    std::map<uint16_t, std::array<uint8_t, S> > &map) {

    std::vector<std::array<uint8_t, 2> > parts;

    Status ret = partitionInto<2>(deltaList, parts);

    if (ret != OK) {
        return ret;
    }

    std::vector<std::vector<std::array<uint8_t, 2> > > split;

    //
    // split into lists where first is {0, ...}
    //
    splitAt(parts.cbegin(), parts.cend(), split,
        [](std::array<uint8_t, 2> x) {
            return x[0] == 0;
        }
    );

    int32_t unit = 0;

    auto dv = std::div(unit, S);

    auto space = static_cast<uint16_t>(dv.quot);
    auto slot = static_cast<uint16_t>(dv.rem);

    int32_t newUnit;

    uint16_t newSpace;
    uint16_t newSlot;

    std::array<uint8_t, S> units{};
    bool hasNonDefaultValue = false;

    for (const auto &s: split) {

        uint16_t n;
        uint8_t y;

        if (s[0][0] == 0) {

            CHECK(s.size() == 2, "unhandled");

            //
            // parse s[0][1] and s[1][0] as a single short
            //
            n = parseLE2(s[0][1], s[1][0]);

            y = s[1][1];

        } else {

            ASSERT(s.size() == 1);

            n = s[0][0];

            y = s[0][1];
        }

        newUnit = unit + n;

        dv = std::div(static_cast<int>(newUnit), S);

        newSpace = static_cast<uint16_t>(dv.quot);
        newSlot = static_cast<uint16_t>(dv.rem);

        if (y == x) {

            //
            // using default value
            //

            if (space == newSpace) {

                CHECK(slot < newSlot, "unhandled");

                //
                // continue filling current space;
                //
                // fill-in from slot to newSlot with value y;
                //
                std::memset(units.data() + slot, y, (newSlot - slot));

            } else {

                ASSERT(space < newSpace);

                //
                // finish filling current space
                //
                std::memset(units.data() + slot, y, (S - slot));

                if (hasNonDefaultValue) {

                    //
                    // only insert into map if has non-default value
                    //

                    map[space] = units;

                    hasNonDefaultValue = false;
                }

                if (0 < newSlot) {

                    //
                    // start filling new space
                    //
                    std::memset(units.data(), y, (newSlot - 0));
                }
            }

        } else {

            //
            // NOT using default value
            //

            if (space == newSpace) {

                CHECK(slot < newSlot, "unhandled");

                //
                // continue filling current space;
                //
                // fill-in from slot to newSlot with value y;
                //
                std::memset(units.data() + slot, y, (newSlot - slot));

                hasNonDefaultValue = true;

            } else {

                ASSERT(space < newSpace);

                //
                // finish filling current space
                //
                std::memset(units.data() + slot, y, (S - slot));

                map[space] = units;

                hasNonDefaultValue = false;

                //
                // completely start and finish any whole spaces between space + 1 and newSpace
                //
                if (space + 1 < newSpace) {

                    for (uint16_t sp = space + 1; sp < newSpace; sp++) {

                        //
                        // this is not actually exercised by any known .tbt file
                        //

                        map[sp].fill(y);
                    }
                }

                if (0 < newSlot) {

                    //
                    // start filling new space
                    //
                    std::memset(units.data() + 0, y, (newSlot - 0));

                    hasNonDefaultValue = true;
                }
            }
        }

        unit = newUnit;

        space = newSpace;
        slot = newSlot;
    }

#ifndef NDEBUG
    ASSERT(static_cast<uint32_t>(unit) == unitCount);
#else
    (void)unitCount;
#endif // NDEBUG

    return OK;
}


#undef TAG











