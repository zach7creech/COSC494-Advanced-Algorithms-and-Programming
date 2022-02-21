#include <vector>
#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "StonesOnATree.cpp"
using namespace std;

int main(int argc, char **argv)
{
  int i;
  class StonesOnATree TheClass;
  int retval;
  string l;
  int s;
  vector <int> p;
  vector <int> w;
  istringstream ss;

  if (argc != 2) { fprintf(stderr, "usage: a.out num or -\n"); exit(1); }

  if (((string) argv[1]) == "-") {
    if (!getline(cin, l)) exit(1);
    ss.clear();
    ss.str(l);
    while (ss >> s) p.push_back(s);

    if (!getline(cin, l)) exit(1);
    ss.clear();
    ss.str(l);
    while (ss >> s) w.push_back(s);

    if (p.size() != w.size()-1) {
      fprintf(stderr, "The vectors are not the right size.\n");
      exit(1);
    }

  } else {

    if (atoi(argv[1]) == 0) {
      p.push_back(0);
      p.push_back(1);
      p.push_back(2);
      p.push_back(3);
      w.push_back(1);
      w.push_back(2);
      w.push_back(2);
      w.push_back(4);
      w.push_back(4);
    }
  
    if (atoi(argv[1]) == 1) {
      p.push_back(0);
      p.push_back(0);
      w.push_back(1);
      w.push_back(2);
      w.push_back(3);
    }
  
    if (atoi(argv[1]) == 2) {
      p.push_back(0);
      w.push_back(100000);
      w.push_back(100000);
    }
  
    if (atoi(argv[1]) == 3) {
      p.push_back(0);
      p.push_back(0);
      p.push_back(1);
      p.push_back(1);
      p.push_back(2);
      p.push_back(2);
      w.push_back(1);
      w.push_back(1);
      w.push_back(1);
      w.push_back(1);
      w.push_back(1);
      w.push_back(1);
      w.push_back(1);
    }
  
    if (atoi(argv[1]) == 4) {
      p.push_back(0);
      p.push_back(0);
      p.push_back(1);
      p.push_back(2);
      p.push_back(3);
      p.push_back(4);
      p.push_back(4);
      p.push_back(2);
      p.push_back(1);
      p.push_back(3);
      p.push_back(6);
      p.push_back(7);
      w.push_back(1);
      w.push_back(2);
      w.push_back(3);
      w.push_back(4);
      w.push_back(5);
      w.push_back(6);
      w.push_back(6);
      w.push_back(7);
      w.push_back(8);
      w.push_back(8);
      w.push_back(8);
      w.push_back(9);
      w.push_back(10);
    }
  }

  retval = TheClass.minStones(p, w);
  cout << retval << endl;

  exit(0);
}
