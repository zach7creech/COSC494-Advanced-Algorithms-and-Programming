/* Author: Zachery Creech
 * COSC494 Fall 2021
 * Lab 0: BoardFolding.cpp
 * This file contains the implementation for the BoardFolding class that solves TopCoder
 * SRM 639, D1, 500-pointer. However, this implementation's howMany() returns a long long
 * instead of an int, handling R and C values <= 3000.
 * 09/07/2021 */

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>

using namespace std;

class BoardFolding
{
    public:
        long long howMany(long long r, long long c, vector<string> &compressedPaper);
        vector<int> starting_places(vector<int> &grid);
        long long countFolds(vector<int> &start, vector<int> &end);
        int ctoi(int c);
};

//this algorithm should be roughly O(n^2) as described in Dr. Plank's presentation
long long BoardFolding::howMany(long long r, long long c, vector<string> &compressedPaper)
{
    unordered_map<string, int> rowID, colID;
    unordered_map<string, int>::iterator mitr;
    long long uniqueRow = 0, uniqueCol = 0;
    string row;
    vector<string> stringGrid;
    vector<int> gridRows, gridCols; 
    
    //first decompress the board into a vector of strings of 0s and 1s
    //save those rows as strings to create the rotated board (columns become rows)'s vector of ints
    //at the same time, go ahead and create the rows vector of ints to eliminate string comparison in starting_places
    for(int i = 0; i < r; i++)
    {
        row = "";

        //decompress the current row
        for(int j = 0; j < c; j++)
            row += to_string((ctoi(compressedPaper[i][j / 6]) >> (j % 6)) % 2);

        stringGrid.push_back(row); //always push back string of 0s and 1s to transpose for columns

        mitr = rowID.find(row); //see if this row's pattern has been found before
        
        if(mitr == rowID.end()) //if it hasn't, add it to the map, add its integer equivalent to the vector, and increment it
        {
            rowID[row] = uniqueRow;
            gridRows.push_back(uniqueRow);
            uniqueRow++;
        }
        else
            gridRows.push_back(mitr->second); //otherwise just get the pattern's ID from the map and add it to the vector
    }

    //similar to above but decompression is already done
    //starting in the bottom left corner of the original board, read up each column to make another board with the original columns as rows
    //rotates original board clockwise by 90 degrees
    for(int j = 0; j < stringGrid[0].size(); j++)
    {
        row = "";

        //build each row character by character
        for(int i = stringGrid.size() - 1; i >= 0; i--)
            row += stringGrid[i][j];

        mitr = colID.find(row); //see if this row's pattern  has been found before

        if(mitr == colID.end()) //if it hasn't, add it to the map, add its integer equivalent to the vector, and increment it
        {
            colID[row] = uniqueCol;
            gridCols.push_back(uniqueCol);
            uniqueCol++;
        }
        else
            gridCols.push_back(mitr->second); //otherwise just get the pattern's ID from the map and add it to the vector
    }

    vector<int> rowStart, rowEnd, colStart, colEnd;
    long long rowFolds, colFolds;

    rowStart = starting_places(gridRows); //call starting_places on the original board to get a vector of starting places (rows)
    reverse(gridRows.begin(), gridRows.end()); //reverse the original board (bottom becomes top) to find the ending places
    rowEnd = starting_places(gridRows);
    reverse(rowEnd.begin(), rowEnd.end()); //reverse the resulting vector to get the correct ending places

    colStart = starting_places(gridCols); //call starting_places on the rotated board to get a vector of starting places (columns)
    reverse(gridCols.begin(), gridCols.end()); //reverse the rotated board (bottom becomes top) to find the ending places
    colEnd = starting_places(gridCols);
    reverse(colEnd.begin(), colEnd.end()); //reverse the resulting vector to get the correct ending places

    //match up the number of starting places that have valid ending places for both the rows and columns
    rowFolds = countFolds(rowStart, rowEnd);
    colFolds = countFolds(colStart, colEnd);

    //if(rowFolds == 0 && colFolds == 0) //only if the board cannot be folded at all
    //    return 1;
    //else
        return rowFolds * colFolds; //vertical and horizontal folds are independent, so they can be computed separately then multiplied
}

//decide if a row, j, is a valid starting place (or ending place, if the passed board has been reversed)
vector<int> BoardFolding::starting_places(vector<int> &grid)
{
    vector<int> rowStart;

    rowStart.push_back(1); //the first row is always a starting row

    //as described in Dr. Plank's presentation, iterate from j to n, and for each j, iterate w from 1 to j
    for(int j = 1; j < grid.size(); j++)
    {
        for(int w = 1; w <= j; w++)
        {
            if(grid[j - w] != grid[j + w - 1]) //if this row does not have a corresponding mirror across the potential starting place j, j is NOT a starting place
            {    
                rowStart.push_back(0);
                break;
            }
            if(rowStart[j - w] == 1) //check if the row j - w is also a starting row. If it is, then j is too. The first row (j == 0) is always a starting row
            {
                rowStart.push_back(1);
                break;
            }
        }
    }

    rowStart.push_back(0); //the last row is never a starting row

    return rowStart;
}

//count the number of starting places that have valid ending places for a given board
long long BoardFolding::countFolds(vector<int> &start, vector<int> &end)
{
    long long folds = 0;

    //traverse the starting rows vector, and for each starting row (start[i] == 1) check if there is a corresponding ending row in an index higher than it
    for(int i = 0; i < start.size(); i++)
    {
        if(start[i] == 1)
        {
            for(int j = i + 1; j < end.size(); j++) //only need to check indices greater than i
            {
                if(end[j] == 1) //each corresponding ending row is another possible fold
                    folds++;
            }
        }
    }

    return folds;
}

//converts a character into the corresponding integer value as specified in TopCoder SRM 639
int BoardFolding::ctoi(int c)
{
    if(c > 47 && c < 58)
        c -= 48;
    else if(c > 96 && c < 123)
        c -= 87;
    else if(c > 64 && c < 91)
        c -= 29;
    else if(c == 35)
        c = 62;
    else if(c == 64)
        c = 63;

    return c;
}
