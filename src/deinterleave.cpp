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

#include "deinterleave.hpp"

const char* getCurrentSIMD(){
#if defined(__ARM_NEON__)
    return "ARM NEON";
#elif defined(__AVX2__)
    return "AVX2";
#elif defined(__AVX__)
    return "AVX";
#else
    return "NO SIMD";
#endif
}


void deinterleave(const float* interleaved, std::vector<std::vector<REAL>> &output, size_t numSamples, size_t numChannels) {
    size_t i = 0;
    
    output = std::vector<std::vector<REAL>>(numChannels);
    for(int i = 0; i < numChannels; ++i)
        output[i] = std::vector<REAL>(numSamples);
    
#if defined(__ARM_NEON__)
    const size_t simd_width = 4; // 4 floats
    
    if(numChannels == 1){
        for (; i + simd_width <= numSamples; i += simd_width) {
            float32x4_t samples = vld1q_f32(&interleaved[i]);
            vst1q_f32(&output[0][i], samples);
        }
    }
    else if(numChannels==2){
        for (; i + simd_width <= numSamples; i += simd_width) {
            // (L0, R0, L1, R1, L2, R2, L3, R3)
            float32x4x2_t stereo = vld2q_f32(&interleaved[i * 2]);
            for(int c = 0; c < numChannels; ++c){
                vst1q_f32(&output[c][i], stereo.val[c]);
            }
        }
    }
    else if(numChannels==3){
        for (; i + simd_width <= numSamples; i += simd_width) {
            // (L0, R0, C0, L1, R1, C1, L2, R2, C2, L3, R3, C3)
            float32x4x3_t multi = vld3q_f32(&interleaved[i * 3]);
            for(int c = 0; c < numChannels; ++c){
                vst1q_f32(&output[c][i], multi.val[c]);
            }
        }
    }
    else if(numChannels==4){
        for (; i + simd_width <= numSamples; i += simd_width) {
            // (L0, R0, C0, D0, L1, R1, C1, D1
            float32x4x4_t multi = vld4q_f32(&interleaved[i * 4]);
            for(int c = 0; c < numChannels; ++c){
                vst1q_f32(&output[c][i], multi.val[c]);
            }
        }
    }
#elif defined(__AVX2__)
    const size_t simd_width = 8; // 8 floats per AVX register
    
    if (numChannels == 1) {
        for (; i + simd_width <= numSamples; i += simd_width) {
            __m256 samples = _mm256_loadu_ps(&interleaved[i]);
            _mm256_storeu_ps(&output[0][i], samples);
        }
    }
    else if (numChannels == 2) {
        for (; (i + simd_width * numChannels) <= (numSamples * numChannels); i += (simd_width * numChannels)) {
            __m256 stereo1 = _mm256_loadu_ps(&interleaved[i]); // l0, r0, l1, r1, ..., l3, r3
            __m256 stereo2 = _mm256_loadu_ps(&interleaved[i + simd_width]); // l4, r4, l5, r5, ..., l7, r7
            
            __m256 deinterleavedL_low = _mm256_permutevar8x32_ps(stereo1, _mm256_setr_epi32(0, 2, 4, 6, -1, -1, -1, -1));
            __m256 deinterleavedL_high = _mm256_permutevar8x32_ps(stereo2, _mm256_setr_epi32(-1, -1, -1, -1, 0, 2, 4, 6));
            
            __m256 deinterleavedR_low = _mm256_permutevar8x32_ps(stereo1, _mm256_setr_epi32(1, 3, 5, 7, -1, -1, -1, -1));
            __m256 deinterleavedR_high = _mm256_permutevar8x32_ps(stereo2, _mm256_setr_epi32(-1, -1, -1, -1, 1, 3, 5, 7));
            
            __m256 deinterleavedL = _mm256_blend_ps(deinterleavedL_low, deinterleavedL_high, 0b11110000);
            __m256 deinterleavedR = _mm256_blend_ps(deinterleavedR_low, deinterleavedR_high, 0b11110000);
            
            _mm256_storeu_ps(output[0].data() + i / numChannels, deinterleavedL); // L channel
            _mm256_storeu_ps(output[1].data() + i / numChannels, deinterleavedR); // R channel
        }
        
        for (; i < numSamples * numChannels; i += numChannels) {
            output[0][i / numChannels] = interleaved[i]; // L channel
            output[1][i / numChannels] = interleaved[i + 1]; // R channel
        }
    }
    else if (numChannels == 3) {
        for (; (i + simd_width * numChannels) <= (numSamples * numChannels); i += (simd_width * numChannels)) {
            __m256 vec0 = _mm256_loadu_ps(interleaved + i); // l0, r0, c0, l1, r1, c1, l2, r2
            __m256 vec1 = _mm256_loadu_ps(interleaved + i + simd_width); // c2, l3, r3, c3, l4, r4, c4, l5
            __m256 vec2 = _mm256_loadu_ps(interleaved + i + 2 * simd_width); // r5, c5, l6, r6, c6, l7, r7, c7
            
            __m256 shuffled_l0 = _mm256_permutevar8x32_ps(vec0, _mm256_set_epi32(0, 0, 0, 0, 0, 6, 3, 0));
            __m256 shuffled_l1 = _mm256_permutevar8x32_ps(vec1, _mm256_set_epi32(0, 0, 7, 4, 1, 0, 0, 0));
            __m256 shuffled_l2 = _mm256_permutevar8x32_ps(vec2, _mm256_set_epi32(5, 2, 0, 0, 0, 0, 0, 0));
            shuffled_l0 = _mm256_blend_ps(shuffled_l1, shuffled_l0, 0b00000111);
            shuffled_l0 = _mm256_blend_ps(shuffled_l2, shuffled_l0, 0b00111111);
            
            __m256 shuffled_r0 = _mm256_permutevar8x32_ps(vec0, _mm256_set_epi32(0, 0, 0, 0, 0, 7, 4, 1));
            __m256 shuffled_r1 = _mm256_permutevar8x32_ps(vec1, _mm256_set_epi32(0, 0, 0, 5, 2, 0, 0, 0));
            __m256 shuffled_r2 = _mm256_permutevar8x32_ps(vec2, _mm256_set_epi32(6, 3, 0, 0, 0, 0, 0, 0));
            shuffled_r0 = _mm256_blend_ps(shuffled_r1, shuffled_r0, 0b00000111);
            shuffled_r0 = _mm256_blend_ps(shuffled_r2, shuffled_r0, 0b00011111);
            
            __m256 shuffled_c0 = _mm256_permutevar8x32_ps(vec0, _mm256_set_epi32(0, 0, 0, 0, 0, 0, 5, 2));
            __m256 shuffled_c1 = _mm256_permutevar8x32_ps(vec1, _mm256_set_epi32(0, 0, 0, 6, 3, 0, 0, 0));
            __m256 shuffled_c2 = _mm256_permutevar8x32_ps(vec2, _mm256_set_epi32(7, 4, 1, 0, 0, 0, 0, 0));
            shuffled_c0 = _mm256_blend_ps(shuffled_c1, shuffled_c0, 0b00000011);
            shuffled_c0 = _mm256_blend_ps(shuffled_c2, shuffled_c0, 0b00011111);
            
            _mm256_storeu_ps(output[0].data() + i / numChannels, shuffled_l0); // L channel
            _mm256_storeu_ps(output[1].data() + i / numChannels, shuffled_r0); // R channel
            _mm256_storeu_ps(output[2].data() + i / numChannels, shuffled_c0); // C channel
        }
        
        for (; i < numSamples * numChannels; i += numChannels) {
            output[0][i / numChannels] = interleaved[i]; // L channel
            output[1][i / numChannels] = interleaved[i + 1]; // R channel
            output[2][i / numChannels] = interleaved[i + 2]; // C channel
        }
    }
    else if (numChannels == 4) {
        for (; (i + simd_width * numChannels) <= (numSamples * numChannels); i += (simd_width * numChannels)) {
            __m256 vec0 = _mm256_loadu_ps(interleaved + i); // l0, r0, c0, s0, l1, r1, c1, s1
            __m256 vec1 = _mm256_loadu_ps(interleaved + i + simd_width); // l2, r2, c2, s2, l3, r3, c3, s3
            __m256 vec2 = _mm256_loadu_ps(interleaved + i + 2 * simd_width); // l4, r4, c4, s4, l5, r5, c5, s5
            __m256 vec3 = _mm256_loadu_ps(interleaved + i + 3 * simd_width); // l6, r6, c6, s6, l7, r7, c7, s7
            
            __m256 shuffled_l0 = _mm256_permutevar8x32_ps(vec0, _mm256_set_epi32(0, 0, 0, 0, 0, 0, 4, 0));
            __m256 shuffled_l1 = _mm256_permutevar8x32_ps(vec1, _mm256_set_epi32(0, 0, 0, 0, 4, 0, 0, 0));
            __m256 shuffled_l2 = _mm256_permutevar8x32_ps(vec2, _mm256_set_epi32(0, 0, 4, 0, 0, 0, 0, 0));
            __m256 shuffled_l3 = _mm256_permutevar8x32_ps(vec3, _mm256_set_epi32(4, 0, 0, 0, 0, 0, 0, 0));
            shuffled_l0 = _mm256_blend_ps(shuffled_l1, shuffled_l0, 0b00000011);
            shuffled_l0 = _mm256_blend_ps(shuffled_l2, shuffled_l0, 0b00001111);
            shuffled_l0 = _mm256_blend_ps(shuffled_l3, shuffled_l0, 0b00111111);
            
            __m256 shuffled_r0 = _mm256_permutevar8x32_ps(vec0, _mm256_set_epi32(0, 0, 0, 0, 0, 0, 5, 1));
            __m256 shuffled_r1 = _mm256_permutevar8x32_ps(vec1, _mm256_set_epi32(0, 0, 0, 0, 5, 1, 0, 0));
            __m256 shuffled_r2 = _mm256_permutevar8x32_ps(vec2, _mm256_set_epi32(0, 0, 5, 1, 0, 0, 0, 0));
            __m256 shuffled_r3 = _mm256_permutevar8x32_ps(vec3, _mm256_set_epi32(5, 1, 0, 0, 0, 0, 0, 0));
            shuffled_r0 = _mm256_blend_ps(shuffled_r1, shuffled_r0, 0b00000011);
            shuffled_r0 = _mm256_blend_ps(shuffled_r2, shuffled_r0, 0b00001111);
            shuffled_r0 = _mm256_blend_ps(shuffled_r3, shuffled_r0, 0b00111111);
            
            __m256 shuffled_c0 = _mm256_permutevar8x32_ps(vec0, _mm256_set_epi32(0, 0, 0, 0, 0, 0, 6, 2));
            __m256 shuffled_c1 = _mm256_permutevar8x32_ps(vec1, _mm256_set_epi32(0, 0, 0, 0, 6, 2, 0, 0));
            __m256 shuffled_c2 = _mm256_permutevar8x32_ps(vec2, _mm256_set_epi32(0, 0, 6, 2, 0, 0, 0, 0));
            __m256 shuffled_c3 = _mm256_permutevar8x32_ps(vec3, _mm256_set_epi32(6, 2, 0, 0, 0, 0, 0, 0));
            shuffled_c0 = _mm256_blend_ps(shuffled_c1, shuffled_c0, 0b00000011);
            shuffled_c0 = _mm256_blend_ps(shuffled_c2, shuffled_c0, 0b00001111);
            shuffled_c0 = _mm256_blend_ps(shuffled_c3, shuffled_c0, 0b00111111);
            
            __m256 shuffled_s0 = _mm256_permutevar8x32_ps(vec0, _mm256_set_epi32(0, 0, 0, 0, 0, 0, 7, 3));
            __m256 shuffled_s1 = _mm256_permutevar8x32_ps(vec1, _mm256_set_epi32(0, 0, 0, 0, 7, 3, 0, 0));
            __m256 shuffled_s2 = _mm256_permutevar8x32_ps(vec2, _mm256_set_epi32(0, 0, 7, 3, 0, 0, 0, 0));
            __m256 shuffled_s3 = _mm256_permutevar8x32_ps(vec3, _mm256_set_epi32(7, 3, 0, 0, 0, 0, 0, 0));
            shuffled_s0 = _mm256_blend_ps(shuffled_s1, shuffled_s0, 0b00000011);
            shuffled_s0 = _mm256_blend_ps(shuffled_s2, shuffled_s0, 0b00001111);
            shuffled_s0 = _mm256_blend_ps(shuffled_s3, shuffled_s0, 0b00111111);
            
            _mm256_storeu_ps(output[0].data() + i / numChannels, shuffled_l0); // L channel
            _mm256_storeu_ps(output[1].data() + i / numChannels, shuffled_r0); // R channel
            _mm256_storeu_ps(output[2].data() + i / numChannels, shuffled_c0); // C channel
            _mm256_storeu_ps(output[3].data() + i / numChannels, shuffled_s0); // S channel
        }
        
        for (; i < numSamples * numChannels; i += numChannels) {
            output[0][i / numChannels] = interleaved[i]; // L channel
            output[1][i / numChannels] = interleaved[i + 1]; // R channel
            output[2][i / numChannels] = interleaved[i + 2]; // C channel
            output[3][i / numChannels] = interleaved[i + 3]; // S channel
        }
    }
#elif defined(__AVX__)
    const size_t simd_width = 4; // 4 floats per AVX register

    if (numChannels == 1) {
        for (size_t i = 0; i + simd_width <= numSamples; i += simd_width) {
            __m128 samples = _mm_loadu_ps(&interleaved[i]);

            _mm_storeu_ps(&output[0][i], samples);
        }

        for (; i < numSamples; ++i) {
            output[0][i] = interleaved[i];
        }
    } else if (numChannels == 2) {
        for (size_t i = 0; i + simd_width * numChannels <= numSamples * numChannels; i += simd_width * numChannels) {
            __m128 stereo1 = _mm_loadu_ps(&interleaved[i]); // l0, r0, l1, r1
            __m128 stereo2 = _mm_loadu_ps(&interleaved[i + simd_width]); // l2, r2, l3, r3

            __m128 deinterleavedL = _mm_shuffle_ps(stereo1, stereo2, _MM_SHUFFLE(2, 0, 2, 0));
            __m128 deinterleavedR = _mm_shuffle_ps(stereo1, stereo2, _MM_SHUFFLE(3, 1, 3, 1));

            _mm_storeu_ps(output[0].data() + i / numChannels, deinterleavedL); // L channel
            _mm_storeu_ps(output[1].data() + i / numChannels, deinterleavedR); // R channel
        }

        for (; i < numSamples * numChannels; i += numChannels) {
            output[0][i / numChannels] = interleaved[i]; // L channel
            output[1][i / numChannels] = interleaved[i + 1]; // R channel
        }
    } else if (numChannels == 3) {
        for (size_t i = 0; i + simd_width * numChannels <= numSamples * numChannels; i += simd_width * numChannels) {
            __m128 vec0 = _mm_loadu_ps(&interleaved[i]); // l0, r0, c0, l1
            __m128 vec1 = _mm_loadu_ps(&interleaved[i + simd_width]); // r1, c1, l2, r2
            __m128 vec2 = _mm_loadu_ps(&interleaved[i + 2 * simd_width]); // c2, l3, r3, c3

            __m128 shuffled_l = _mm_shuffle_ps(vec0, vec2, _MM_SHUFFLE(3, 1, 0, 0));
            __m128 shuffled_r = _mm_shuffle_ps(vec0, vec2, _MM_SHUFFLE(2, 3, 2, 1));
            __m128 shuffled_c = _mm_shuffle_ps(vec0, vec2, _MM_SHUFFLE(3, 2, 1, 2));

            _mm_storeu_ps(output[0].data() + i / numChannels, shuffled_l); // L channel
            _mm_storeu_ps(output[1].data() + i / numChannels, shuffled_r); // R channel
            _mm_storeu_ps(output[2].data() + i / numChannels, shuffled_c); // C channel
        }

        for (; i < numSamples * numChannels; i += numChannels) {
            output[0][i / numChannels] = interleaved[i]; // L channel
            output[1][i / numChannels] = interleaved[i + 1]; // R channel
            output[2][i / numChannels] = interleaved[i + 2]; // C channel
        }
    } else if (numChannels == 4) {
        for (size_t i = 0; i + simd_width * numChannels <= numSamples * numChannels; i += simd_width * numChannels) {
            __m128 vec0 = _mm_loadu_ps(&interleaved[i]); // l0, r0, c0, s0
            __m128 vec1 = _mm_loadu_ps(&interleaved[i + simd_width]); // l1, r1, c1, s1
            __m128 vec2 = _mm_loadu_ps(&interleaved[i + 2 * simd_width]); // l2, r2, c2, s2
            __m128 vec3 = _mm_loadu_ps(&interleaved[i + 3 * simd_width]); // l3, r3, c3, s3

            __m128 shuffled_l = _mm_shuffle_ps(vec0, vec3, _MM_SHUFFLE(2, 0, 2, 0));
            __m128 shuffled_r = _mm_shuffle_ps(vec0, vec3, _MM_SHUFFLE(3, 1, 3, 1));
            __m128 shuffled_c = _mm_shuffle_ps(vec0, vec3, _MM_SHUFFLE(3, 2, 3, 2));
            __m128 shuffled_s = _mm_shuffle_ps(vec0, vec3, _MM_SHUFFLE(3, 3, 3, 3));

            _mm_storeu_ps(output[0].data() + i / numChannels, shuffled_l); // L channel
            _mm_storeu_ps(output[1].data() + i / numChannels, shuffled_r); // R channel
            _mm_storeu_ps(output[2].data() + i / numChannels, shuffled_c); // C channel
            _mm_storeu_ps(output[3].data() + i / numChannels, shuffled_s); // S channel
        }

        for (; i < numSamples * numChannels; i += numChannels) {
            output[0][i / numChannels] = interleaved[i]; // L channel
            output[1][i / numChannels] = interleaved[i + 1]; // R channel
            output[2][i / numChannels] = interleaved[i + 2]; // C channel
            output[3][i / numChannels] = interleaved[i + 3]; // S channel
        }
    }
#endif
    
    for (; i < numSamples; i++) {
        for(int c = 0; c < numChannels; ++c){
            output[c][i] = interleaved[i * numChannels + c];
        }
    }
}
