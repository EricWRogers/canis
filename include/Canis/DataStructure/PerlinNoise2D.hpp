#pragma once
#include <vector>
#include <cmath>
#include <random>

namespace Canis
{
class PerlinNoise2D {
public:
    PerlinNoise2D(int seed = 0);

    void GenerateNoise(int _width, int _height, float _scale, int _octaves = 1, float _persistence = 0.5f);
    float Get(int _x, int _y);

private:
    float Fade(float _t);
    float Gradient(int _hash, float _x, float _y);
    float Perlin(float _x, float _y);

    std::vector<int> m_permutation;
    std::vector<std::vector<float>> m_noise;
};
}