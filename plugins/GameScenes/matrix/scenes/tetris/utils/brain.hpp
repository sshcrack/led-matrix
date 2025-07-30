#pragma once

#include <vector>
#include "grid.hpp"
#include "neuralNetwork.hpp"

using namespace std;

class Brain {
    public:
        NeuralNetwork params;
        int score;

        Brain();
        Brain(NeuralNetwork params);
        string getBestMove(Grid grid);
		Brain crossover(Brain partner);
        vector<int> getColumnHeights(vector<vector<int>> grid);
        int getCompletedLines(vector<vector<int>> grid);
        int getAggregateHeight(vector<vector<int>> grid);
        int getBumpiness(vector<vector<int>> grid);
        int getHoles(vector<vector<int>> grid);
};