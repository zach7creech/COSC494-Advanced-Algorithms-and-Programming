#include <string>
#include <vector>
#include <list>
#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
using namespace std;


void create(int id, int nodes, vector < vector <int> > &adj)
{
  int nl;

  if (nodes == 1) return;
  if (nodes == 2 || drand48() < .5) {
    adj[id].push_back(id+1);
    create(id+1, nodes-1, adj);
  } else {
    nl = (int) (drand48() * (nodes-2)) + 1;
    adj[id].push_back(id+1);
    adj[id].push_back(id+1+nl);
    create(id+1, nl, adj);
    create(id+1+nl, nodes-1-nl, adj);
  }
}


int main()
{
  vector < vector <int> > adj;
  vector < int> w;
  vector < int> p;
  int n, j;

  cin >> n;

  srand48(time(0));
  adj.resize(n);
  create(0, n, adj);

  p.resize(n-1);

  for (n = 0; n < adj.size(); n++) {
    w.push_back((drand48() * 100000) + 1);
    for (j = 0; j < adj[n].size(); j++) {
      p[adj[n][j]-1] = n;
    }
  }

  printf("( echo");
  for (n = 0; n < p.size(); n++) printf(" %d", p[n]); 
  printf(" ; echo");
  for (n = 0; n < w.size(); n++) printf(" %d", w[n]); 
  printf(" ) | ./a.out -\n");

  return 0;
}
