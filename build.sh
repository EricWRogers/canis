#!/bin/bash

g++ -pipe src/*.cpp src/Canis/*.cpp -std=gnu++11 -lSDL2 -lGL -lGLEW -pthread -llua5.3 -o bin/run