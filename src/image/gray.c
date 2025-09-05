/*
    Contains grayscale processing related functions, exposed api is in `gray.h`
*/

#include "gray.h"

#include <immintrin.h>
#include <stdint.h>

#include "branch.h"
#include "types.h"

ErrorCode boxBlurGray(const unsigned char *const grayInput,
		      unsigned char *const blurredOutput,
		      const FrameDimensions *dimensions) {
    if (UNLIKELY(grayInput == NULL || blurredOutput == NULL ||
		 dimensions == NULL)) {
	return ERROR_INVALID_ARGUMENT;
    }
    if (UNLIKELY(dimensions->width == 0 || dimensions->height == 0)) {
	return ERROR_INVALID_ARGUMENT;
    }
    if (UNLIKELY(dimensions->width % 4 != 0)) {
	return ERROR_INVALID_ARGUMENT;
    }

    for (unsigned int row = 1; row < dimensions->height - 1; row++) {
	for (unsigned int column = 1; column <= dimensions->width - 33;
	     column += 32) {
	    __m256i top = _mm256_loadu_si256((const __m256i *)&grayInput[(
		((row - 1) * dimensions->width) + column - 1)]);
	    __m256i mid = _mm256_loadu_si256(
		(const __m256i
		     *)&grayInput[(row * dimensions->width) + column - 1]);
	    __m256i bottom = _mm256_loadu_si256(
		(const __m256i
		     *)&grayInput[((row + 1) * dimensions->width) + column]);

	    __m256i t_lo = _mm256_unpacklo_epi8(top, _mm256_setzero_si256());
	    __m256i m_lo = _mm256_unpacklo_epi8(mid, _mm256_setzero_si256());
	    __m256i b_lo = _mm256_unpacklo_epi8(bottom, _mm256_setzero_si256());
	    __m256i t_hi = _mm256_unpackhi_epi8(top, _mm256_setzero_si256());
	    __m256i m_hi = _mm256_unpackhi_epi8(mid, _mm256_setzero_si256());
	    __m256i b_hi = _mm256_unpackhi_epi8(bottom, _mm256_setzero_si256());

	    __m256i sum_lo =
		_mm256_add_epi16(_mm256_add_epi16(t_lo, m_lo), b_lo);
	    __m256i sum_hi =
		_mm256_add_epi16(_mm256_add_epi16(t_hi, m_hi), b_hi);

	    __m256i left = _mm256_loadu_si256(
		(const __m256i *)&grayInput[((row - 1) * dimensions->width) +
					    column - 2]);
	    __m256i right = _mm256_loadu_si256(
		(const __m256i
		     *)&grayInput[((row - 1) * dimensions->width) + column]);

	    __m256i l_lo = _mm256_unpacklo_epi8(left, _mm256_setzero_si256());
	    __m256i l_hi = _mm256_unpackhi_epi8(left, _mm256_setzero_si256());
	    __m256i r_lo = _mm256_unpacklo_epi8(right, _mm256_setzero_si256());
	    __m256i r_hi = _mm256_unpackhi_epi8(right, _mm256_setzero_si256());

	    sum_lo = _mm256_add_epi16(sum_lo, _mm256_add_epi16(l_lo, r_lo));
	    sum_hi = _mm256_add_epi16(sum_hi, _mm256_add_epi16(l_hi, r_hi));

	    __m256i bias = _mm256_set1_epi16(4);
	    sum_lo = _mm256_add_epi16(sum_lo, bias);
	    sum_hi = _mm256_add_epi16(sum_hi, bias);
	    sum_lo = _mm256_mulhi_epu16(sum_lo, _mm256_set1_epi16(7282));
	    sum_hi = _mm256_mulhi_epu16(sum_hi, _mm256_set1_epi16(7282));

	    __m256i out = _mm256_packus_epi16(sum_lo, sum_hi);
	    _mm256_storeu_si256(
		(__m256i *)&blurredOutput[(row * dimensions->width) + column],
		out);
	}
    }

    return ERROR_NONE;
}
