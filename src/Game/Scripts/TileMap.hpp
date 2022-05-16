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
    GREEN  = 0,
    BLUE   = 1,
    PURPLE = 2,
    ORANGE = 3
};

struct SlimeInfo
{
    float speed;
    int health;
    glm::vec3 size;
    glm::vec4 color;
};

SlimeInfo const GreenSlimeInfo = {
    3.0f,                                   // speed
    3,                                      // health
    glm::vec3(0.8f, 0.8f, 0.8f),            // size
    glm::vec4(0.23f, 0.78f, 0.01f, 0.78f)    // color #c83dc705
};

SlimeInfo const BlueSlimeInfo = {
    5.0f,                                   // speed
    1,                                      // health
    glm::vec3(0.8f, 0.8f, 0.8f),            // size
    glm::vec4(0.18f, 0.70f, 0.91f, 0.78f)    // color #c82fb5e9
};

SlimeInfo const PurpleSlimeInfo = {
    6.0f,                                   // speed
    6,                                      // health
    glm::vec3(0.8f, 0.8f, 0.8f),            // size
    glm::vec4(0.53f, 0.18f, 0.91f, 0.78f)    // color #
};

SlimeInfo const OrangeSlimeInfo = {
    3.0f,                                   // speed
    30,                                      // health
    glm::vec3(1.6f, 1.6f, 1.6f),            // size
    glm::vec4(1.0f, 0.71f, 0.08f, 0.78f)    // color #
};