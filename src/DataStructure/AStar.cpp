#include <Canis/DataStructure/AStar.hpp>

#include <algorithm>
#include <limits>
#include <queue>

namespace Canis
{
    namespace
    {
        struct OpenPoint
        {
            float score = 0.0f;
            unsigned int id = 0u;

            bool operator<(const OpenPoint& _other) const
            {
                return score > _other.score;
            }
        };
    }

    AStar::AStar()
    {
        m_graph.emplace_back();
    }

    unsigned int AStar::AddPoint(const Vector3& _position)
    {
        if (const auto found = m_pointLookup.find(_position); found != m_pointLookup.end())
            return found->second;

        const unsigned int id = static_cast<unsigned int>(m_graph.size());
        m_graph.push_back(AStarNode{.position = _position});
        m_pointLookup.emplace(_position, id);
        return id;
    }

    unsigned int AStar::GetClosestPoint(const Vector3& _position) const
    {
        unsigned int closest = 0u;
        float closestDistanceSquared = std::numeric_limits<float>::max();

        for (unsigned int id = 1u; id < m_graph.size(); ++id)
        {
            const Vector3 difference = m_graph[id].position - _position;
            const float distanceSquared = glm::dot(difference, difference);
            if (distanceSquared < closestDistanceSquared)
            {
                closest = id;
                closestDistanceSquared = distanceSquared;
            }
        }

        return closest;
    }

    unsigned int AStar::GetPointByPosition(const Vector3& _position) const
    {
        const auto found = m_pointLookup.find(_position);
        return found == m_pointLookup.end() ? 0u : found->second;
    }

    const Vector3& AStar::GetPointPosition(unsigned int _id) const
    {
        static const Vector3 invalidPoint(0.0f);
        return _id > 0u && _id < m_graph.size() ? m_graph[_id].position : invalidPoint;
    }

    void AStar::ConnectPoints(unsigned int _idFrom, unsigned int _idTo)
    {
        if (_idFrom == 0u || _idTo == 0u || _idFrom == _idTo ||
            _idFrom >= m_graph.size() || _idTo >= m_graph.size())
            return;

        auto connectOneWay = [this](unsigned int _from, unsigned int _to)
        {
            std::vector<unsigned int>& adjacent = m_graph[_from].adjacentPointIDs;
            if (std::find(adjacent.begin(), adjacent.end(), _to) == adjacent.end())
                adjacent.push_back(_to);
        };

        connectOneWay(_idFrom, _idTo);
        connectOneWay(_idTo, _idFrom);
    }

    void AStar::RemovePoint(unsigned int _id)
    {
        if (_id == 0u || _id >= m_graph.size())
            return;

        m_pointLookup.erase(m_graph[_id].position);
        m_graph[_id].adjacentPointIDs.clear();
        for (AStarNode& node : m_graph)
        {
            node.adjacentPointIDs.erase(
                std::remove(node.adjacentPointIDs.begin(), node.adjacentPointIDs.end(), _id),
                node.adjacentPointIDs.end());
        }
    }

    bool AStar::ArePointsConnected(unsigned int _idFrom, unsigned int _idTo) const
    {
        if (_idFrom == 0u || _idFrom >= m_graph.size())
            return false;
        const std::vector<unsigned int>& adjacent = m_graph[_idFrom].adjacentPointIDs;
        return std::find(adjacent.begin(), adjacent.end(), _idTo) != adjacent.end();
    }

    bool AStar::ValidPoint(const Vector3& _position) const
    {
        return GetPointByPosition(_position) != 0u;
    }

    std::vector<Vector3> AStar::GetPath(unsigned int _idFrom, unsigned int _idTo) const
    {
        if (_idFrom == 0u || _idTo == 0u ||
            _idFrom >= m_graph.size() || _idTo >= m_graph.size())
            return {};
        if (_idFrom == _idTo)
            return {m_graph[_idTo].position};

        const float infinity = std::numeric_limits<float>::max();
        std::vector<float> distance(m_graph.size(), infinity);
        std::vector<unsigned int> predecessor(m_graph.size(), 0u);
        std::priority_queue<OpenPoint> open;

        distance[_idFrom] = 0.0f;
        open.push(OpenPoint{glm::distance(m_graph[_idFrom].position, m_graph[_idTo].position), _idFrom});

        while (!open.empty())
        {
            const unsigned int current = open.top().id;
            open.pop();
            if (current == _idTo)
                break;

            for (const unsigned int neighbor : m_graph[current].adjacentPointIDs)
            {
                if (neighbor == 0u || neighbor >= m_graph.size())
                    continue;

                const float candidate = distance[current] +
                    glm::distance(m_graph[current].position, m_graph[neighbor].position);
                if (candidate >= distance[neighbor])
                    continue;

                distance[neighbor] = candidate;
                predecessor[neighbor] = current;
                const float heuristic = glm::distance(m_graph[neighbor].position, m_graph[_idTo].position);
                open.push(OpenPoint{candidate + heuristic, neighbor});
            }
        }

        if (predecessor[_idTo] == 0u)
            return {};

        std::vector<Vector3> path;
        for (unsigned int current = _idTo; current != _idFrom; current = predecessor[current])
        {
            if (current == 0u)
                return {};
            path.push_back(m_graph[current].position);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
}
