/*
* Simd Library (http://simd.sourceforge.net).
*
* Copyright (c) 2011-2015 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdStore.h"

namespace Simd
{
#ifdef SIMD_AVX2_ENABLE
    namespace Avx2
    {
        namespace
        {
            struct Buffer
            {
                Buffer(size_t size)
                {
                    _p = Allocate(sizeof(uint8_t)*size);
                    p = (uint8_t*)_p;
                }

                ~Buffer()
                {
                    Free(_p);
                }

                uint8_t * p;
            private:
                void *_p;
            };
        }

        template <bool srcAlign, bool stepAlign>
        SIMD_INLINE __m256i AbsSecondDerivative(const uint8_t * src, ptrdiff_t step)
        {
            const __m256i s0 = Load<srcAlign && stepAlign>((__m256i*)(src - step));
            const __m256i s1 = Load<srcAlign>((__m256i*)src);
            const __m256i s2 = Load<srcAlign && stepAlign>((__m256i*)(src + step));
            return AbsDifferenceU8(_mm256_avg_epu8(s0, s2), s1);
        }

        template <bool align>
        SIMD_INLINE void AbsSecondDerivative(const uint8_t * src, ptrdiff_t colStep, ptrdiff_t rowStep, uint8_t * dst)
        {
            const __m256i sdX = AbsSecondDerivative<align, false>(src, colStep);
            const __m256i sdY = AbsSecondDerivative<align, true>(src, rowStep);
            Store<align>((__m256i*)dst, _mm256_max_epu8(sdY, sdX));
        }

        template<bool align> void AbsSecondDerivativeHistogram(const uint8_t *src, size_t width, size_t height, size_t stride,
            size_t step, size_t indent, uint32_t * histogram)
        {
            uint32_t SIMD_ALIGNED(32) histograms[4][HISTOGRAM_SIZE];
            memset(histograms, 0, sizeof(uint32_t)*HISTOGRAM_SIZE*4);

            Buffer buffer(stride);
            buffer.p += indent;
            src += indent*(stride + 1);
            height -= 2*indent;
            width -= 2*indent;

            ptrdiff_t bodyStart = (uint8_t*)AlignHi(buffer.p, A) - buffer.p;
            ptrdiff_t bodyEnd = bodyStart + AlignLo(width - bodyStart, A);
            size_t rowStep = step*stride;
            size_t alignedWidth = Simd::AlignLo(width, 4);
            for(size_t row = 0; row < height; ++row)
            {
                if(bodyStart)
                    AbsSecondDerivative<false>(src, step, rowStep, buffer.p);
                for(ptrdiff_t col = bodyStart; col < bodyEnd; col += A)
                    AbsSecondDerivative<align>(src + col, step, rowStep, buffer.p + col);
                if(width != (size_t)bodyEnd)
                    AbsSecondDerivative<false>(src + width - A, step, rowStep, buffer.p + width - A);

                size_t col = 0;
                for(; col < alignedWidth; col += 4)
                {
                    ++histograms[0][buffer.p[col + 0]];
                    ++histograms[1][buffer.p[col + 1]];
                    ++histograms[2][buffer.p[col + 2]];
                    ++histograms[3][buffer.p[col + 3]];
                }
                for(; col < width; ++col)
                    ++histograms[0][buffer.p[col + 0]];
                src += stride;
            }

            for(size_t i = 0; i < HISTOGRAM_SIZE; i += 8)
            {
                Store<false>((__m256i*)(histogram + i), _mm256_add_epi32(
                    _mm256_add_epi32(Load<true>((__m256i*)(histograms[0] + i)), Load<true>((__m256i*)(histograms[1] + i))), 
                    _mm256_add_epi32(Load<true>((__m256i*)(histograms[2] + i)), Load<true>((__m256i*)(histograms[3] + i)))));
            }
        }

        void AbsSecondDerivativeHistogram(const uint8_t *src, size_t width, size_t height, size_t stride,
            size_t step, size_t indent, uint32_t * histogram)
        {
            assert(width > 2*indent && height > 2*indent && indent >= step && width >= A + 2*indent);

            if(Aligned(src) && Aligned(stride))
                AbsSecondDerivativeHistogram<true>(src, width, height, stride, step, indent, histogram);
            else
                AbsSecondDerivativeHistogram<false>(src, width, height, stride, step, indent, histogram);
        }
    }
#endif// SIMD_AVX2_ENABLE
}
