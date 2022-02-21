#include <vector>

using namespace std;

class StonesOnATree
{
  public:
    vector< vector<int> > Adj;
    vector<int> W;
    int maxScore;
    int minStones(vector <int> p, vector <int> w);
    int S(int n);
};

int StonesOnATree::minStones(vector <int> p, vector <int> w)
{
    int rootScore;
    vector<int> children;

    W = w;
    Adj.push_back(children);

    for(int i = 1; i < p.size(); i++)
    {
        while(p[i - 1] > Adj.size() - 1)
            Adj.push_back(children);
        Adj[p[i - 1]].push_back(i);
    }

    while(p[p.size() - 1] > Adj.size() - 1)
        Adj.push_back(children);
    Adj[p[p.size() - 1]].push_back(p.size());

    while(Adj.size() < p.size() + 1)
        Adj.push_back(children);

    return S(0);
}

int StonesOnATree::S(int n)
{
    if(Adj[n].empty())
        return W[n];

    int leftS, rightS, bigger, leftFirst, rightFirst, possibleS, totalW;

    leftS = S(Adj[n][0]);

    if(Adj[n].size() > 1)
    {
        rightS = S(Adj[n][1]);

        if(leftS > rightS)
            bigger = leftS;
        else
            bigger = rightS;

        totalW = W[Adj[n][0]] + W[Adj[n][1]] + W[n];

        leftFirst = W[Adj[n][0]] + rightS;
        rightFirst = W[Adj[n][1]] + leftS;

        if(totalW < leftFirst && totalW < rightFirst)
        {
            if(leftFirst < rightFirst)
                possibleS = leftFirst;
            else
                possibleS = rightFirst;

            if(possibleS < bigger)
                return bigger;
            else
                return possibleS;
        }
        else if(totalW > bigger)
            return totalW;
        else
            return bigger;
    }
    else
    {
        totalW = W[Adj[n][0]] + W[n];

        if(totalW > leftS)
            return totalW;
        else
            return leftS;
    }
}
