#include <cmath>
#include <string>
#include <cstdlib>
#include <bits/stdc++.h>
#include "brain.hpp"


Brain::Brain() {
    score = 0;
}

Brain::Brain(NeuralNetwork params) {
    this->params = params;
    score = 0;
}

string Brain::getBestMove(Grid grid) {
    string bestMove, bestRotation;
    float maxScore = -pow(10.0, 5);
    int nRotations = grid.piece.shapeRotations[grid.piece.n];
    Grid startingGrid = grid;
    vector<string> possibleMoves = {"llll", "lll", "ll", "l", "rrrrr", "rrrr", "rrr", "rr", "r", ""}; 
    
    for (int r=0; r<nRotations; r++) {
        for(int m=0; m<10; m++) {

            grid = startingGrid;

            for (int i=0; i<r; i++) grid.rotatePiece();

            if (possibleMoves[m][0] == 'l') {
                for (int i=0; i<possibleMoves[m].size(); i++) grid.movePiece(-1,0);
            } else if (possibleMoves[m][0] == 'r') {
                for (int i=0; i<possibleMoves[m].size(); i++) grid.movePiece(1,9);
            }

            grid.gravity(3);
            vector<int> heuristics = {getAggregateHeight(grid.matrix), getCompletedLines(grid.matrix),
                                      getHoles(grid.matrix), getBumpiness(grid.matrix)};


            float score = params.forward(heuristics);

            grid.piece.newShape();
            
            int nRotationsNext = grid.piece.shapeRotations[grid.piece.n];
            Grid nextStartingGrid = grid;
            
            for (int rNext=0; rNext<nRotationsNext; rNext++) {
                for (int mNext=0; mNext<10; mNext++) {

                    grid = nextStartingGrid;
                    for (int i=0; i<rNext; i++) grid.rotatePiece();

                    if (possibleMoves[mNext][0] == 'l') {
                        for (int i=0; i<possibleMoves[mNext].size(); i++) grid.movePiece(-1,0);
                    } else if (possibleMoves[mNext][0] == 'r') {
                        for (int i=0; i<possibleMoves[mNext].size(); i++) grid.movePiece(1,9);
                    }

                    vector<int> heuristics = {getAggregateHeight(grid.matrix), getCompletedLines(grid.matrix),
                                              getHoles(grid.matrix), getBumpiness(grid.matrix)};

					float score2 = score + params.forward(heuristics);
                    
                    if (score2 >= maxScore) {
                        maxScore = score2;
                        bestMove = possibleMoves[m];
                        bestRotation = to_string(r);
                    }
                }
            }
        }
    }
    return bestMove + bestRotation;
}

Brain Brain::crossover(Brain partner) {
	Brain offspring;
    NeuralNetwork p1 = this->params;
    NeuralNetwork p2 = partner.params;
    float f1 = this->score;
    float f2 = partner.score;
    int i, j;

	for(i=0; i<3; i++) {
		for (j=0; j<4; j++) {
			offspring.params.layer1[i][j] = (f1*p1.layer1[i][j]+f2*p2.layer1[i][j])/(f1+f2+pow(10, -10));
		}
		offspring.params.biases1[i] = (f1*p1.biases1[i]+f2*p2.biases1[i])/(f1+f2+pow(10, -10));
	}

	for (i=0; i<3; i++) {
		offspring.params.layer2[i] = (f1*p1.layer2[i]+f2*p2.layer2[i])/(f1+f2+pow(10, -10));
	}
	offspring.params.bias2 = (f1*p1.bias2+f2*p2.bias2)/(f1+f2+pow(10, -10));

	return offspring;
}

vector<int> Brain::getColumnHeights(vector<vector<int>> grid) {
    vector<int> columnHeights;
    for (int col=0; col<10; col++) {
        int height = 20;
        for (int row=4; row<24; row++) {
            if (grid[row][col] != 0 && grid[row][col] != 1) break;
            else height--;
        }
        columnHeights.push_back(height);
    }
    return columnHeights;
}

int Brain::getCompletedLines(vector<vector<int>> grid) {
    int completedLines = 0;
    for (int l=0; l<24; l++) {
        if (find(grid[l].begin(), grid[l].end(), 0) == grid[l].end()) {
            completedLines++;
        }
    }
    return completedLines;
}

int Brain::getAggregateHeight(vector<vector<int>> grid) {
    int sum = 0;
    vector<int> heights = getColumnHeights(grid);
    for(int i=0; i<10; i++) sum += heights[i];
    return sum;
}

int Brain::getBumpiness(vector<vector<int>> grid) {
    vector<int> heights = getColumnHeights(grid);
    int bumpiness = 0;
    for(int i=0; i<9; i++) bumpiness += abs(heights[i]-heights[i+1]);
    return bumpiness;
}

int Brain::getHoles(vector<vector<int>> grid) {
    int holes = 0;

    for(int row=23; row>3; row--) {
        vector<int> zeros = {0,0,0,0,0,0,0,0,0,0};
        if (grid[row] == zeros) return holes;
        
        for (int col=9; col>=0; col--) {
            bool covered = false;
            if (grid[row][col] == 0) {
                for (int r=row-1; r>3; r--) {
                    if (grid[r][col] != 0) {
                        covered = true;
                        break;
                    }
                }
                if (covered) holes++;
            }
        }
    }
    return holes;
}
