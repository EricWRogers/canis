#pragma once

#include <unordered_map>
#include <vector>

#include <Canis/Math.hpp>

namespace Canis
{
    struct AStarNode
    {
        Vector3 position = Vector3(0.0f);
        std::vector<unsigned int> adjacentPointIDs = {};
    };

    // A small, engine-level navigation graph. Point id 0 is reserved as an
    // invalid id so callers can safely use zero as "not found".
    class AStar
    {
    public:
        AStar();

        unsigned int AddPoint(const Vector3& _position);
        unsigned int GetClosestPoint(const Vector3& _position) const;
        unsigned int GetPointByPosition(const Vector3& _position) const;
        const Vector3& GetPointPosition(unsigned int _id) const;

        void ConnectPoints(unsigned int _idFrom, unsigned int _idTo);
        void RemovePoint(unsigned int _id);

        bool ArePointsConnected(unsigned int _idFrom, unsigned int _idTo) const;
        bool ValidPoint(const Vector3& _position) const;
        std::vector<Vector3> GetPath(unsigned int _idFrom, unsigned int _idTo) const;
        std::size_t GetPointCount() const { return m_graph.size() - 1u; }

    private:
        std::vector<AStarNode> m_graph = {};
        std::unordered_map<Vector3, unsigned int> m_pointLookup = {};
    };
}
