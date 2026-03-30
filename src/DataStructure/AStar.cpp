#include <Canis/DataStructure/AStar.hpp>

namespace Canis
{
    AStar::AStar()
    {
        AStarNode node = {};
        
        graph.reserve(1000);

        graph.push_back(node);
    }

    unsigned int AStar::AddPoint(Vector3 _position)
    {
        AStarNode node = {};

        node.position = _position;

        graph.push_back(node);
        m_umap[_position] = graph.size() - 1;

        return graph.size() - 1;
    }

    unsigned int AStar::GetClosestPoint(Vector3 _position)
    {
        if (graph.size() == 0)
            Debug::FatalError("AStar::GetClosetPoint was called before a point was added to the graph.");
        
        unsigned int id = 0u;
        float minDistance = f32_max; // set float to its largest value
        float distance = 0.0f;

        for(int i = 1; i < graph.size(); i++)
        {
            distance = glm::distance(_position, graph[i].position);

            if (minDistance > distance)
            {
                id = i;
                minDistance = distance;
            }
        }

        return id;
    }

    unsigned int AStar::GetPointByPosition(Vector3 _position)
    {
        
        if (m_umap.contains(_position))
            return m_umap[_position];
        else
            return 0;
    }

    void AStar::ConnectPoints(unsigned int idFrom, unsigned int idTo)
    {
        if (graph.size() <= idFrom)
            Debug::FatalError("AStar::ConnectPoints idFrom has not been added to the graph.");

        if (graph.size() <= idTo)
            Debug::FatalError("AStar::ConnectPoints idTo has not been added to the graph.");
        
        if (idTo == idFrom)
            return;
        
        if (!VecHas(graph[idFrom].adjacentPointIDs, idTo))
            graph[idFrom].adjacentPointIDs.push_back(idTo);
        if (!VecHas(graph[idTo].adjacentPointIDs, idFrom))
            graph[idTo].adjacentPointIDs.push_back(idFrom);
    }

    void AStar::RemovePoint(unsigned int id) {
        if (graph.size() <= id)
            Debug::FatalError("AStar::RemovePoint id has not been added to the graph.");
        for (int i = 0; i < graph[id].adjacentPointIDs.size(); i++) { //remove from adjacent points
            for (int j = 0; j < graph[i].adjacentPointIDs.size(); j++) { //iterate over adjacent points of neighbor
                if (graph[i].adjacentPointIDs[j] == id) { 
                    graph[i].adjacentPointIDs.erase(graph[i].adjacentPointIDs.begin()+j);
                    continue;
                }
            }
        }
        graph.erase(graph.begin()+id);
    }

    bool AStar::ArePointsConnected(unsigned int idFrom, unsigned int idTo)
    {
        if (graph.size() <= idFrom)
            Debug::FatalError("AStar::ArePointsConnected idFrom has not been added to the graph.");

        if (graph.size() <= idTo)
            Debug::FatalError("AStar::ArePointsConnected idTo has not been added to the graph.");
        
        if (graph[idFrom].adjacentPointIDs.size() )

        for(int i = 0; i < graph[idFrom].adjacentPointIDs.size(); i++)
        {
            if (graph[idFrom].adjacentPointIDs[i] == idTo)
            {
                return true;
            }
        }

        return false;
    }

    bool AStar::ValidPoint(Vector3 position)
    {
        for (int i = 1; i < graph.size();i++)
        {
            if (graph[i].position == position)
                return true;
        }

        return false;
    }

    std::vector<Vector3> AStar::GetPath(unsigned int idFrom, unsigned int idTo)
    {
        if (graph.size() <= idFrom)
            Debug::FatalError("AStar::GetPath idFrom has not been added to the graph.");

        if (graph.size() <= idTo)
            Debug::FatalError("AStar::GetPath idTo has not been added to the graph.");

        // find better solution for reseting the graph
        for(AStarNode& node : graph) {
            node.f = 0.0f;
            node.g = 0.0f;
            node.h = 0.0f;
            node.predecessorID = 0;
        }
        
        std::vector<unsigned int> searchingSet;
        std::vector<unsigned int> hasSearchedSet;
        unsigned int graphNodeIndex;

        searchingSet.push_back(idFrom);

        while (searchingSet.size() > 0)
        {
            unsigned int lowestPath = 0;

            for (unsigned int i = 0; i < searchingSet.size(); i++)
            {
                if (graph[searchingSet[lowestPath]].f > graph[searchingSet[i]].f)
                {
                    lowestPath = i;
                }
            }

            AStarNode *node = &graph[searchingSet[lowestPath]];
            graphNodeIndex = searchingSet[lowestPath];

            if (idTo == searchingSet[lowestPath])
            {
                return BuildPath(node);
            }

            hasSearchedSet.push_back(searchingSet[lowestPath]);

            searchingSet.erase(searchingSet.begin() + lowestPath);

            std::vector<unsigned int> neighborIDs = node->adjacentPointIDs;

            for(unsigned int i = 0; i < neighborIDs.size(); i++)
            {
                AStarNode *neighborNode = &graph[neighborIDs[i]];                

                if (!VecHas(hasSearchedSet, neighborIDs[i]))
                {
                    if (graphNodeIndex == neighborIDs[i])
                    {
                        continue;
                    }

                    float currentG = node->g + std::abs(glm::distance(node->position, neighborNode->position));

                    bool isNewPath = false;

                    if (VecHas(searchingSet, neighborIDs[i]))
                    {
                        if (currentG < neighborNode->g)
                        {
                            isNewPath = true;
                            neighborNode->g = currentG;
                        }
                    }
                    else
                    {
                        isNewPath = true;
                        neighborNode->g = currentG;
                        searchingSet.push_back(neighborIDs[i]);
                    }

                    if (isNewPath)
                    {
                        neighborNode->h = std::abs(glm::distance(neighborNode->position, graph[idTo].position));
                        neighborNode->f = neighborNode->g + neighborNode->h;
                        neighborNode->predecessorID = graphNodeIndex;
                    }
                }
            }
        }

        return std::vector<Vector3> {};
    }

    std::vector<Vector3> AStar::BuildPath(AStarNode *node)
    {
        std::vector<Vector3> path;
        AStarNode *currentNode = node;

        while (currentNode->predecessorID != 0)
        {
            path.insert(path.begin(), currentNode->position);
            currentNode = &graph[currentNode->predecessorID];
        }

        return path;
    }

    bool AStar::VecHas(std::vector<unsigned int> &toCheck, unsigned int value)
    {
        for (unsigned int n = 0; n < toCheck.size(); n++)
        {
            if (toCheck[n] == value)
            {
                return true;
            }
        }

        return false;
    }
} // end of Canis namespace
