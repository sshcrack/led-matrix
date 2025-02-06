#include <vector>
#include "brain.hpp"

#ifndef POPULATION_H
#define POPULATION_H

class Population {
    public:
        vector<Brain> agents;
        int numAgents;
        
        Population();
        vector<float> normalize(vector<float> params);
        void generateNewPopulation();
};

#endif