#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/External/entt.hpp>

namespace Canis
{
struct QuadPoint {
    glm::vec2 position;
    entt::entity entity;
    glm::vec2 velocity; 
};

struct QuadNode
{
    unsigned short top0 = 0;
    unsigned short top1 = 0;
    unsigned short bottom0 = 0;
    unsigned short bottom1 = 0;
    glm::vec2 center = glm::vec2(0.0f);
    float size;
    QuadPoint points[100] = {};
    unsigned short pointsCapacity = 100;
    unsigned short numOfPoints = 0;
};

class QuadTree
{
private:
    std::vector<QuadNode> quadNodes = std::vector<QuadNode>(30000);
    QuadNode quadNodeDefault = {};
    unsigned int rootIndex = 0;
    unsigned int totalPoints = 0;
    unsigned int totalNodes = 0;
    unsigned int currentNumberOfNodes = 0;

    void PointsQuery(const glm::vec2 &center, float radius, unsigned short nodeIndex, std::vector<QuadPoint> &results) {
        if (quadNodes[nodeIndex].top0 == 0) {
            for(int i = 0; i < quadNodes[nodeIndex].numOfPoints; i++) {
                results.push_back(quadNodes[nodeIndex].points[i]);
            }
            return;
        }
        if (QuadtreeOverlap(center, radius, quadNodes[nodeIndex].top0)) {
            PointsQuery(center, radius, quadNodes[nodeIndex].top0, results);
        }
        if (QuadtreeOverlap(center, radius, quadNodes[nodeIndex].top1)) {
            PointsQuery(center, radius, quadNodes[nodeIndex].top1, results);
        }
        if (QuadtreeOverlap(center, radius, quadNodes[nodeIndex].bottom0)) {
            PointsQuery(center, radius, quadNodes[nodeIndex].bottom0, results);
        }
        if (QuadtreeOverlap(center, radius, quadNodes[nodeIndex].bottom1)) {
            PointsQuery(center, radius, quadNodes[nodeIndex].bottom1, results);
        }
    }

    bool QuadtreeOverlap(const glm::vec2 &point, const float &radius, unsigned short nodeIndex) {        
        if (glm::distance( point, quadNodes[nodeIndex].center) < radius + (quadNodes[nodeIndex].size*1.4f))
            return true;
        
        return false;
    }

    bool OctNodeBoundsContains(const glm::vec2 &point, unsigned short nodeIndex) {
        float size = quadNodes[nodeIndex].size/2.0f;
        if (point.x > quadNodes[nodeIndex].center.x - size && point.x < quadNodes[nodeIndex].center.x + size &&
            point.y > quadNodes[nodeIndex].center.y - size && point.y < quadNodes[nodeIndex].center.y + size)
            return true;
        
        return false;
    }

