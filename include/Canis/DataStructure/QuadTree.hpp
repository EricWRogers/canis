#pragma once

#include <glm/glm.hpp>

#include <Canis/Debug.hpp>
#include <Canis/Entity.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace Canis
{
    struct QuadPoint
    {
        glm::vec2 position = glm::vec2(0.0f);
        Entity* entity = nullptr;
        UUID entityUUID = UUID(0);
        glm::vec2 velocity = glm::vec2(0.0f);
    };

    struct QuadNode
    {
        static constexpr std::size_t InvalidIndex = std::numeric_limits<std::size_t>::max();

        glm::vec2 center = glm::vec2(0.0f);
        float size = 0.0f;
        std::array<std::size_t, 4> children = {
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex};
        std::vector<QuadPoint> points = {};

        bool IsLeaf() const
        {
            return children[0] == InvalidIndex;
        }
    };

    class QuadTree
    {
    public:
        explicit QuadTree(glm::vec2 _center, float _size, std::size_t _leafCapacity = 16u, std::size_t _reserveNodes = 1024u)
            : m_leafCapacity(std::max<std::size_t>(_leafCapacity, 1u))
        {
            m_nodes.reserve(std::max<std::size_t>(_reserveNodes, 1u));
            m_rootCenter = _center;
            m_rootSize = _size;
            Reset();
        }

        void Reset()
        {
            m_nodes.clear();
            m_totalPoints = 0;

            QuadNode root;
            root.center = m_rootCenter;
            root.size = m_rootSize;
            root.points.reserve(m_leafCapacity);
            m_nodes.push_back(root);
            m_rootIndex = 0;
        }

        void Print() const
        {
            Debug::Log("QuadTree nodes: %zu, points: %zu", m_nodes.size(), m_totalPoints);
        }

        void AddPoint(glm::vec2 _point, Entity* _entity, glm::vec2 _velocity = glm::vec2(0.0f))
        {
            QuadPoint payload;
            payload.position = _point;
            payload.entity = _entity;
            payload.entityUUID = (_entity != nullptr) ? _entity->uuid : UUID(0);
            payload.velocity = _velocity;
            InsertPoint(m_rootIndex, payload);
        }

        void AddPoint(glm::vec2 _point, UUID _entityUUID, glm::vec2 _velocity = glm::vec2(0.0f))
        {
            QuadPoint payload;
            payload.position = _point;
            payload.entity = nullptr;
            payload.entityUUID = _entityUUID;
            payload.velocity = _velocity;
            InsertPoint(m_rootIndex, payload);
        }

        bool PointsQuery(const glm::vec2 &_center, float _radius, std::vector<QuadPoint> &_results) const
        {
            if (_radius < 0.0f || m_nodes.empty())
                return false;

            if (!OverlapsCircle(m_rootIndex, _center, _radius))
                return false;

            QueryRecursive(m_rootIndex, _center, _radius * _radius, _results);
            return true;
        }

    private:
        std::vector<QuadNode> m_nodes = {};
        glm::vec2 m_rootCenter = glm::vec2(0.0f);
        float m_rootSize = 1.0f;
        std::size_t m_rootIndex = 0;
        std::size_t m_leafCapacity = 16;
        std::size_t m_totalPoints = 0;

        bool ContainsPoint(std::size_t _nodeIndex, const glm::vec2 &_point) const
        {
            const QuadNode& node = m_nodes[_nodeIndex];
            const float half = node.size * 0.5f;
            return _point.x >= (node.center.x - half) && _point.x <= (node.center.x + half) &&
                _point.y >= (node.center.y - half) && _point.y <= (node.center.y + half);
        }

        bool OverlapsCircle(std::size_t _nodeIndex, const glm::vec2 &_center, float _radius) const
        {
            const QuadNode& node = m_nodes[_nodeIndex];
            const float half = node.size * 0.5f;

            const float minX = node.center.x - half;
            const float maxX = node.center.x + half;
            const float minY = node.center.y - half;
            const float maxY = node.center.y + half;

            const float closestX = std::clamp(_center.x, minX, maxX);
            const float closestY = std::clamp(_center.y, minY, maxY);

            const float dx = _center.x - closestX;
            const float dy = _center.y - closestY;
            return (dx * dx + dy * dy) <= (_radius * _radius);
        }

        void Subdivide(std::size_t _nodeIndex)
        {
            QuadNode& node = m_nodes[_nodeIndex];
            const float childSize = node.size * 0.5f;
            const float quarter = node.size * 0.25f;

            auto makeChild = [&](float _xOffset, float _yOffset) -> std::size_t
            {
                QuadNode child;
                child.size = childSize;
                child.center = node.center + glm::vec2(_xOffset, _yOffset);
                child.points.reserve(m_leafCapacity);
                m_nodes.push_back(child);
                return m_nodes.size() - 1u;
            };

            node.children[0] = makeChild(-quarter, +quarter);
            node.children[1] = makeChild(+quarter, +quarter);
            node.children[2] = makeChild(-quarter, -quarter);
            node.children[3] = makeChild(+quarter, -quarter);

            std::vector<QuadPoint> oldPoints = std::move(node.points);
            node.points.clear();

            for (const QuadPoint& point : oldPoints)
                InsertPoint(_nodeIndex, point);
        }

        void InsertPoint(std::size_t _nodeIndex, const QuadPoint &_point)
        {
            if (!ContainsPoint(_nodeIndex, _point.position))
                return;

            QuadNode& node = m_nodes[_nodeIndex];
            if (node.IsLeaf())
            {
                if (node.points.size() < m_leafCapacity)
                {
                    node.points.push_back(_point);
                    ++m_totalPoints;
                    return;
                }

                Subdivide(_nodeIndex);
            }

            for (std::size_t childIndex : node.children)
            {
                if (childIndex == QuadNode::InvalidIndex)
                    continue;

                if (ContainsPoint(childIndex, _point.position))
                {
                    InsertPoint(childIndex, _point);
                    return;
                }
            }

            node.points.push_back(_point);
            ++m_totalPoints;
        }

        void QueryRecursive(std::size_t _nodeIndex, const glm::vec2 &_center, float _radiusSquared, std::vector<QuadPoint> &_results) const
        {
            const QuadNode& node = m_nodes[_nodeIndex];

            if (node.IsLeaf())
            {
                for (const QuadPoint& point : node.points)
                {
                    const glm::vec2 delta = point.position - _center;
                    if (glm::dot(delta, delta) <= _radiusSquared)
                        _results.push_back(point);
                }
                return;
            }

            for (std::size_t childIndex : node.children)
            {
                if (childIndex == QuadNode::InvalidIndex)
                    continue;

                const float radius = std::sqrt(_radiusSquared);
                if (OverlapsCircle(childIndex, _center, radius))
                    QueryRecursive(childIndex, _center, _radiusSquared, _results);
            }

            for (const QuadPoint& point : node.points)
            {
                const glm::vec2 delta = point.position - _center;
                if (glm::dot(delta, delta) <= _radiusSquared)
                    _results.push_back(point);
            }
        }
    };
}
