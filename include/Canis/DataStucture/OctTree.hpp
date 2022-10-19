#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace Canis
{
struct OctNode
{
    OctNode *top0 = nullptr;
    OctNode *top1 = nullptr;
    OctNode *top2 = nullptr;
    OctNode *top3 = nullptr;
    OctNode *bottom0 = nullptr;
    OctNode *bottom1 = nullptr;
    OctNode *bottom2 = nullptr;
    OctNode *bottom3 = nullptr;
    glm::vec3 center = glm::vec3(0.0f);
    float size;
    glm::vec3 points[10] = {};
    int pointsCapacity = 10;
    int numOfPoints = 0;

    ~OctNode() {
        delete top0;
        delete top1;
        delete top2;
        delete top3;
        delete bottom0;
        delete bottom1;
        delete bottom2;
        delete bottom3;
    }
};

class OctTree
{
private:
    OctNode *m_root;

    void PointsQuery(glm::vec3 center, float radius, OctNode *node, std::vector<glm::vec3> &results) {
        if (OctNodeBoundsContains(center,node)) {
            if (node->top0 == nullptr) {
                for(int i = 0; i < node->numOfPoints; i++) {
                    results.push_back(node->points[i]);
                }
                return;
            }
            if (OctNodeBoundsContains(center,node->top0)) {
                PointsQuery(center, radius, node->top0, results);
            }
            if (OctNodeBoundsContains(center,node->top1)) {
                PointsQuery(center, radius, node->top1, results);
            }
            if (OctNodeBoundsContains(center,node->top2)) {
                PointsQuery(center, radius, node->top2, results);
            }
            if (OctNodeBoundsContains(center,node->top3)) {
                PointsQuery(center, radius, node->top3, results);
            }
            if (OctNodeBoundsContains(center,node->bottom0)) {
                PointsQuery(center, radius, node->bottom0, results);
            }
            if (OctNodeBoundsContains(center,node->bottom1)) {
                PointsQuery(center, radius, node->bottom1, results);
            }
            if (OctNodeBoundsContains(center,node->bottom2)) {
                PointsQuery(center, radius, node->bottom2, results);
            }
            if (OctNodeBoundsContains(center,node->bottom3)) {
                PointsQuery(center, radius, node->bottom3, results);
            }
        }
    }

    bool OctNodeBoundsContains(glm::vec3 &point, OctNode *node) {
        if (point.x > node->center.x - (node->size/2.0f) && point.x < node->center.x + (node->size/2.0f) &&
            point.y > node->center.y - (node->size/2.0f) && point.y < node->center.y + (node->size/2.0f) &&
            point.z > node->center.z - (node->size/2.0f) && point.z < node->center.z + (node->size/2.0f) ) {
            return true;
        }
        return false;
    }

    void AddPoint(glm::vec3 point, OctNode *node) {
        if (node->top0 == nullptr) {
            if (node->pointsCapacity > node->numOfPoints) {
                node->points[node->numOfPoints] = point;
                node->numOfPoints++;
                return;
            }
            
            // create octnodes
            node->top0 = new OctNode();
            node->top0->size = node->size/2.0f;
            node->top0->center = node->center;
            node->top0->center.x -= node->size/4.0f;
            node->top0->center.y += node->size/4.0f;
            node->top0->center.z += node->size/4.0f;

            node->top1 = new OctNode();
            node->top1->size = node->size/2.0f;
            node->top1->center = node->center;
            node->top1->center.x += node->size/4.0f;
            node->top1->center.y += node->size/4.0f;
            node->top1->center.z += node->size/4.0f;

            node->top2 = new OctNode();
            node->top2->size = node->size/2.0f;
            node->top2->center = node->center;
            node->top2->center.x += node->size/4.0f;
            node->top2->center.y += node->size/4.0f;
            node->top2->center.z -= node->size/4.0f;

            node->top3 = new OctNode();
            node->top3->size = node->size/2.0f;
            node->top3->center = node->center;
            node->top3->center.x -= node->size/4.0f;
            node->top3->center.y += node->size/4.0f;
            node->top3->center.z -= node->size/4.0f;

            node->bottom0 = new OctNode();
            node->bottom0->size = node->size/2.0f;
            node->bottom0->center = node->center;
            node->bottom0->center.x -= node->size/4.0f;
            node->bottom0->center.y -= node->size/4.0f;
            node->bottom0->center.z += node->size/4.0f;

            node->bottom1 = new OctNode();
            node->bottom1->size = node->size/2.0f;
            node->bottom1->center = node->center;
            node->bottom1->center.x += node->size/4.0f;
            node->bottom1->center.y -= node->size/4.0f;
            node->bottom1->center.z += node->size/4.0f;

            node->bottom2 = new OctNode();
            node->bottom2->size = node->size/2.0f;
            node->bottom2->center = node->center;
            node->bottom2->center.x += node->size/4.0f;
            node->bottom2->center.y -= node->size/4.0f;
            node->bottom2->center.z -= node->size/4.0f;

            node->bottom3 = new OctNode();
            node->bottom3->size = node->size/2.0f;
            node->bottom3->center = node->center;
            node->bottom3->center.x -= node->size/4.0f;
            node->bottom3->center.y -= node->size/4.0f;
            node->bottom3->center.z -= node->size/4.0f;

            // load points into child node
            for(int i = 0; i < node->numOfPoints; i++) {
                if (OctNodeBoundsContains(point, node->top0)) {
                    AddPoint(point, node->top0);
                } else if (OctNodeBoundsContains(point, node->top1)) {
                    AddPoint(point, node->top1);
                } else if (OctNodeBoundsContains(point, node->top2)) {
                    AddPoint(point, node->top2);
                } else if (OctNodeBoundsContains(point, node->top3)) {
                    AddPoint(point, node->top3);
                } else if (OctNodeBoundsContains(point, node->bottom0)) {
                    AddPoint(point, node->bottom0);
                } else if (OctNodeBoundsContains(point, node->bottom1)) {
                    AddPoint(point, node->bottom1);
                } else if (OctNodeBoundsContains(point, node->bottom2)) {
                    AddPoint(point, node->bottom2);
                } else if (OctNodeBoundsContains(point, node->bottom3)) {
                    AddPoint(point, node->bottom3);
                }
            }
        }

        if (OctNodeBoundsContains(point, node->top0)) {
            AddPoint(point, node->top0);
            return;
        } else if (OctNodeBoundsContains(point, node->top1)) {
            AddPoint(point, node->top1);
            return;
        } else if (OctNodeBoundsContains(point, node->top2)) {
            AddPoint(point, node->top2);
            return;
        } else if (OctNodeBoundsContains(point, node->top3)) {
            AddPoint(point, node->top3);
            return;
        } else if (OctNodeBoundsContains(point, node->bottom0)) {
            AddPoint(point, node->bottom0);
            return;
        } else if (OctNodeBoundsContains(point, node->bottom1)) {
            AddPoint(point, node->bottom1);
            return;
        } else if (OctNodeBoundsContains(point, node->bottom2)) {
            AddPoint(point, node->bottom2);
            return;
        } else if (OctNodeBoundsContains(point, node->bottom3)) {
            AddPoint(point, node->bottom3);
            return;
        }
    }

public:
    OctTree(glm::vec3 center, float size) {
        OctNode *node = new OctNode();
        node->size = size;
        node->center = center;

        m_root = node;
    }

    ~OctTree() {
        delete m_root;
    }

    void AddPoint(glm::vec3 point) {
        AddPoint(point,m_root);
    }

    bool PointsQuery(glm::vec3 center, float radius, std::vector<glm::vec3> &results) {
        if (OctNodeBoundsContains(center,m_root)) {
            //Canis::Log("true");
            if (m_root->top0 == nullptr) {
                for(int i = 0; i < m_root->numOfPoints; i++) {
                    results.push_back(m_root->points[i]);
                }
            }
            else
            {
                if (OctNodeBoundsContains(center,m_root->top0)) {
                    PointsQuery(center, radius, m_root->top0, results);
                }
                if (OctNodeBoundsContains(center,m_root->top1)) {
                    PointsQuery(center, radius, m_root->top1, results);
                }
                if (OctNodeBoundsContains(center,m_root->top2)) {
                    PointsQuery(center, radius, m_root->top2, results);
                }
                if (OctNodeBoundsContains(center,m_root->top3)) {
                    PointsQuery(center, radius, m_root->top3, results);
                }
                if (OctNodeBoundsContains(center,m_root->bottom0)) {
                    PointsQuery(center, radius, m_root->bottom0, results);
                }
                if (OctNodeBoundsContains(center,m_root->bottom1)) {
                    PointsQuery(center, radius, m_root->bottom1, results);
                }
                if (OctNodeBoundsContains(center,m_root->bottom2)) {
                    PointsQuery(center, radius, m_root->bottom2, results);
                }
                if (OctNodeBoundsContains(center,m_root->bottom3)) {
                    PointsQuery(center, radius, m_root->bottom3, results);
                }
            }
            return true;
        }

        return false;
    }
};
} // end of Canis namespace