#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace Canis
{
struct OctNode
{
    OctNode *m_top0 = nullptr;
    OctNode *m_top1 = nullptr;
    OctNode *m_top2 = nullptr;
    OctNode *m_top3 = nullptr;
    OctNode *m_bottom0 = nullptr;
    OctNode *m_bottom1 = nullptr;
    OctNode *m_bottom2 = nullptr;
    OctNode *m_bottom3 = nullptr;
    glm::vec3 m_center = glm::vec3(0.0f);
    float m_size;
    glm::vec3 m_points[10] = {};
    int m_pointsCapacity = 10;
    int m_numOfPoints = 0;

    ~OctNode() {
        delete m_top0;
        delete m_top1;
        delete m_top2;
        delete m_top3;
        delete m_bottom0;
        delete m_bottom1;
        delete m_bottom2;
        delete m_bottom3;
    }
};

class OctTree
{
private:
    OctNode *m_root;

    void PointsQuery(glm::vec3 center, float radius, OctNode *node, std::vector<glm::vec3> &results) {
        if (OctNodeBoundsContains(center,node)) {
            if (node->m_top0 == nullptr) {
                for(int i = 0; i < node->m_numOfPoints; i++) {
                    results.push_back(node->m_points[i]);
                }
                return;
            }
            if (OctNodeBoundsContains(center,node->m_top0)) {
                PointsQuery(center, radius, node->m_top0, results);
            }
            if (OctNodeBoundsContains(center,node->m_top1)) {
                PointsQuery(center, radius, node->m_top1, results);
            }
            if (OctNodeBoundsContains(center,node->m_top2)) {
                PointsQuery(center, radius, node->m_top2, results);
            }
            if (OctNodeBoundsContains(center,node->m_top3)) {
                PointsQuery(center, radius, node->m_top3, results);
            }
            if (OctNodeBoundsContains(center,node->m_bottom0)) {
                PointsQuery(center, radius, node->m_bottom0, results);
            }
            if (OctNodeBoundsContains(center,node->m_bottom1)) {
                PointsQuery(center, radius, node->m_bottom1, results);
            }
            if (OctNodeBoundsContains(center,node->m_bottom2)) {
                PointsQuery(center, radius, node->m_bottom2, results);
            }
            if (OctNodeBoundsContains(center,node->m_bottom3)) {
                PointsQuery(center, radius, node->m_bottom3, results);
            }
        }
    }

    bool OctNodeBoundsContains(glm::vec3 &point, OctNode *node) {
        if (point.x > node->m_center.x - (node->m_size/2.0f) && point.x < node->m_center.x + (node->m_size/2.0f) &&
            point.y > node->m_center.y - (node->m_size/2.0f) && point.y < node->m_center.y + (node->m_size/2.0f) &&
            point.z > node->m_center.z - (node->m_size/2.0f) && point.z < node->m_center.z + (node->m_size/2.0f) ) {
            return true;
        }
        return false;
    }

    void AddPoint(glm::vec3 point, OctNode *node) {
        if (node->m_top0 == nullptr) {
            if (node->m_pointsCapacity > node->m_numOfPoints) {
                node->m_points[node->m_numOfPoints] = point;
                node->m_numOfPoints++;
                return;
            }
            
            // create octnodes
            node->m_top0 = new OctNode();
            node->m_top0->m_size = node->m_size/2.0f;
            node->m_top0->m_center = node->m_center;
            node->m_top0->m_center.x -= node->m_size/4.0f;
            node->m_top0->m_center.y += node->m_size/4.0f;
            node->m_top0->m_center.z += node->m_size/4.0f;

            node->m_top1 = new OctNode();
            node->m_top1->m_size = node->m_size/2.0f;
            node->m_top1->m_center = node->m_center;
            node->m_top1->m_center.x += node->m_size/4.0f;
            node->m_top1->m_center.y += node->m_size/4.0f;
            node->m_top1->m_center.z += node->m_size/4.0f;

            node->m_top2 = new OctNode();
            node->m_top2->m_size = node->m_size/2.0f;
            node->m_top2->m_center = node->m_center;
            node->m_top2->m_center.x += node->m_size/4.0f;
            node->m_top2->m_center.y += node->m_size/4.0f;
            node->m_top2->m_center.z -= node->m_size/4.0f;

            node->m_top3 = new OctNode();
            node->m_top3->m_size = node->m_size/2.0f;
            node->m_top3->m_center = node->m_center;
            node->m_top3->m_center.x -= node->m_size/4.0f;
            node->m_top3->m_center.y += node->m_size/4.0f;
            node->m_top3->m_center.z -= node->m_size/4.0f;

            node->m_bottom0 = new OctNode();
            node->m_bottom0->m_size = node->m_size/2.0f;
            node->m_bottom0->m_center = node->m_center;
            node->m_bottom0->m_center.x -= node->m_size/4.0f;
            node->m_bottom0->m_center.y -= node->m_size/4.0f;
            node->m_bottom0->m_center.z += node->m_size/4.0f;

            node->m_bottom1 = new OctNode();
            node->m_bottom1->m_size = node->m_size/2.0f;
            node->m_bottom1->m_center = node->m_center;
            node->m_bottom1->m_center.x += node->m_size/4.0f;
            node->m_bottom1->m_center.y -= node->m_size/4.0f;
            node->m_bottom1->m_center.z += node->m_size/4.0f;

            node->m_bottom2 = new OctNode();
            node->m_bottom2->m_size = node->m_size/2.0f;
            node->m_bottom2->m_center = node->m_center;
            node->m_bottom2->m_center.x += node->m_size/4.0f;
            node->m_bottom2->m_center.y -= node->m_size/4.0f;
            node->m_bottom2->m_center.z -= node->m_size/4.0f;

            node->m_bottom3 = new OctNode();
            node->m_bottom3->m_size = node->m_size/2.0f;
            node->m_bottom3->m_center = node->m_center;
            node->m_bottom3->m_center.x -= node->m_size/4.0f;
            node->m_bottom3->m_center.y -= node->m_size/4.0f;
            node->m_bottom3->m_center.z -= node->m_size/4.0f;

            // load points into child node
            for(int i = 0; i < node->m_numOfPoints; i++) {
                if (OctNodeBoundsContains(point, node->m_top0)) {
                    AddPoint(point, node->m_top0);
                } else if (OctNodeBoundsContains(point, node->m_top1)) {
                    AddPoint(point, node->m_top1);
                } else if (OctNodeBoundsContains(point, node->m_top2)) {
                    AddPoint(point, node->m_top2);
                } else if (OctNodeBoundsContains(point, node->m_top3)) {
                    AddPoint(point, node->m_top3);
                } else if (OctNodeBoundsContains(point, node->m_bottom0)) {
                    AddPoint(point, node->m_bottom0);
                } else if (OctNodeBoundsContains(point, node->m_bottom1)) {
                    AddPoint(point, node->m_bottom1);
                } else if (OctNodeBoundsContains(point, node->m_bottom2)) {
                    AddPoint(point, node->m_bottom2);
                } else if (OctNodeBoundsContains(point, node->m_bottom3)) {
                    AddPoint(point, node->m_bottom3);
                }
            }
        }

        if (OctNodeBoundsContains(point, node->m_top0)) {
            AddPoint(point, node->m_top0);
            return;
        } else if (OctNodeBoundsContains(point, node->m_top1)) {
            AddPoint(point, node->m_top1);
            return;
        } else if (OctNodeBoundsContains(point, node->m_top2)) {
            AddPoint(point, node->m_top2);
            return;
        } else if (OctNodeBoundsContains(point, node->m_top3)) {
            AddPoint(point, node->m_top3);
            return;
        } else if (OctNodeBoundsContains(point, node->m_bottom0)) {
            AddPoint(point, node->m_bottom0);
            return;
        } else if (OctNodeBoundsContains(point, node->m_bottom1)) {
            AddPoint(point, node->m_bottom1);
            return;
        } else if (OctNodeBoundsContains(point, node->m_bottom2)) {
            AddPoint(point, node->m_bottom2);
            return;
        } else if (OctNodeBoundsContains(point, node->m_bottom3)) {
            AddPoint(point, node->m_bottom3);
            return;
        }
    }

public:
    OctTree(glm::vec3 center, float size) {
        OctNode *node = new OctNode();
        node->m_size = size;
        node->m_center = center;

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
            if (m_root->m_top0 == nullptr) {
                for(int i = 0; i < m_root->m_numOfPoints; i++) {
                    results.push_back(m_root->m_points[i]);
                }
            }
            else
            {
                if (OctNodeBoundsContains(center,m_root->m_top0)) {
                    PointsQuery(center, radius, m_root->m_top0, results);
                }
                if (OctNodeBoundsContains(center,m_root->m_top1)) {
                    PointsQuery(center, radius, m_root->m_top1, results);
                }
                if (OctNodeBoundsContains(center,m_root->m_top2)) {
                    PointsQuery(center, radius, m_root->m_top2, results);
                }
                if (OctNodeBoundsContains(center,m_root->m_top3)) {
                    PointsQuery(center, radius, m_root->m_top3, results);
                }
                if (OctNodeBoundsContains(center,m_root->m_bottom0)) {
                    PointsQuery(center, radius, m_root->m_bottom0, results);
                }
                if (OctNodeBoundsContains(center,m_root->m_bottom1)) {
                    PointsQuery(center, radius, m_root->m_bottom1, results);
                }
                if (OctNodeBoundsContains(center,m_root->m_bottom2)) {
                    PointsQuery(center, radius, m_root->m_bottom2, results);
                }
                if (OctNodeBoundsContains(center,m_root->m_bottom3)) {
                    PointsQuery(center, radius, m_root->m_bottom3, results);
                }
            }
            return true;
        }

        return false;
    }
};
} // end of Canis namespace