    void AddPoint(const glm::vec2 &point, const entt::entity &entity, const glm::vec2 &vel, unsigned short nodeIndex) {
        if (quadNodes[nodeIndex].top0 == 0) {
            if (quadNodes[nodeIndex].pointsCapacity > quadNodes[nodeIndex].numOfPoints) {
                quadNodes[nodeIndex].points[quadNodes[nodeIndex].numOfPoints].position = point;
                quadNodes[nodeIndex].points[quadNodes[nodeIndex].numOfPoints].entity = entity;
                quadNodes[nodeIndex].points[quadNodes[nodeIndex].numOfPoints].velocity = vel;
                quadNodes[nodeIndex].numOfPoints++;
                totalPoints++;
                return;
            }
            
            // create octnodes
            float size = quadNodes[nodeIndex].size/2.0f;
            float halfSize = size/2.0f;

            currentNumberOfNodes++;
            if (quadNodes.size() <= currentNumberOfNodes) {
                quadNodes.push_back(quadNodeDefault);
            }
            quadNodes[nodeIndex].top0 = currentNumberOfNodes - 1;
            quadNodes[quadNodes[nodeIndex].top0].size = size;
            quadNodes[quadNodes[nodeIndex].top0].center = quadNodes[nodeIndex].center;
            quadNodes[quadNodes[nodeIndex].top0].center.x -= halfSize;
            quadNodes[quadNodes[nodeIndex].top0].center.y += halfSize;
            quadNodes[quadNodes[nodeIndex].top0].top0 = 0;
            quadNodes[quadNodes[nodeIndex].top0].top1 = 0;
            quadNodes[quadNodes[nodeIndex].top0].bottom0 = 0;
            quadNodes[quadNodes[nodeIndex].top0].bottom1 = 0;
            quadNodes[quadNodes[nodeIndex].top0].numOfPoints = 0;

            currentNumberOfNodes++;
            if (quadNodes.size() <= currentNumberOfNodes) {
                quadNodes.push_back(quadNodeDefault);
            }
            quadNodes[nodeIndex].top1 = currentNumberOfNodes - 1;
            quadNodes[quadNodes[nodeIndex].top1].size = size;
            quadNodes[quadNodes[nodeIndex].top1].center = quadNodes[nodeIndex].center;
            quadNodes[quadNodes[nodeIndex].top1].center.x += halfSize;
            quadNodes[quadNodes[nodeIndex].top1].center.y += halfSize;
            quadNodes[quadNodes[nodeIndex].top1].top0 = 0;
            quadNodes[quadNodes[nodeIndex].top1].top1 = 0;
            quadNodes[quadNodes[nodeIndex].top1].bottom0 = 0;
            quadNodes[quadNodes[nodeIndex].top1].bottom1 = 0;
            quadNodes[quadNodes[nodeIndex].top1].numOfPoints = 0;

            currentNumberOfNodes++;
            if (quadNodes.size() <= currentNumberOfNodes) {
                quadNodes.push_back(quadNodeDefault);
            }
            quadNodes[nodeIndex].bottom0 = currentNumberOfNodes - 1;
            quadNodes[quadNodes[nodeIndex].bottom0].size = size;
            quadNodes[quadNodes[nodeIndex].bottom0].center = quadNodes[nodeIndex].center;
            quadNodes[quadNodes[nodeIndex].bottom0].center.x -= halfSize;
            quadNodes[quadNodes[nodeIndex].bottom0].center.y -= halfSize;
            quadNodes[quadNodes[nodeIndex].bottom0].top0 = 0;
            quadNodes[quadNodes[nodeIndex].bottom0].top1 = 0;
            quadNodes[quadNodes[nodeIndex].bottom0].bottom0 = 0;
            quadNodes[quadNodes[nodeIndex].bottom0].bottom1 = 0;
            quadNodes[quadNodes[nodeIndex].bottom0].numOfPoints = 0;

            currentNumberOfNodes++;
            if (quadNodes.size() <= currentNumberOfNodes) {
                quadNodes.push_back(quadNodeDefault);
            }
            quadNodes[nodeIndex].bottom1 = currentNumberOfNodes - 1;
            quadNodes[quadNodes[nodeIndex].bottom1].size = size;
            quadNodes[quadNodes[nodeIndex].bottom1].center = quadNodes[nodeIndex].center;
            quadNodes[quadNodes[nodeIndex].bottom1].center.x += halfSize;
            quadNodes[quadNodes[nodeIndex].bottom1].center.y -= halfSize;
            quadNodes[quadNodes[nodeIndex].bottom1].top0 = 0;
            quadNodes[quadNodes[nodeIndex].bottom1].top1 = 0;
            quadNodes[quadNodes[nodeIndex].bottom1].bottom0 = 0;
            quadNodes[quadNodes[nodeIndex].bottom1].bottom1 = 0;
            quadNodes[quadNodes[nodeIndex].bottom1].numOfPoints = 0;

            // load points into child node
            for(int i = 0; i < quadNodes[nodeIndex].numOfPoints; i++) {
                if (OctNodeBoundsContains(point, quadNodes[nodeIndex].top0)) {
                    AddPoint(point, entity, vel, quadNodes[nodeIndex].top0);
                } else if (OctNodeBoundsContains(point, quadNodes[nodeIndex].top1)) {
                    AddPoint(point, entity, vel, quadNodes[nodeIndex].top1);
                } else if (OctNodeBoundsContains(point, quadNodes[nodeIndex].bottom0)) {
                    AddPoint(point, entity, vel, quadNodes[nodeIndex].bottom0);
                } else if (OctNodeBoundsContains(point, quadNodes[nodeIndex].bottom1)) {
                    AddPoint(point, entity, vel, quadNodes[nodeIndex].bottom1);
                }
            }
        }

        if (OctNodeBoundsContains(point, quadNodes[nodeIndex].top0)) {
            AddPoint(point, entity, vel, quadNodes[nodeIndex].top0);
            return;
        } else if (OctNodeBoundsContains(point, quadNodes[nodeIndex].top1)) {
            AddPoint(point, entity, vel, quadNodes[nodeIndex].top1);
            return;
        } else if (OctNodeBoundsContains(point, quadNodes[nodeIndex].bottom0)) {
            AddPoint(point, entity, vel, quadNodes[nodeIndex].bottom0);
            return;
        } else if (OctNodeBoundsContains(point, quadNodes[nodeIndex].bottom1)) {
            AddPoint(point, entity, vel, quadNodes[nodeIndex].bottom1);
            return;
        }
    }

public:
    QuadTree(glm::vec2 center, float size) {
        rootIndex = 0;
        totalNodes = 0;
        quadNodes[rootIndex].size = size;
        quadNodes[rootIndex].center = center;
    }

