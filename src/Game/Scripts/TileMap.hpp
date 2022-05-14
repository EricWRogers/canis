#pragma once
#include <glm/glm.hpp>

enum BlockTypes
{
	NONE         = 0,
	GRASS        = 1,
	DIRT         = 2,
	PORTAL       = 3,
	CASTLE       = 4,
	SPIKETOWER   = 5,
    SPIKE        = 6,
    GEMMINETOWER = 7,
    FIRETOWER    = 8,
    ICETOWER     = 9
};

glm::uint8 static titleMap[2][9][9] = {
    {
        { 1,1,2,1,1,1,1,1,1 },
        { 1,1,2,1,1,1,1,1,1 },
        { 1,1,2,1,1,1,1,1,1 },
        { 1,1,2,1,2,2,2,1,1 },
        { 1,1,2,1,2,1,2,1,1 },
        { 1,1,2,2,2,1,2,1,1 },
        { 1,1,1,1,1,1,2,1,1 },
        { 1,1,1,1,1,1,2,1,1 },
        { 1,1,1,1,1,1,2,1,1 },
    },
    {
        { 0,0,0,0,0,0,0,0,0 },
        { 0,0,3,0,0,0,0,7,0 },
        { 0,0,0,0,0,0,0,0,0 },
        { 0,9,0,0,0,0,0,0,0 },
        { 0,0,0,5,0,8,0,0,0 },
        { 0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,4,0,0 },
        { 0,0,0,0,0,0,0,0,0 },
    }
};

enum SlimeStatus
{
    NO    = 0,
    CHILL   = 1,
    BURNING = 2,
    POISON  = 4
};

enum SlimeType
{
    GREEN = 0,
    BLUE  = 1
};

struct GreenSlime
{
    float speed = 3.0f;
    int health = 10;
};