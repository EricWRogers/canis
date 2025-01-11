#include <Canis/DataStructure/PerlinNoise2D.hpp>
#include <Canis/Math.hpp>

#include <numeric>

namespace Canis
{
    PerlinNoise2D::PerlinNoise2D(int seed)
    {
        m_permutation.resize(512);

        // Generate permutation vector based on seed
        std::vector<int> perm(256);
        std::iota(perm.begin(), perm.end(), 0);

        std::default_random_engine engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);

        // Duplicate the permutation vector
        for (int i = 0; i < 256; ++i)
        {
            m_permutation[i] = perm[i];
            m_permutation[i + 256] = perm[i];
        }
    }

    void PerlinNoise2D::GenerateNoise(int _width, int _height, float _scale, int _octaves, float _persistence) {
    m_noise.resize(_height, std::vector<float>(_width, 0.0f));

    float maxAmplitude = 0.0f;
    float amplitude = 1.0f;

    for (int octave = 0; octave < _octaves; ++octave) {
        float frequency = std::pow(2.0f, octave);
        amplitude *= _persistence;
        maxAmplitude += amplitude;

        for (int y = 0; y < _height; ++y) {
            for (int x = 0; x < _width; ++x) {
                float nx = x / (_scale * frequency);
                float ny = y / (_scale * frequency);
                m_noise[y][x] += Perlin(nx, ny) * amplitude;
            }
        }
    }

    // Normalize the noise to the range [0, 1]
    for (auto& row : m_noise) {
        for (float& value : row) {
            value = (value + maxAmplitude) / (2.0f * maxAmplitude);
        }
    }
}

    float PerlinNoise2D::Get(int _x, int _y)
    {
        if (_y >= m_noise.size() || _y < 0)
            return 0.0f;

        if (_x >= m_noise[_y].size() || _x < 0)
            return 0.0f;

        return m_noise[_y][_x];
    }

    float PerlinNoise2D::Fade(float _t)
    {
        return _t * _t * _t * (_t * (_t * 6 - 15) + 10);
    }

    float PerlinNoise2D::Gradient(int _hash, float _x, float _y)
    {
        int h = _hash & 3; // Take the first 2 bits of the hash
        float u = h < 2 ? _x : _y;
        float v = h < 2 ? _y : _x;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    float PerlinNoise2D::Perlin(float _x, float _y)
    {
        int X = static_cast<int>(std::floor(_x)) & 255;
        int Y = static_cast<int>(std::floor(_y)) & 255;

        _x -= std::floor(_x);
        _y -= std::floor(_y);

        float u = Fade(_x);
        float v = Fade(_y);

        int aa = m_permutation[m_permutation[X] + Y];
        int ab = m_permutation[m_permutation[X] + Y + 1];
        int ba = m_permutation[m_permutation[X + 1] + Y];
        int bb = m_permutation[m_permutation[X + 1] + Y + 1];

        return Lerp(v,
                    Lerp(u, Gradient(aa, _x, _y), Gradient(ba, _x - 1, _y)),
                    Lerp(u, Gradient(ab, _x, _y - 1), Gradient(bb, _x - 1, _y - 1)));
    }
}