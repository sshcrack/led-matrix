#include <iostream>
#include <fstream>
#include <typeinfo>
#include <raylib.h>
#include "utils/piece.hpp"
#include "utils/grid.hpp"
#include "utils/neuralNetwork.hpp"
#include "utils/brain.hpp"

using namespace std;

int main() {
    
    char *line;
    size_t len = 0;
    int l = 1;
    NeuralNetwork bestParams("data/bestParams.txt");
    FILE *file = fopen("data/record.txt", "r");
    int record;

    if (getline(&line, &len, file) != -1) record = atoi(line);
    fclose(file);
    
    Grid grid;
    Brain brain(bestParams);
    const int windowWidth = 500;
    const int windowHeight = 600;
    int velocity = 1;
    bool start = false;
    bool end = false;
    bool gameOver = false;
    bool fixed = false;
    bool rotated = false;
    bool human = false;
    int textWidth;
    string bestMove = brain.getBestMove(grid);
    int bestRotation = (int)bestMove.back()-48;
    bestMove.pop_back();
    Color colorList[7] = {YELLOW, ORANGE, VIOLET, RED, GREEN, PINK, DARKBLUE};
    int offsets[7][2] = {{35, -30}, {5, 15}, {20, 0}, {-10, 0}, {15, -15}, {20, 0}, {20, 5}};

    InitWindow(windowWidth, windowHeight, "TETRIS AI");
    SetTargetFPS(50);

    while (!WindowShouldClose()) {

        while(!start) {
            BeginDrawing();
            ClearBackground(BLACK);
            textWidth = MeasureText("TETRIS AI", 75);
            DrawText("TETRIS AI", (windowWidth-textWidth)/2, 190, 75, GREEN);
            textWidth = MeasureText("PRESS <SPACE> TO START", 30);
            DrawText("PRESS <SPACE> TO START", (windowWidth-textWidth)/2, 320, 30, WHITE);
            EndDrawing();
            if (IsKeyPressed(KEY_SPACE)) start = true;
            if (WindowShouldClose()) {
                CloseWindow();
                start = true;
                end = true;
            }
        }
        while (!gameOver) {

            velocity = 1;
            end = false;

            if (IsKeyPressed(KEY_C)) {
                if (human) {
                    human = false;
                    SetTargetFPS(50);
                } else {
                    human = true;
                    SetTargetFPS(6);
                }
            }

            if (human) {
                if (IsKeyPressed(KEY_UP) || IsKeyDown(KEY_UP)) grid.rotatePiece();
                if (IsKeyDown(KEY_DOWN)) velocity = 2;
                if (IsKeyPressed(KEY_SPACE)) velocity = 3;
                if (IsKeyPressed(KEY_RIGHT) || IsKeyDown(KEY_RIGHT)) grid.movePiece(1, 9);
                if (IsKeyPressed(KEY_LEFT) || IsKeyDown(KEY_LEFT)) grid.movePiece(-1, 0);

                grid.clearLine();
                grid.gravity(velocity);
                grid.update();

            } else {

               if (!rotated) {
                   for (int i=0; i<bestRotation; i++) grid.rotatePiece();
                   rotated = true;     
               }

               if (bestMove.size() > 0) {
                   if (bestMove[0] == 'r') {
                        grid.movePiece(1,9);
                        bestMove.pop_back();
                   } else {
                        grid.movePiece(-1,0);
                        bestMove.pop_back();
                   }
                }
                   
               if (fixed) {
                   bestMove = brain.getBestMove(grid);
                   bestRotation = (int)bestMove.back()-48;
                   bestMove.pop_back();
                   rotated = false;
                   fixed = false; 
               }

               grid.gravity(1);
               grid.clearLine();
               grid.update();

            }
            
            if (grid.gameOver) {
                while(!end) {
                    BeginDrawing();
                    ClearBackground(BLACK);
                    textWidth = MeasureText("CLEARED LINES", 35);
                    DrawText("CLEARED LINES", (windowWidth-textWidth)/2, 170, 35, WHITE);
                    textWidth = MeasureText(TextFormat("%d", (int)grid.clearedLines), 65);
                    DrawText(TextFormat("%d", (int)grid.clearedLines), (windowWidth-textWidth)/2, 230, 65, WHITE);
                    textWidth = MeasureText(TextFormat("RECORD: %d", record), 30);
                    DrawText(TextFormat("RECORD: %d", record), (windowWidth-textWidth)/2, 320, 30, WHITE);
                    textWidth = MeasureText("<SPACE>: NEW GAME", 30);
                    DrawText("<SPACE>: NEW GAME", (windowWidth-textWidth)/2, 400, 30, WHITE);
                    textWidth = MeasureText("<Q> QUIT", 30);
                    DrawText("<Q> QUIT", (windowWidth-textWidth)/2, 450, 30, WHITE);
                    EndDrawing();
                    if (WindowShouldClose() || IsKeyPressed(KEY_Q)) {
                        end = true;
                        gameOver = true;
                    } else if (IsKeyPressed(KEY_SPACE)) {
                        end = true;
                        grid = Grid();
                    }
                }
            }
            

            if (grid.piece.fixed) {
                grid.piece.newShape();
                grid.piece.newNext();
                fixed = true;
            }

            BeginDrawing();
            ClearBackground(BLACK);
            
            for (int j=4; j<24; j++) {
                for (int i=0; i<10; i++)  {
                    if (grid.matrix[j][i] == 1) { 
                        DrawRectangle(i*30, (j-4)*30, 30, 30, colorList[grid.piece.n]);
                    } else if (grid.matrix[j][i] != 0) { 
                        DrawRectangle(i*30, (j-4)*30, 30, 30, colorList[grid.matrix[j][i]-2]);
                    }                        
                }
            }
            
            DrawLine(300, 0, 300, 600, WHITE);
            DrawLine(0, 0, 500, 0, WHITE);
            DrawLine(0, 599, 500, 599, WHITE);
            DrawLine(1, 0, 1, 600, WHITE);
            DrawLine(500, 0, 500, 600, WHITE);
            
            textWidth = MeasureText("PLAYING", 30);
            DrawText("PLAYING", (200-textWidth)/2+300, 100, 30, WHITE);
            if (human) {
                textWidth = MeasureText("HUMAN", 30);
                DrawText("HUMAN", (200-textWidth)/2+300, 150, 30, WHITE);
            } else {
                textWidth = MeasureText("AI", 30);
                DrawText("AI", (200-textWidth)/2+300, 150, 30, WHITE);
            }

            textWidth = MeasureText("LINES", 30);
            DrawText("LINES", (200-textWidth)/2+300, 260, 30, WHITE);
            textWidth = MeasureText(TextFormat("%d", (int)grid.clearedLines), 30);
            DrawText(TextFormat("%d", (int)grid.clearedLines), (200-textWidth)/2+300, 310, 30, WHITE);
            textWidth = MeasureText("NEXT", 30);
            DrawText("NEXT", (200-textWidth)/2+300, 410, 30, WHITE);

            for (int i=0; i<16; i++) {
                if (grid.piece.shapeList[grid.piece.next][0][i] == 'o') { 
                    double x = offsets[grid.piece.next][0]+335+(i%4)*30;
                    double y = offsets[grid.piece.next][1]+450+(i/4)*30;
                    DrawRectangle(x, y, 30, 30, colorList[grid.piece.next]);
                }
            }
            
            if (grid.clearedLines > record) {
                ofstream file("data/record.txt");
                if (file.is_open()) {
                    record = grid.clearedLines;
                    file << record;
                    file.close();
                }
            }
            EndDrawing();
            if (WindowShouldClose()) gameOver = true;
        }
        CloseWindow();
    }
    return 0;
}
