#!/bin/bash

g++ -pipe src/*.c* src/Canis/*.c* -std=gnu++11 -lSDL2 -lGL -lGLEW -pthread -llua5.3 -o bin/run