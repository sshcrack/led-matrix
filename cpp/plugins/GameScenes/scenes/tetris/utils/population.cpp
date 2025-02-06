#include <iostream>
#include <algorithm>
#include "population.hpp"

#define NUM_AGENTS 300

random_device rd3;
uniform_int_distribution<int> d3(0,NUM_AGENTS);


Population::Population() {
    numAgents = NUM_AGENTS;
    for (int a=0; a<numAgents; a++) {
		NeuralNetwork params;
        agents.push_back(Brain(params));
    }
}

void Population::generateNewPopulation() {
    vector<Brain> newOffsprings;
    int nWorstAgents = numAgents*0.3;
    int nSelection = numAgents*0.1;

    for (int i=0; i<nWorstAgents; i++) {
        vector<Brain> randomSelection;

        for (int j=0; j<nSelection; j++) {
            randomSelection.push_back(agents[d3(rd3)%numAgents]);
        }
        

        sort(randomSelection.begin(), randomSelection.end(),
        [](Brain & b1, Brain & b2 ) -> bool {
            return b1.score < b2.score;
        });

        Brain newBrain = randomSelection[nSelection-1].crossover(randomSelection[nSelection-2]);
        newOffsprings.push_back(newBrain);
    }
    sort(this->agents.begin(), this->agents.end(),
    [](Brain & b1, Brain & b2 ) -> bool {
        return b1.score < b2.score;
    });
     
    int totalFitness = 0;
    for (int a=0; a<numAgents; a++) totalFitness += agents[a].score;
    
    cout << "#############################################################" << endl << endl;
    cout << "average fitness: " << (float)totalFitness/numAgents << endl;
    cout << "highest fitness: " << agents.back().score << endl;
    cout << endl<< "#############################################################" << endl << endl << endl;
    
    for (int i=0; i<nWorstAgents; i++) agents[i] = newOffsprings[i];
}
