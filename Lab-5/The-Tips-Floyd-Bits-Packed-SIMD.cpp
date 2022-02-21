/* Author: Zachery Creech
 * COSC494 Fall 2021
 * Lab 5: The-Tips-Floyd-Bits-Packed-SIMD.cpp
 * This file contains the solution for Topcoder problem "TheTips" using Intel's SSE2
 * instructions to achieve significant speedup in a Floyd-Warshall fashion. Rather than
 * comparing bits one at a time, it is possible to OR 128 bits at once with __m128i vectors.
 * 11/30/2021 */

#include "The-Tips.h"
#include <stdlib.h>
#include <emmintrin.h>

double TheTips::solve(vector <string> Cl, vector <int> probability, int print)
{
	int i, j, v, div64;
	uint64_t **adj, *two_longs, one, zero, extract;	
	
	vector < vector <__m128i> > C;
	__m128i packed;
	__m128i *cur_vec;
	
	vector <double> p;
	double x;
	
	one = 1;

	//figure out how many unint64_t are needed to fit the bits described in Cl
	div64 = Cl.size() / 64;

	if(Cl.size() % 64 != 0)
		div64++;
		
	//number of rows of adjacency matrix will be same as number of strings in Cl
	adj = (uint64_t **)malloc(Cl.size() * sizeof(uint64_t *));
			    
	/* Change the Y/N's to 1/0's */

	for (i = 0; i < Cl.size(); i++) 
	{
		//each row of the adjacency matrix will have the number of uint64_t as calculated above
		adj[i] = (uint64_t *)malloc(div64 * sizeof(uint64_t));
	
		//for each character in the string (a row) set the appropriate bit in the appropriate uint64_t to be 0 or 1
		for (j = 0; j < Cl[i].size(); j++) 
		{
			//each uint64_t needs to be initialized to 0
			if(j % 64 == 0)
				adj[i][j / 64] = 0;
			
			//condensed if from original version of this solution, but OR with a uint64_t with exactly one bit set (1 representing Y)
			adj[i][j / 64] |= (Cl[i][j] == 'Y') ? (one << (63 - (j % 64))) : 0;
		}
		//diagonal bits set to 1
		adj[i][i / 64] |= one << (63 - (i % 64));
	}
		
	/* Pack each adjacent pair of uint64_t's into __m128i vectors */
	
	C.resize(Cl.size());
	
	for(i = 0; i < Cl.size(); i++)
	{
		//pack each pair of uint64_t from adj into a __m128i vector using _mm_set_epi64
		for(j = 0; j < div64; j+=2)
		{
			//this only needs to be done to pad out the last __m128i with zeros if there's an odd number of uint64_t
			if(j == div64 - 1 && div64 % 2 != 0)
				packed = _mm_set_epi64((__m64)(adj[i][j]), (__m64)zero);
			//pack each pair
			else
				packed = _mm_set_epi64((__m64)adj[i][j], (__m64)adj[i][j + 1]);
			C[i].push_back(packed);
		}
	}

	/* Do Floyd-Warshall */
	
	for (v = 0; v < Cl.size(); v++) 
	{
		for (i = 0; i < Cl.size(); i++) 
		{
			//extract the right __m128i vector
			cur_vec = &(C[i][v / 128]);

			two_longs = (uint64_t *)cur_vec;
			
			//bit to be checked represented with this uint64_t with exactly one bit set
			extract = one << (63 - (v % 64));
				
			//decide if this bit is in the "upper" or "lower" uint64_t and extract it by AND with extract number set above
			if(v % 128 < 64)
				extract &= two_longs[1];
			else	
				extract &= two_longs[0];

			//if extracted bit is a 1, OR the entire row i with row v, 128 bits at a time and save in row i
			if (extract >> (63 - (v % 64))) 
			{
				for(j = 0; j < C[i].size(); j++)
					C[i][j] = _mm_or_si128(C[i][j], C[v][j]);
			}
		}
	}
	
	/* Calculate the values of p from the probabilities and reachability: */

	p.resize(C.size(), 0);

	for (i = 0; i < C.size(); i++) 
	{	
		x = probability[i];
		x /= 100.0;
		for (j = 0; j < Cl.size(); j++) 
		{
			//extract correct __m128i vector and bit exactly as above
			cur_vec = &(C[i][j / 128]);

			two_longs = (uint64_t *)cur_vec;
			
			extract = one << (63 - (j % 64));
				
			if(j % 128 < 64)
				extract &= two_longs[1];
			else	
				extract &= two_longs[0];
			
			//if that bit is a 1, increment probability at p[j]
			if (extract >> (63 - (j % 64))) p[j] += ((1 - p[j]) * x);
		}
	}
					    
	/* Calculate the final return value */

	x = 0;
	for (i = 0; i < C.size(); i++) x += p[i];
					 
	return x;
}
