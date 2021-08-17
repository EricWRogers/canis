#!/bin/bash

g++ -pipe src/*.cpp -std=gnu++11 -lSDL2 -lGL -lGLEW -pthread -llua -o bin/run