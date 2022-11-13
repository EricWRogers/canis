#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/External/entt.hpp>

namespace Canis
{
struct QuadPoint {
    glm::vec2 position;
    entt::entity entity;

    /*QuadPoint(glm::vec2 _position, entt::entity _entity) {
        position = _position;
        entity = _entity;
    }*/
};

struct QuadNode
{
    QuadNode *top0 = nullptr;
    QuadNode *top1 = nullptr;
    QuadNode *bottom0 = nullptr;
    QuadNode *bottom1 = nullptr;
    glm::vec2 center = glm::vec2(0.0f);
    float size;
    QuadPoint points[20] = {};
    int pointsCapacity = 20;
    int numOfPoints = 0;

    QuadNode() {

    }

    ~QuadNode() {
        delete top0;
        delete top1;
        delete bottom0;
        delete bottom1;
    }
};

class QuadTree
{
private:
    QuadNode *root;
    unsigned int totalPoints = 0;

    void PointsQuery(const glm::vec2 &center, float radius, QuadNode *node, std::vector<QuadPoint> &results) {
        if (node->top0 == nullptr) {
            for(int i = 0; i < node->numOfPoints; i++) {
                results.push_back(node->points[i]);
            }
            return;
        }
        if (QuadtreeOverlap(center, radius, node->top0)) {
            PointsQuery(center, radius, node->top0, results);
        }
        if (QuadtreeOverlap(center, radius, node->top1)) {
            PointsQuery(center, radius, node->top1, results);
        }
        if (QuadtreeOverlap(center, radius, node->bottom0)) {
            PointsQuery(center, radius, node->bottom0, results);
        }
        if (QuadtreeOverlap(center, radius, node->bottom1)) {
            PointsQuery(center, radius, node->bottom1, results);
        }
    }

    bool QuadtreeOverlap(const glm::vec2 &point, const float &radius, QuadNode *node) {        
        if (glm::distance( point, node->center) < radius + (node->size*1.4f))
            return true;
        
        return false;
    }

    bool OctNodeBoundsContains(const glm::vec2 &point, QuadNode *node) {
        float size = node->size/2.0f;
        if (point.x > node->center.x - size && point.x < node->center.x + size &&
            point.y > node->center.y - size && point.y < node->center.y + size)
            return true;
        
        return false;
    }

    void AddPoint(const glm::vec2 &point, const entt::entity &entity, QuadNode *node) {
        if (node->top0 == nullptr) {
            if (node->pointsCapacity > node->numOfPoints) {
                node->points[node->numOfPoints].position = point;
                node->points[node->numOfPoints].entity = entity;
                node->numOfPoints++;
                totalPoints++;
                return;
            }
            
            // create octnodes
            float size = node->size/2.0f;
            float halfSize = size/2.0f;
            node->top0 = new QuadNode();
            node->top0->size = size;
            node->top0->center = node->center;
            node->top0->center.x -= halfSize;
            node->top0->center.y += halfSize;

            node->top1 = new QuadNode();
            node->top1->size = size;
            node->top1->center = node->center;
            node->top1->center.x += halfSize;
            node->top1->center.y += halfSize;

            node->bottom0 = new QuadNode();
            node->bottom0->size = size;
            node->bottom0->center = node->center;
            node->bottom0->center.x -= halfSize;
            node->bottom0->center.y -= halfSize;

            node->bottom1 = new QuadNode();
            node->bottom1->size = size;
            node->bottom1->center = node->center;
            node->bottom1->center.x += halfSize;
            node->bottom1->center.y -= halfSize;

            // load points into child node
            for(int i = 0; i < node->numOfPoints; i++) {
                if (OctNodeBoundsContains(point, node->top0)) {
                    AddPoint(point, entity, node->top0);
                } else if (OctNodeBoundsContains(point, node->top1)) {
                    AddPoint(point, entity, node->top1);
                } else if (OctNodeBoundsContains(point, node->bottom0)) {
                    AddPoint(point, entity, node->bottom0);
                } else if (OctNodeBoundsContains(point, node->bottom1)) {
                    AddPoint(point, entity, node->bottom1);
                }
            }
        }

        if (OctNodeBoundsContains(point, node->top0)) {
            AddPoint(point, entity, node->top0);
            return;
        } else if (OctNodeBoundsContains(point, node->top1)) {
            AddPoint(point, entity, node->top1);
            return;
        } else if (OctNodeBoundsContains(point, node->bottom0)) {
            AddPoint(point, entity, node->bottom0);
            return;
        } else if (OctNodeBoundsContains(point, node->bottom1)) {
            AddPoint(point, entity, node->bottom1);
            return;
        }
    }

public:
    QuadTree(glm::vec2 center, float size) {
        QuadNode *node = new QuadNode();
        node->size = size;
        node->center = center;

        root = node;
    }

    ~QuadTree() {
        delete root;
    }

    void PrintTotalPoints() {
        Canis::Log("total points : " + std::to_string(totalPoints));
    }

    void AddPoint(glm::vec2 point, entt::entity entity) {
        AddPoint(point, entity, root);
    }

    bool PointsQuery(const glm::vec2 &center, float radius, std::vector<QuadPoint> &results) {
        if (QuadtreeOverlap(center, radius, root)) {
            //Canis::Log("true");
            if (root->top0 == nullptr) {
                for(int i = 0; i < root->numOfPoints; i++) {
                    results.push_back(root->points[i]);
                }
            }
            else
            {
                if (QuadtreeOverlap(center, radius, root->top0)) {
                    PointsQuery(center, radius, root->top0, results);
                }
                if (QuadtreeOverlap(center, radius, root->top1)) {
                    PointsQuery(center, radius, root->top1, results);
                }
                if (QuadtreeOverlap(center, radius, root->bottom0)) {
                    PointsQuery(center, radius, root->bottom0, results);
                }
                if (QuadtreeOverlap(center, radius, root->bottom1)) {
                    PointsQuery(center, radius, root->bottom1, results);
                }
            }
            return true;
        }

        return false;
    }
};
} // end of Canis namespace

