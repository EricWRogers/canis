#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/External/entt.hpp>
#include <Canis/Debug.hpp>

namespace Canis
{
namespace QuadTree
{
    struct QuadPoint
    {
        glm::vec2 position;
        entt::entity entity;
        glm::vec2 velocity;
    };

    struct QuadNode
    {
        unsigned short parent = 0;
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

    struct QuadTreeData
    {
        std::vector<QuadNode> quadNodes = std::vector<QuadNode>(30000);
        QuadNode quadNodeDefault = {};
        unsigned int rootIndex = 0;
        unsigned int totalPoints = 0;
        unsigned int totalNodes = 0;
        unsigned int currentNumberOfNodes = 0;
    };

    bool QuadtreeOverlap(QuadTreeData &_quadTreeData, const glm::vec2 &point, const float &radius, unsigned short nodeIndex)
    {
        if (glm::distance(point, _quadTreeData.quadNodes[nodeIndex].center) < radius + (_quadTreeData.quadNodes[nodeIndex].size * 1.4f))
            return true;

        return false;
    }

    void PointsQuery(QuadTreeData &_quadTreeData, const glm::vec2 &center, float radius, unsigned short nodeIndex, std::vector<QuadPoint> &results)
    {
        if (_quadTreeData.quadNodes[nodeIndex].top0 == 0)
        {
            for (int i = 0; i < _quadTreeData.quadNodes[nodeIndex].numOfPoints; i++)
            {
                results.push_back(_quadTreeData.quadNodes[nodeIndex].points[i]);
            }
            return;
        }
        if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].top0))
        {
            PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].top0, results);
        }
        if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].top1))
        {
            PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].top1, results);
        }
        if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].bottom0))
        {
            PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].bottom0, results);
        }
        if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].bottom1))
        {
            PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[nodeIndex].bottom1, results);
        }
    }

    bool OctNodeBoundsContains(QuadTreeData &_quadTreeData, const glm::vec2 &point, unsigned short nodeIndex)
    {
        float size = _quadTreeData.quadNodes[nodeIndex].size / 2.0f;
        if (point.x > _quadTreeData.quadNodes[nodeIndex].center.x - size && point.x < _quadTreeData.quadNodes[nodeIndex].center.x + size &&
            point.y > _quadTreeData.quadNodes[nodeIndex].center.y - size && point.y < _quadTreeData.quadNodes[nodeIndex].center.y + size)
            return true;

        return false;
    }



    QuadTreeData Init(glm::vec2 center, float size)
    {
        QuadTreeData qtd;
        qtd.rootIndex = 0;
        qtd.totalNodes = 0;
        qtd.quadNodes[0].size = size;
        qtd.quadNodes[0].center = center;
        return qtd;
    }

    void Init(QuadTreeData &_quadTreeData, glm::vec2 center, float size)
    {
        _quadTreeData.rootIndex = 0;
        _quadTreeData.totalNodes = 0;
        _quadTreeData.quadNodes[0].size = size;
        _quadTreeData.quadNodes[0].center = center;
    }

    void Reset(QuadTreeData &_quadTreeData)
    {
        _quadTreeData.currentNumberOfNodes = 1;
        float size = _quadTreeData.quadNodes[_quadTreeData.rootIndex].size;
        glm::vec2 center = _quadTreeData.quadNodes[_quadTreeData.rootIndex].center;
        _quadTreeData.rootIndex = 0;
        _quadTreeData.quadNodes[_quadTreeData.rootIndex].size = size;
        _quadTreeData.quadNodes[_quadTreeData.rootIndex].center = center;
        _quadTreeData.quadNodes[_quadTreeData.rootIndex].top0 = 0;
        _quadTreeData.quadNodes[_quadTreeData.rootIndex].top1 = 0;
        _quadTreeData.quadNodes[_quadTreeData.rootIndex].bottom0 = 0;
        _quadTreeData.quadNodes[_quadTreeData.rootIndex].bottom1 = 0;
        _quadTreeData.quadNodes[_quadTreeData.rootIndex].numOfPoints = 0;
    }

    void Print(QuadTreeData &_quadTreeData)
    {
        Canis::Log("quad nodes : " + std::to_string(_quadTreeData.quadNodes.size()));
    }

    void AddPoint(QuadTreeData &_quadTreeData, glm::vec2 point, entt::entity entity, glm::vec2 vel, unsigned short nodeIndex = 0)
    {
        if (_quadTreeData.quadNodes[nodeIndex].top0 == 0)
        {
            if (_quadTreeData.quadNodes[nodeIndex].pointsCapacity > _quadTreeData.quadNodes[nodeIndex].numOfPoints)
            {
                _quadTreeData.quadNodes[nodeIndex].points[_quadTreeData.quadNodes[nodeIndex].numOfPoints].position = point;
                _quadTreeData.quadNodes[nodeIndex].points[_quadTreeData.quadNodes[nodeIndex].numOfPoints].entity = entity;
                _quadTreeData.quadNodes[nodeIndex].points[_quadTreeData.quadNodes[nodeIndex].numOfPoints].velocity = vel;
                _quadTreeData.quadNodes[nodeIndex].numOfPoints++;
                _quadTreeData.totalPoints++;
                return;
            }

            // create octnodes
            float size = _quadTreeData.quadNodes[nodeIndex].size / 2.0f;
            float halfSize = size / 2.0f;

            _quadTreeData.currentNumberOfNodes++;
            if (_quadTreeData.quadNodes.size() <= _quadTreeData.currentNumberOfNodes)
            {
                _quadTreeData.quadNodes.push_back(_quadTreeData.quadNodeDefault);
            }
            _quadTreeData.quadNodes[nodeIndex].top0 = _quadTreeData.currentNumberOfNodes - 1;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].size = size;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].center = _quadTreeData.quadNodes[nodeIndex].center;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].center.x -= halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].center.y += halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].top0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].top1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].bottom0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].bottom1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top0].numOfPoints = 0;

            _quadTreeData.currentNumberOfNodes++;
            if (_quadTreeData.quadNodes.size() <= _quadTreeData.currentNumberOfNodes)
            {
                _quadTreeData.quadNodes.push_back(_quadTreeData.quadNodeDefault);
            }
            _quadTreeData.quadNodes[nodeIndex].top1 = _quadTreeData.currentNumberOfNodes - 1;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].size = size;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].center = _quadTreeData.quadNodes[nodeIndex].center;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].center.x += halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].center.y += halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].top0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].top1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].bottom0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].bottom1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].top1].numOfPoints = 0;

            _quadTreeData.currentNumberOfNodes++;
            if (_quadTreeData.quadNodes.size() <= _quadTreeData.currentNumberOfNodes)
            {
                _quadTreeData.quadNodes.push_back(_quadTreeData.quadNodeDefault);
            }
            _quadTreeData.quadNodes[nodeIndex].bottom0 = _quadTreeData.currentNumberOfNodes - 1;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].size = size;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].center = _quadTreeData.quadNodes[nodeIndex].center;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].center.x -= halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].center.y -= halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].top0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].top1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].bottom0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].bottom1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom0].numOfPoints = 0;

            _quadTreeData.currentNumberOfNodes++;
            if (_quadTreeData.quadNodes.size() <= _quadTreeData.currentNumberOfNodes)
            {
                _quadTreeData.quadNodes.push_back(_quadTreeData.quadNodeDefault);
            }
            _quadTreeData.quadNodes[nodeIndex].bottom1 = _quadTreeData.currentNumberOfNodes - 1;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].size = size;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].center = _quadTreeData.quadNodes[nodeIndex].center;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].center.x += halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].center.y -= halfSize;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].top0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].top1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].bottom0 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].bottom1 = 0;
            _quadTreeData.quadNodes[_quadTreeData.quadNodes[nodeIndex].bottom1].numOfPoints = 0;

            // load points into child node
            for (int i = 0; i < _quadTreeData.quadNodes[nodeIndex].numOfPoints; i++)
            {
                if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].top0))
                {
                    AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].top0);
                }
                else if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].top1))
                {
                    AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].top1);
                }
                else if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].bottom0))
                {
                    AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].bottom0);
                }
                else if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].bottom1))
                {
                    AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].bottom1);
                }
            }
        }

        if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].top0))
        {
            AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].top0);
            return;
        }
        else if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].top1))
        {
            AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].top1);
            return;
        }
        else if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].bottom0))
        {
            AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].bottom0);
            return;
        }
        else if (OctNodeBoundsContains(_quadTreeData, point, _quadTreeData.quadNodes[nodeIndex].bottom1))
        {
            AddPoint(_quadTreeData, point, entity, vel, _quadTreeData.quadNodes[nodeIndex].bottom1);
            return;
        }
    }

    bool PointsQuery(QuadTreeData &_quadTreeData, const glm::vec2 &center, float radius, std::vector<QuadPoint> &results)
    {
        if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.rootIndex))
        {
            if (_quadTreeData.quadNodes[_quadTreeData.rootIndex].top0 == 0)
            {
                for (int i = 0; i < _quadTreeData.quadNodes[_quadTreeData.rootIndex].numOfPoints; i++)
                {
                    results.push_back(_quadTreeData.quadNodes[_quadTreeData.rootIndex].points[i]);
                }
            }
            else
            {
                if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].top0))
                {
                    PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].top0, results);
                }
                if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].top1))
                {
                    PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].top1, results);
                }
                if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].bottom0))
                {
                    PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].bottom0, results);
                }
                if (QuadtreeOverlap(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].bottom1))
                {
                    PointsQuery(_quadTreeData, center, radius, _quadTreeData.quadNodes[_quadTreeData.rootIndex].bottom1, results);
                }
            }
            return true;
        }

        return false;
    }
}

} // end of Canis namespace
