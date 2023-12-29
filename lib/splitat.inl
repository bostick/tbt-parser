// Copyright (C) 2023 by Brenton Bostick
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


//
// adapted from:
// https://subscription.packtpub.com/book/programming/9781803248714/11/ch11lvl1sec14/build-your-own-algorithm-split
//
// Elements that match predicate are kept.
//
template <typename It, typename Oc, typename Pred>
void splitAt(It it, It end_it, Oc &dest, Pred f) {
    using SliceContainer = typename Oc::value_type;
    while (it != end_it) {

        SliceContainer dest_elm{};

        auto slice{it};

        while (slice != end_it) {

            dest_elm.push_back(*slice);

            if (!f(*slice)) {
                break;
            }

            slice++;
        }

        dest.push_back(dest_elm);

        it = ++slice;
    }
}








