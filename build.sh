#!/bin/bash

g++ -pipe src/*.cpp src/Canis/*.cpp -std=gnu++11 -lSDL2 -lGL -lGLEW -pthread -llua -o bin/run