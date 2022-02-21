/* Author: Zachery Creech
 * COSC494 Fall 2021
 * Lab 0: AliceGame.cpp
 * This file contains implementation for the AliceGame class that solves TopCoder
 * SRM 639, D1, 250-pointer. However, this implementation's findMinimumValue() returns
 * a long long instead of a long, handling a + b < 2^63.
 * 09/03/2021 */

#include <iostream>
#include <cstdlib>
#include <string>
#include <cmath>

using namespace std;

class AliceGame
{
    public:
        long long a, b, rounds, h;
        long long findMinimumValue(long long score, long long r);       
        long long BinarySearch();
};

//originally recursive, but now uses a binary search to find the solution in roughly O(log(r)) as described in Dr. Plank's presentation
long long AliceGame::findMinimumValue(long long score, long long r)
{
    long long h;

    //this check is leftover from when this function was recursive. The number of rounds, r, would be calculated only in the first recursive call
    if(r == -1)
    {    
        r = sqrtl(a + b); //as stated in Dr. Plank's presentation, the number of rounds is the square root of the sum of Alice's and Kirito's scores
        if(r*r != a + b)
            return -1;
        rounds = r;
    
        h = BinarySearch(); //perform a binary search to find maximum h, the last number of rounds in a row that went to Alice
        //the remaining score and r values will satisfy a base case
        score -= (2*r*h - h*h);
        r -= h; 
    }

    //add the base case returns from the recursion to the h found in the binary search, and that's the answer
    if(score == 2 || b == 2)
        return -1;
    if(score == 0)
        return 0 + h;
    if(score < 2*r && score % 2 != 0)
        return 1 + h;
    if(score <= 2*r && score % 2 == 0)
        return 2 + h;
    if(score == 2*r + 1)
        return 3 + h;

    return -2; //this should never be reached, just wanted to silence the compiler warning
}

//perform a binary search that finds the maximum value of h that is the largest value on this parabola:
//2*r*h - h*h
//that also satisfies this inequality:
//0 <= a - f(r,h) <= 2(r - h) + 1
//as described in Dr. Plank's presentation
long long AliceGame::BinarySearch()
{
    long long lower, upper, h, result;

    lower = 0;
    upper = rounds;
    h = upper / 2; //start in the middle

    //search until h is found
    while(1)
    {
        result = a - (2*rounds*h - h*h);

        //adjust bounds and h until inequality is satisfied
        if(result < 0) //too far to the right on the parabola
        {
            upper = h - 1;
            h = (lower + upper) / 2;
        }
        else if(result > 2*(rounds - h) + 1) //too far to the left on the parabola
        {
            lower = h + 1;
            h = (lower + upper) / 2;
        }
        else
            return h;
    }
}
