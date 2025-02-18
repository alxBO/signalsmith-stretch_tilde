/**
 MIT License

 Copyright (c) 2025 Alex Bouvier.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */


#ifndef deinterleave_hpp
#define deinterleave_hpp

#include <vector>
#include "common.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
#elif defined(__AVX__) ||  defined(__AVX2__)
    #include <immintrin.h>
#endif

const char* getCurrentSIMD();


void deinterleave(const float* interleaved, std::vector<std::vector<REAL>> &output, size_t numSamples, size_t numChannels);

#endif /* deinterleave_hpp */
