#include "tr_noise_cplus.hpp"
#include <cstdlib>
#include <cmath>

constexpr int NOISE_SIZE = 256;
constexpr int NOISE_MASK = (NOISE_SIZE - 1);

static float s_noise_table[NOISE_SIZE];
static int s_noise_perm[NOISE_SIZE];

static float LERP(float a, float b, float t)
{
    return a + t * (b - a);
}

static int VAL(int a)
{
    return s_noise_perm[a & NOISE_MASK];
}

static int INDEX(int x, int y, int z, int t)
{
    return VAL(x + VAL(y + VAL(z + VAL(t))));
}

float GetNoiseValue_plus(int x, int y, int z, int t)
{
    int index = INDEX(x, y, z, t);
    return s_noise_table[index];
}

void NoiseInit_plus()
{
    for (int i = 0; i < NOISE_SIZE; i++)
    {
        s_noise_table[i] = static_cast<float>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0 - 1.0));
        s_noise_perm[i] = static_cast<unsigned char>(static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 255);
    }
}

float NoiseGet4f_plus(float x, float y, float z, double t)
{
    int ix, iy, iz, it;
    float fx, fy, fz, ft;
    float front[4];
    float back[4];
    float fvalue, bvalue, value[2], finalvalue;

    ix = static_cast<int>(std::floor(x));
    fx = x - ix;
    iy = static_cast<int>(std::floor(y));
    fy = y - iy;
    iz = static_cast<int>(std::floor(z));
    fz = z - iz;
    it = static_cast<int>(std::floor(t));
    ft = t - it;

    for (int i = 0; i < 2; i++)
    {
        front[0] = GetNoiseValue_plus(ix, iy, iz, it + i);
        front[1] = GetNoiseValue_plus(ix + 1, iy, iz, it + i);
        front[2] = GetNoiseValue_plus(ix, iy + 1, iz, it + i);
        front[3] = GetNoiseValue_plus(ix + 1, iy + 1, iz, it + i);

        back[0] = GetNoiseValue_plus(ix, iy, iz + 1, it + i);
        back[1] = GetNoiseValue_plus(ix + 1, iy, iz + 1, it + i);
        back[2] = GetNoiseValue_plus(ix, iy + 1, iz + 1, it + i);
        back[3] = GetNoiseValue_plus(ix + 1, iy + 1, iz + 1, it + i);

        fvalue = LERP(LERP(front[0], front[1], fx), LERP(front[2], front[3], fx), fy);
        bvalue = LERP(LERP(back[0], back[1], fx), LERP(back[2], back[3], fx), fy);

        value[i] = LERP(fvalue, bvalue, fz);
    }

    finalvalue = LERP(value[0], value[1], ft);

    return finalvalue;
}