    ~QuadTree() {
    }

    void Reset() {
        currentNumberOfNodes = 1;
        float size = quadNodes[rootIndex].size;
        glm::vec2 center = quadNodes[rootIndex].center;
        rootIndex = 0;
        quadNodes[rootIndex].size = size;
        quadNodes[rootIndex].center = center;
        quadNodes[rootIndex].top0 = 0;
        quadNodes[rootIndex].top1 = 0;
        quadNodes[rootIndex].bottom0 = 0;
        quadNodes[rootIndex].bottom1 = 0;
        quadNodes[rootIndex].numOfPoints = 0;
    }

    void Print() {
        Canis::Log("quad nodes : " + std::to_string(quadNodes.size()));
    }

    void AddPoint(glm::vec2 point, entt::entity entity, glm::vec2 vel) {
        AddPoint(point, entity, vel, rootIndex);
    }

    bool PointsQuery(const glm::vec2 &center, float radius, std::vector<QuadPoint> &results) {
        if (QuadtreeOverlap(center, radius, rootIndex)) {
            if (quadNodes[rootIndex].top0 == 0) {
                for(int i = 0; i < quadNodes[rootIndex].numOfPoints; i++) {
                    results.push_back(quadNodes[rootIndex].points[i]);
                }
            }
            else
            {
                if (QuadtreeOverlap(center, radius, quadNodes[rootIndex].top0)) {
                    PointsQuery(center, radius, quadNodes[rootIndex].top0, results);
                }
                if (QuadtreeOverlap(center, radius, quadNodes[rootIndex].top1)) {
                    PointsQuery(center, radius, quadNodes[rootIndex].top1, results);
                }
                if (QuadtreeOverlap(center, radius, quadNodes[rootIndex].bottom0)) {
                    PointsQuery(center, radius, quadNodes[rootIndex].bottom0, results);
                }
                if (QuadtreeOverlap(center, radius, quadNodes[rootIndex].bottom1)) {
                    PointsQuery(center, radius, quadNodes[rootIndex].bottom1, results);
                }
            }
            return true;
        }

        return false;
    }
};
} // end of Canis namespace

