/* Author: Zachery Creech
 * COSC494 Fall 2021
 * Lab 5: AP-Flow-SIMD.cpp
 * This solves the All Pairs Max Flow Paths problem using Intel's SSE2 instructions,
 * achieving significant speedup in a Floyd-Warshall fashion. The problem is to compute
 * the flow of the maximum flow path between every pair of nodes in a weighted, directed
 * graph. Rather than having to compare every node in a row to every other path individually,
 * it can be done 16 nodes at a time (16 1-byte numbers can be stored in a __m128i vector). 
 * 11/30/2021 */

#include "AP-Flow.h"
#include <emmintrin.h>

void APFlow::CalcFlow()
{
	int i, j, v;

	__m128i alli, *vv, fv, *iv, rv;
	
	for (i = 0; i < N*N; i++) Flow[i] = Adj[i];

	for (v = 0; v < N; v++) 
	{
		for (i = 0; i < N; i++) 
		{
			//load __m128i vector with 16 instances of Flow[i*N+v]
			alli = _mm_set1_epi8(Flow[i*N+v]);

			for (j = 0; j < N; j+=16) 
			{
				//load __m128i vector with Flow[v*N+j] through Flow[v*N+j+15] by casting uint8_t pointer (of size 16) into __m128i pointer
				vv = (__m128i *)(&Flow[v*N+j]);

				//get the min of each number compared between __m128i vectors alli and vv
				fv = _mm_min_epu8(alli, *vv);
				
				//load __m128i vector with Flow[i*N+j] through Flow[i*N+j+15] by casting uint8_t pointer (of size 16) into __m128i pointer
				iv = (__m128i *)(&Flow[i*N+j]);

				//get the max of each number compared between __m128i vectors fv and iv
				rv = _mm_max_epu8(fv, *iv);
	
				//load __m128i vector rv into Flow[i*N+j] through Flow[i*N+j+15] by casting uint8_t pointer (of size 16) into __m128i pointer, 
				//dereferencing it to get the actual data as a __m128i vector, and setting it equal to rv
				*((__m128i *)(&Flow[i*N+j])) = rv;
			}   
		}
	}
}
