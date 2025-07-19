#include "grid.hpp"

random_device rd1;
uniform_int_distribution<int> d1(0,7);


vector<int> removeElement(vector<int> v, int elem) {
    int it = 0;
    while(v[it] != elem) it++;
    swap(v[it], v.back());
    v.pop_back();
    return v;
}

Piece::Piece() {
    pieceBag = {0, 1, 2, 3, 4, 5, 6};
    int s1 = pieceBag[d1(rd1)%pieceBag.size()];
    pieceBag = removeElement(pieceBag, s1);
    int s2 = pieceBag[d1(rd1)%pieceBag.size()];
    pieceBag = removeElement(pieceBag, s2);
    n = s1;
    next = s2;
    m = 0;
    position[0] = 0;
    position[1] = 3;
    fixed = false;
    vector<string> shape1 = {"        oo  oo  ", "        oo  oo  ", "        oo  oo  ", "        oo  oo  "}; //O
    vector<string> shape2 = {"    oooo        ", " o   o   o   o  ", "    oooo        ", " o   o   o   o  "}; //I
    vector<string> shape3 = {"    oo   oo     ", "  o  oo  o      ", "    oo   oo     ", "  o  oo  o      "}; //Z
    vector<string> shape4 = {"      oo oo     ", " o   oo   o     ", "      oo oo     ", " o   oo   o     "}; //S
    vector<string> shape5 = {"     o   o   oo ", "    ooo o       ", "     oo   o   o ", "      o ooo     "}; //L
    vector<string> shape6 = {"    o   ooo     ", "     oo  o   o  ", "    ooo   o     ", "      o   o  oo "}; //J
    vector<string> shape7 = {"    ooo  o      ", " o  oo   o      ", " o  ooo         ", " o   oo  o      "}; //T
    shapeList = {shape1, shape2, shape3, shape4, shape5, shape6, shape7};
    shapeRotations = {1,2,2,2,4,4,4};
    shape = shapeList[n][m];
}

void Piece::newShape() {
    n = next;
    m = 0;
    shape = shapeList[n][m];
    position[0] = 0;
    position[1] = 3;
    fixed = false;
}

void Piece::newNext() {
    int s = pieceBag[d1(rd1)%pieceBag.size()];
    pieceBag = removeElement(pieceBag, s);
    next = s;
    if (pieceBag.size() == 0) pieceBag = {0, 1, 2, 3, 4, 5, 6};
}
