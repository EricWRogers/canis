#pragma once

#include <glm/glm.hpp>

#include <Canis/Entity.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace Canis
{
    struct OctPoint
    {
        glm::vec3 position = glm::vec3(0.0f);
        Entity* entity = nullptr;
        UUID entityUUID = UUID(0);
        glm::vec3 velocity = glm::vec3(0.0f);
    };

    struct OctNode
    {
        static constexpr std::size_t InvalidIndex = std::numeric_limits<std::size_t>::max();

        glm::vec3 center = glm::vec3(0.0f);
        float size = 0.0f;
        std::array<std::size_t, 8> children = {
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex,
            InvalidIndex, InvalidIndex, InvalidIndex, InvalidIndex};
        std::vector<OctPoint> points = {};

        bool IsLeaf() const
        {
            return children[0] == InvalidIndex;
        }
    };

    class OctTree
    {
    public:
        explicit OctTree(glm::vec3 _center, float _size, std::size_t _leafCapacity = 16u, std::size_t _reserveNodes = 1024u)
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

            OctNode root;
            root.center = m_rootCenter;
            root.size = m_rootSize;
            root.points.reserve(m_leafCapacity);
            m_nodes.push_back(root);
            m_rootIndex = 0;
        }

        void AddPoint(glm::vec3 _point)
        {
            AddPoint(_point, nullptr, glm::vec3(0.0f));
        }

        void AddPoint(glm::vec3 _point, Entity* _entity, glm::vec3 _velocity = glm::vec3(0.0f))
        {
            OctPoint payload;
            payload.position = _point;
            payload.entity = _entity;
            payload.entityUUID = (_entity != nullptr) ? _entity->uuid : UUID(0);
            payload.velocity = _velocity;
            InsertPoint(m_rootIndex, payload);
        }

        void AddPoint(glm::vec3 _point, UUID _entityUUID, glm::vec3 _velocity = glm::vec3(0.0f))
        {
            OctPoint payload;
            payload.position = _point;
            payload.entity = nullptr;
            payload.entityUUID = _entityUUID;
            payload.velocity = _velocity;
            InsertPoint(m_rootIndex, payload);
        }

        bool PointsQuery(glm::vec3 _center, float _radius, std::vector<OctPoint> &_results) const
        {
            if (_radius < 0.0f || m_nodes.empty())
                return false;

            if (!OverlapsSphere(m_rootIndex, _center, _radius))
                return false;

            QueryRecursive(m_rootIndex, _center, _radius * _radius, _results);
            return true;
        }

    private:
        std::vector<OctNode> m_nodes = {};
        glm::vec3 m_rootCenter = glm::vec3(0.0f);
        float m_rootSize = 1.0f;
        std::size_t m_rootIndex = 0;
        std::size_t m_leafCapacity = 16;
        std::size_t m_totalPoints = 0;

        bool ContainsPoint(std::size_t _nodeIndex, const glm::vec3 &_point) const
        {
            const OctNode& node = m_nodes[_nodeIndex];
            const float half = node.size * 0.5f;
            return _point.x >= (node.center.x - half) && _point.x <= (node.center.x + half) &&
                _point.y >= (node.center.y - half) && _point.y <= (node.center.y + half) &&
                _point.z >= (node.center.z - half) && _point.z <= (node.center.z + half);
        }

        bool OverlapsSphere(std::size_t _nodeIndex, const glm::vec3 &_center, float _radius) const
        {
            const OctNode& node = m_nodes[_nodeIndex];
            const float half = node.size * 0.5f;

            const glm::vec3 minCorner = node.center - glm::vec3(half);
            const glm::vec3 maxCorner = node.center + glm::vec3(half);

            const glm::vec3 closest(
                std::clamp(_center.x, minCorner.x, maxCorner.x),
                std::clamp(_center.y, minCorner.y, maxCorner.y),
                std::clamp(_center.z, minCorner.z, maxCorner.z));

            const glm::vec3 delta = _center - closest;
            return glm::dot(delta, delta) <= (_radius * _radius);
        }

        void Subdivide(std::size_t _nodeIndex)
        {
            OctNode& node = m_nodes[_nodeIndex];
            const float childSize = node.size * 0.5f;
            const float quarter = node.size * 0.25f;

            auto makeChild = [&](float _xOffset, float _yOffset, float _zOffset) -> std::size_t
            {
                OctNode child;
                child.size = childSize;
                child.center = node.center + glm::vec3(_xOffset, _yOffset, _zOffset);
                child.points.reserve(m_leafCapacity);
                m_nodes.push_back(child);
                return m_nodes.size() - 1u;
            };

            node.children[0] = makeChild(-quarter, +quarter, +quarter);
            node.children[1] = makeChild(+quarter, +quarter, +quarter);
            node.children[2] = makeChild(+quarter, +quarter, -quarter);
            node.children[3] = makeChild(-quarter, +quarter, -quarter);
            node.children[4] = makeChild(-quarter, -quarter, +quarter);
            node.children[5] = makeChild(+quarter, -quarter, +quarter);
            node.children[6] = makeChild(+quarter, -quarter, -quarter);
            node.children[7] = makeChild(-quarter, -quarter, -quarter);

            std::vector<OctPoint> oldPoints = std::move(node.points);
            node.points.clear();

            for (const OctPoint& point : oldPoints)
                InsertPoint(_nodeIndex, point);
        }

        void InsertPoint(std::size_t _nodeIndex, const OctPoint &_point)
        {
            if (!ContainsPoint(_nodeIndex, _point.position))
                return;

            OctNode& node = m_nodes[_nodeIndex];
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
                if (childIndex == OctNode::InvalidIndex)
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

        void QueryRecursive(std::size_t _nodeIndex, const glm::vec3 &_center, float _radiusSquared, std::vector<OctPoint> &_results) const
        {
            const OctNode& node = m_nodes[_nodeIndex];

            if (node.IsLeaf())
            {
                for (const OctPoint& point : node.points)
                {
                    const glm::vec3 delta = point.position - _center;
                    if (glm::dot(delta, delta) <= _radiusSquared)
                        _results.push_back(point);
                }
                return;
            }

            const float radius = std::sqrt(_radiusSquared);
            for (std::size_t childIndex : node.children)
            {
                if (childIndex == OctNode::InvalidIndex)
                    continue;

                if (OverlapsSphere(childIndex, _center, radius))
                    QueryRecursive(childIndex, _center, _radiusSquared, _results);
            }

            for (const OctPoint& point : node.points)
            {
                const glm::vec3 delta = point.position - _center;
                if (glm::dot(delta, delta) <= _radiusSquared)
                    _results.push_back(point);
            }
        }
    };
}
