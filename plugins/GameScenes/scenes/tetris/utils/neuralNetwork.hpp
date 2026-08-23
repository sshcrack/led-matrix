#pragma once

#include <vector>
#include <string>

using namespace std;

extern const char *BEST_PARAMS;

class NeuralNetwork {
private:
    void loadParamsFromString(const std::string& params);

public:
    // 4x3x1 neural network
    vector<vector<float>> layer1;
    vector<float> biases1;
    vector<float> layer2;
    float bias2;

    NeuralNetwork();
    explicit NeuralNetwork(const char *params);

    float getRandomParam();

    float reLU(float node);

    float forward(vector<int> inputs);
};