/* Author: Zachery Creech
 * COSC494 Fall 2021
 * Lab 4: NegativeGraphDiv1.cpp
 * This file contains the solution for TopCoder SRM 626 D1 NegativeGraphDiv1.
 * It utilizes a Floyd-Warshall like algorithm to create adjacency matrices
 * containing the shortest paths through the graph when a number of charges
 * are used (see problem description on TopCoder website). This solution is
 * taken directly from Dr. James Plank's writeup (jplank@utk.edu).
 * 11/15/2021 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

using namespace std;

int main()
{
	string N, from, to, weight;
	stringstream from_ss, to_ss, weights_ss;
	int N_int, from_int, to_int, weight_int, charges;
	int x, y, i, j, f, t, z, power, cur_charge;
	vector< vector<long long> > S, L, A, new_A_temp, new_A, cur_A;
	vector<long long> S_edges, L_edges;
	vector< vector< vector<long long> > > all_As;

	//read in number of nodes N and resize the row vectors
	getline(cin, N);
	N_int = stoi(N);

	S_edges.resize(N_int);
	L_edges.resize(N_int);

	//fill S (shortest paths) template with infinity (6000000) and L (largest edges) template with 0
	for(i = 0; i < N_int; i++)
	{
		S_edges[i] = 6000000;
		L_edges[i] = 0;
	}
		
	//create all matrices to be used and zero out the diagonal (self edges) in S
	for(i = 0; i < N_int; i++)
	{	
		S.push_back(S_edges);
		L.push_back(L_edges);
		A.push_back(S_edges);
		new_A_temp.push_back(S_edges);

		S[i][i] = 0;
	}
	
	//read in the from, to, and weights arrays (as strings) and put them in stringstreams to extract as ints
	getline(cin, from);
	from_ss << from;
	getline(cin, to);
	to_ss << to;
	getline(cin, weight);
	weights_ss << weight;
	
	cin >> charges;

	//extract the numbers from the stringstreams and put them in appropriate matrices
	while(!from_ss.eof())
	{
		from_ss >> from;
		to_ss >> to;
		weights_ss >> weight;

		from_int = stoi(from);
		to_int = stoi(to);
		weight_int = stoi(weight);

		//put shortest edge between each node in S
		if(weight_int < S[from_int - 1][to_int - 1])
			S[from_int - 1][to_int - 1] = weight_int;

		//put largest edge between each node in L
		if(weight_int > L[from_int - 1][to_int - 1])
			L[from_int - 1][to_int - 1] = weight_int;
	}

	//Floyd-Warshall on S to find shortest path between each node
	for(i = 0; i < N_int; i++)
	{
		for(x = 0; x < N_int; x++)
		{
			for(y = 0; y < N_int; y++)
			{
				if(S[x][i] + S[i][y] < S[x][y])
					S[x][y] = S[x][i] + S[i][y];
			}
		}
	}

	//if no charges can be used, the shortest path is already contained in S
	if(charges == 0)
	{
		cout << S[0][N_int - 1] << '\n';
		return 0;
	}
	
	//otherwise construct an adjacency matrix of shortest paths if exactly one charge can be used
	//as stated in Dr. Plank's writeup, this is accomplished by subtracting the value of the largest
	//edge contained in L for each intermediate node (i, j)
	for(f = 0; f < N_int; f++)
	{
		for(t = 0; t < N_int; t++)
		{
			for(i = 0; i < N_int; i++)
			{
				for(j = 0; j < N_int; j++)
				{
					//L was initialized to all 0s, so 0s not on the diagonal represent infinity (no edge) and should not be considered
					if(L[i][j] != 0 || i == j)
					{
						if(S[f][i] + S[j][t] - L[i][j] < A[f][t])
							A[f][t] = S[f][i] + S[j][t] - L[i][j];
					}
				}
			}
		}
	}
	
	//push back adjacency matrix with shortest paths using exactly one charge
	//this will be used to build matrices with shortest paths using more charges
	all_As.push_back(A);
	
	power = 1;
	i = 0;

	//add A_charges matrices together to find shortest paths using charges that are powers of 2
	//e.g. A_2 can be found by adding A_1 to itself, A_4 can be found by adding A_2 to itself, etc.
	//this finds the largest power of 2 that is smaller than charges
	while(power * 2 <= charges)
	{	
		new_A = new_A_temp;

		for(f = 0; f < N_int; f++)
		{
			for(t = 0; t < N_int; t++)
			{
				for(z = 0; z < N_int; z++)
				{
					if(all_As[i][f][z] + all_As[i][z][t] < new_A[f][t])
						new_A[f][t] = all_As[i][f][z] + all_As[i][z][t];
				}
			}
		}

		//all_As stores A_2^index at each index, i.e. all_As[0] stores A_1, all_As[1] stores A_2, all_As[2] stores A_4, etc.
		all_As.push_back(new_A);
		power *= 2;
		i++;
	}
	
	//add A_i to A_j to get A_x as described in the writeup. Starting A_j is the largest power of 2 just found
	cur_charge = pow(2, all_As.size() - 1);
	
	cur_A = all_As[all_As.size() - 1];
	new_A = new_A_temp;

	//work backwards to add each power of 2 needed to equal charges
	//it's like a binary number: to get 19, start with A_16. Then try to add A_8, that's larger than 19 so skip it.
	//Try to add A_4, that's still larger than 19 so skip it. Try to add A_2, that works, so now you have A_18. Try to
	//add A_1, that works, and now you have A_19, the answer
	for(i = all_As.size() - 2; i >= 0; i--)
	{
		if(pow(2, i) + cur_charge <= charges)
		{
			for(f = 0; f < N_int; f++)
			{
				for(t = 0; t < N_int; t++)
				{
					for(z = 0; z < N_int; z++)
					{
						if(all_As[i][f][z] + cur_A[z][t] < new_A[f][t])
							new_A[f][t] = all_As[i][f][z] + cur_A[z][t];
					}
				}
			}
			cur_charge += pow(2, i);
			cur_A = new_A;
			new_A = new_A_temp;
		}
	}
	
	//shortest path from first to last node is at the end of the first row in A_charges
	cout << cur_A[0][N_int - 1] << '\n';

	return 0;
}
