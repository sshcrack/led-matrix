#include <cmath>
#include <random>
#include <ostream>
#include <fstream>
#include <sstream>
#include "neuralNetwork.hpp"

const char *BEST_PARAMS = R"#(-0.212454
0.440242
-0.370061
-0.0793521
0.000290688
0.397267
-0.824796
0.532017
-0.283169
0.228108
0.657395
-0.166412
-0.85031
-0.231146
-0.81578
-0.367808
-0.287443
0.623546
-0.760371
18777)#";

random_device rd2;
uniform_int_distribution<int> d2(0, 10000000);

NeuralNetwork::NeuralNetwork() {
    int i, j;

    for (i = 0; i < 3; i++) {
        vector<float> tmp;
        for (j = 0; j < 4; j++) {
            tmp.push_back(getRandomParam());
        }
        layer1.push_back(tmp);
        biases1.push_back(getRandomParam());
    }

    for (i = 0; i < 3; i++) {
        layer2.push_back(getRandomParam());
    }
    bias2 = getRandomParam();
}

NeuralNetwork::NeuralNetwork(const char *params) {
    loadParamsFromString(params);
}

float NeuralNetwork::getRandomParam() {
    // returns random float between -1.0000000 and 1.0000000
    return (float) d2(rd2) / pow(10, 7) * (2 * (d2(rd2) % 2) - 1);
}

float NeuralNetwork::reLU(float node) {
    if (node > 0.0) return node;
    return 0.0;
}

float NeuralNetwork::forward(vector<int> inputs) {
    float tmp, out = 0.0;

    for (int i = 0; i < 3; i++) {
        tmp = 0.0;

        for (int j = 0; j < 4; j++) {
            tmp += inputs[j] * layer1[i][j];
        }

        out += reLU(tmp + biases1[i]) * layer2[i];
    }

    return out + bias2;
}

void NeuralNetwork::loadParamsFromString(const std::string &params) {
    std::istringstream iss(params);
    std::string line;
    int i, j;

    for (i = 0; i < 3; i++) {
        vector<float> tmp;
        for (j = 0; j < 4; j++) {
            std::getline(iss, line);
            tmp.push_back(std::stof(line));
        }
        layer1.push_back(tmp);
        std::getline(iss, line);
        biases1.push_back(std::stof(line));
    }
    for (i = 0; i < 3; i++) {
        std::getline(iss, line);
        layer2.push_back(std::stof(line));
    }
    std::getline(iss, line);
    bias2 = std::stof(line);
}
